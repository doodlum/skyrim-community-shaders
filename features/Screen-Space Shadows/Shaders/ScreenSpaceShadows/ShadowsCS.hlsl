#include "Common/FrameBuffer.hlsli"
#include "Common/SharedData.hlsli"

// Interleaved Gradient Noise (Jorge Jimenez) — pseudo-random in [0, 1) per pixel.
float InterleavedGradientNoise(float2 pixelCoord)
{
	return frac(52.9829189 * frac(0.06711056 * pixelCoord.x + 0.00583715 * pixelCoord.y));
}

// Depth pyramid bindings.
//   srcDepth            (t0) — moments used for VSM samples along the ray.
//                              May be the blurred copy when BlurDepthPyramid is on,
//                              otherwise the raw prefiltered values.
//   srcDepthPrefiltered (t1) — raw prefiltered moments, never blurred.  Used for
//                              accurate per-pixel view-space position reconstruction
//                              at the receiver, so the ray origin isn't smeared.
Texture2D<float2> srcDepth : register(t0);
Texture2D<float2> srcDepthPrefiltered : register(t1);
RWTexture2D<unorm float> shadowOutput : register(u0);

// R32G32_FLOAT is not guaranteed to support linear filtering in D3D11, so the
// ray-march performs explicit bilinear filtering with Texture2D.Load.
SamplerState samplerPointClamp : register(s1);

// Sample count is a compile-time define so the loop bound is a constant
// (the compiler can fold/unroll).  C++ compiles 4 variants — one per mip —
// each with the appropriate count for its segment of the cascaded ray.
#ifndef MIP_SAMPLE_COUNT
#	define MIP_SAMPLE_COUNT 16
#endif

cbuffer SSSCB : register(b1)
{
	float2 FrameDim;
	float2 RcpTexDim;

	float2 TexDim;
	float2 DynamicRes;

	float SurfaceThickness;
	float MaxThicknessDistance;  // world units; thickness saturates to SurfaceThickness at this distance from the receiver
	float SegmentStart;          // world units along the ray where this dispatch's segment begins
	uint CurrentMip;

	float3 LightWorldDir;
	float SegmentLength;  // world units length of this dispatch's segment
};

// Reconstruct view-space position from texUV in [0, DynamicRes] space and linear depth.
float3 ScreenToViewPos(float2 texUV, float linearDepth)
{
	float2 uv01 = texUV / DynamicRes;
	float P00 = FrameBuffer::CameraProj[0][0];
	float P11 = FrameBuffer::CameraProj[1][1];
	float3 ret;
	ret.x = (2.0 / P00 * uv01.x - 1.0 / P00) * linearDepth;
	ret.y = (-2.0 / P11 * uv01.y + 1.0 / P11) * linearDepth;
	ret.z = linearDepth;
	return ret;
}

// Project a view-space position into texture UV [0, DynamicRes].
float2 ViewPosToTexUV(float3 posVS)
{
	return FrameBuffer::ViewToUV(posVS) * DynamicRes;
}

// R32G32_FLOAT is not filterable on all D3D11 hardware. Reproduce a linear
// sample explicitly so the VSM moments retain the intended smooth interpolation.
float2 SampleDepthMoments(float2 uv)
{
	uint width;
	uint height;
	srcDepth.GetDimensions(width, height);

	float2 texelPosition = uv * float2(width, height) - 0.5;
	int2 baseCoord = int2(floor(texelPosition));
	float2 blend = frac(texelPosition);
	uint2 activeDim = max(uint2(1u, 1u), uint2(FrameDim) >> CurrentMip);
	int2 maxCoord = int2(min(uint2(width, height), activeDim)) - 1;

	int2 coord00 = clamp(baseCoord, int2(0, 0), maxCoord);
	int2 coord10 = clamp(baseCoord + int2(1, 0), int2(0, 0), maxCoord);
	int2 coord01 = clamp(baseCoord + int2(0, 1), int2(0, 0), maxCoord);
	int2 coord11 = clamp(baseCoord + int2(1, 1), int2(0, 0), maxCoord);

	float2 moments00 = srcDepth.Load(int3(coord00, 0));
	float2 moments10 = srcDepth.Load(int3(coord10, 0));
	float2 moments01 = srcDepth.Load(int3(coord01, 0));
	float2 moments11 = srcDepth.Load(int3(coord11, 0));

	return lerp(lerp(moments00, moments10, blend.x),
		lerp(moments01, moments11, blend.x), blend.y);
}

static const float VSM_MIN_VARIANCE = 0.00001;

// Chebyshev upper bound on P(X >= t)
// moments.x = mean(z), moments.y = mean(z^2)
float ComputeVSM(float2 moments, float depth)
{
	float variance = max(moments.y - moments.x * moments.x, VSM_MIN_VARIANCE);
	float d = depth - moments.x;
	float pMax = variance / (variance + d * d);
	return (depth <= moments.x) ? 1.0 : pMax;
}

// Chebyshev upper bound on P(X <= t)  (lower-tail form)
// moments.x = mean(z), moments.y = mean(z^2)
float ComputeVSM_Lower(float2 moments, float depth)
{
	float variance = max(moments.y - moments.x * moments.x, VSM_MIN_VARIANCE);
	float d = moments.x - depth;
	float pMax = variance / (variance + d * d);
	return (depth >= moments.x) ? 1.0 : pMax;
}

[numthreads(8, 8, 1)] void main(uint2 dtid : SV_DispatchThreadID) {
	uint mipScale = 1u << CurrentMip;
	uint2 effectiveFrameDim = uint2(FrameDim) >> CurrentMip;

	if (any(float2(dtid) >= float2(effectiveFrameDim)))
		return;

	// Anchor each mip's ray to the upper-left full-res pixel of its block, and
	// reconstruct from mip 0 of the prefiltered pyramid.  Without this, every mip
	// would start from its own block-centre at a heavily averaged depth, so the
	// four cascade segments wouldn't line up — they'd be parallel-translated rays
	// per mip rather than one continuous ray per output pixel.
	float2 startUV = (float2(dtid) * float(mipScale) + 0.5) * RcpTexDim;
	float2 startMoment = srcDepthPrefiltered.SampleLevel(samplerPointClamp, startUV, 0);
	float startDepth = startMoment.x;

	float shadow = 1.0;

	// 0.0 depth is the sentinel written by PrefilterDepthsCS for sky / invalid pixels.
	if (startDepth > 0.0) {
		float3 startPosVS = ScreenToViewPos(startUV, startDepth);
		float3 lightViewDir = normalize(FrameBuffer::WorldToView(LightWorldDir, false));

		// March the slice of the ray assigned to this dispatch:
		//   [SegmentStart, SegmentStart + SegmentLength] world units from the receiver.
		// CPU drives the cascade — mip 0 starts at the receiver with the shortest segment;
		// each subsequent mip doubles both segment length and sample count toward the far end.
		float3 segBeginVS = startPosVS + lightViewDir * SegmentStart;
		float3 segEndVS = startPosVS + lightViewDir * (SegmentStart + SegmentLength);
		float2 segBeginUV = ViewPosToTexUV(segBeginVS);
		float2 segEndUV = ViewPosToTexUV(segEndVS);

		float jitter = InterleavedGradientNoise(float2(dtid));

		const uint numSamples = uint(MIP_SAMPLE_COUNT);

		[unroll] for (uint i = 1; i <= numSamples; i++)
		{
			float t = float(i + jitter * 2.0 - 1.0) / float(numSamples);
			float2 sampleUV = lerp(segBeginUV, segEndUV, t);
			float3 rayPosVS = lerp(segBeginVS, segEndVS, t);

			if (all(sampleUV >= 0.0) && all(sampleUV <= DynamicRes)) {
				float2 moments = SampleDepthMoments(sampleUV);
				// Thickness grows quadratically with distance along the ray (slow near
				// the receiver, fast toward the far end), reaching SurfaceThickness at
				// MaxThicknessDistance (= end of mip 3, the far tip of the cascade).
				float distFromReceiver = SegmentStart + t * SegmentLength;
				float thicknessScale = saturate(distFromReceiver / max(MaxThicknessDistance, 1e-5));
				float thickness = SurfaceThickness * thicknessScale * thicknessScale;
				float shadowFront = ComputeVSM(moments.xy, rayPosVS.z);
				float shadowBack = ComputeVSM_Lower(moments.xy, rayPosVS.z - thickness);
				shadow *= 1.0 - (1.0 - shadowFront) * (1.0 - shadowBack);
			}
		}
	}

	shadowOutput[dtid] = shadow;
}

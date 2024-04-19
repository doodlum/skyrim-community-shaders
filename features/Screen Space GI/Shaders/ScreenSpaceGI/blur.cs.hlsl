#include "../Common/GBuffer.hlsli"
#include "../Common/VR.hlsli"
#include "common.hlsli"

Texture2D<lpfloat4> srcGI : register(t0);
Texture2D<uint> srcAccumFrames : register(t1);  // maybe half-res
RWTexture2D<lpfloat4> outGI : register(u0);

// samples = 8, min distance = 0.5, average samples on radius = 2
static const float3 g_Poisson8[8] = {
	float3(-0.4706069, -0.4427112, +0.6461146),
	float3(-0.9057375, +0.3003471, +0.9542373),
	float3(-0.3487388, +0.4037880, +0.5335386),
	float3(+0.1023042, +0.6439373, +0.6520134),
	float3(+0.5699277, +0.3513750, +0.6695386),
	float3(+0.2939128, -0.1131226, +0.3149309),
	float3(+0.7836658, -0.4208784, +0.8895339),
	float3(+0.1564120, -0.8198990, +0.8346850)
};

[numthreads(8, 8, 1)] void main(const uint2 dtid : SV_DispatchThreadID) {
	const float radius = BlurRadius / srcAccumFrames[dtid];
	const uint numSamples = 8;

	const float2 uv = (dtid + .5) * RcpFrameDim;
	uint eyeIndex = GET_EYE_IDX(uv);

	lpfloat4 sum = 0;
	float4 wsum = 0;
	[unroll] for (uint i = 0; i < numSamples; i++)
	{
		float w = g_Poisson8[i].z;

		float2 pxOffset = radius * g_Poisson8[i].xy;
		float2 uvOffset = pxOffset * RcpFrameDim;
		float2 uvSample = uv + uvOffset;

		lpfloat4 gi = srcGI.SampleLevel(samplerLinearClamp, uvSample * res_scale, 0);

		sum += gi * w;
		wsum += w;
	}

	outGI[dtid] = sum / (wsum + 1e-6);
}
#include "../Common/VR.hlsl"
cbuffer PerFrame : register(b0)
{
	float2 BufferDim;
	float2 RcpBufferDim;
	float4x4 ProjMatrix[2];
	float4x4 InvProjMatrix[2];
	float4 DynamicRes;
	float4 InvDirLightDirectionVS[2];
	float ShadowDistance;
	uint MaxSamples;
	float FarDistanceScale;
	float FarThicknessScale;
	float FarHardness;
	float NearDistance;
	float NearThickness;
	float NearHardness;
	float BlurRadius;
	float BlurDropoff;
};
RWTexture2D<float> OcclusionRW : register(u0);

SamplerState LinearSampler : register(s0);
Texture2D<float> DepthTexture : register(t0);

// Get a raw depth from the depth buffer.
float GetDepth(float2 uv)
{
	// #ifdef VR
	// 	uv = ConvertToVRUV(uv);
	// #else
	// 	uv = uv;
	// #endif  // VR
	// effects like screen space shadows, can get artefacts if a point sampler is used
	return DepthTexture.SampleLevel(LinearSampler, uv * DynamicRes.xy, 0).r;
}

// Inverse project UV + raw depth into the view space.
float3 InverseProjectUVZ(float2 uv, float z, uint a_eyeIndex = 0)
{
	uv.y = 1 - uv.y;
	float4 cp = float4(float3(uv, z) * 2 - 1, 1);  // convert uv [0,1] to [-1, 1]
	float4 vp = mul(InvProjMatrix[a_eyeIndex], cp);
	return float3(vp.xy, vp.z) / vp.w;
}

float3 InverseProjectUV(float2 uv, uint a_eyeIndex = 0)
{
	return InverseProjectUVZ(uv, GetDepth(uv), a_eyeIndex);
}

float2 ViewToUV(float3 x, bool is_position = true, uint a_eyeIndex = 0)
{
	float4 uv = mul(ProjMatrix[a_eyeIndex], float4(x, (float)is_position));
	uv.xy /= uv.w;                            // normalize
	uv.xy = uv * float2(0.5f, -0.5f) + 0.5f;  // convert from [-1,1] to [0,1]
	return uv.xy;
}

bool IsSaturated(float value) { return value == saturate(value); }
bool IsSaturated(float2 value) { return IsSaturated(value.x) && IsSaturated(value.y); }

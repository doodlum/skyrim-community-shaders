cbuffer SSSData : register(b5)
{
	bool EnableSSS;
	uint FrameCount;
};

Texture2D<float> TexOcclusionSampler : register(t21);

#define LinearSampler SampShadowMaskSampler

float2 SSGetDynamicResolutionAdjustedScreenPosition(float2 uv)
{
	return uv * DynamicResolutionParams1.xy;
}

float PrepassScreenSpaceShadows(float3 positionWS, uint a_eyeIndex)
{
#if defined(EYE)
	return 1;
#else
	if (EnableSSS && !FrameParams.z) {
        float2 texCoord = ViewToUV(WorldToView(positionWS, true, a_eyeIndex), true, a_eyeIndex);
		float2 coords = SSGetDynamicResolutionAdjustedScreenPosition(texCoord) / 2;
		
#	ifdef VR
		// X coordinate is further shifted because VR uav is split between left and right eye.
		// So X in [0, .5] is the left eye and [.5, 1] is the right eye
		coords.x = (coords.x + (a_eyeIndex * .5)) / 2;
#	endif
		
		float shadow = TexOcclusionSampler.SampleLevel(LinearSampler, coords, 0);
		return shadow;
	}
	return 1;
#endif
}
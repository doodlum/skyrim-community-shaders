struct LightingData
{
	float WaterHeight[25];
	bool Reflections;
	float4 CameraData;
    float2 BufferDim;
};

StructuredBuffer<LightingData> lightingData : register(t126);

Texture2D<float4> TexDepthSampler : register(t20);

// Get a raw depth from the depth buffer. [0,1] in uv space
float GetDepth(float2 uv, uint a_eyeIndex = 0)
{
	uv = ConvertToStereoUV(uv, a_eyeIndex);
	return TexDepthSampler.Load(int3(uv * lightingData[0].BufferDim, 0));
}

float GetScreenDepth(float depth)
{
	return (lightingData[0].CameraData.w / (-depth * lightingData[0].CameraData.z + lightingData[0].CameraData.x));
}

float GetScreenDepth(float2 uv, uint a_eyeIndex = 0)
{
	float depth = GetDepth(uv, a_eyeIndex);
	return GetScreenDepth(depth);
}
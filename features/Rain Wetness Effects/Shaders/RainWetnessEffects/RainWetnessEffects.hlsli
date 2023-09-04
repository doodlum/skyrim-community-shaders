

struct PerPassRainWetnessEffects
{
	bool IsRaining;
    bool EnableRainWetnessEffects;
    float RainShininessMultiplier;
    float RainSpecularMultiplier;
    float RainDiffuseMultiplier;
	float pad[3];
};

//#if defined(WATER)
//Texture2D<float4> SharedDepthTex : register(t31);
//#endif

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);
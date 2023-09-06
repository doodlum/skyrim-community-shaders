

struct PerPassRainWetnessEffects
{
    bool IsOutdoors;
    bool EnableRainWetnessEffects;
    float RainShininessMultiplier;
    float RainSpecularMultiplier;
    float RainDiffuseMultiplier;
    float TransitionPercentage;
    float ShininessMultiplierCurrent;
    float ShininessMultiplierPrevious;
    float SpecularMultiplierCurrent;
    float SpecularMultiplierPrevious;
    float DiffuseMultiplierCurrent;
    float DiffuseMultiplierPrevious;
};

//#if defined(WATER)
//Texture2D<float4> SharedDepthTex : register(t31);
//#endif

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);


struct PerPassRainWetnessEffects
{
    bool EnableEffect;
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

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);
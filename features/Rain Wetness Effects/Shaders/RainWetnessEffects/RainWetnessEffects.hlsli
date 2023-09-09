

struct PerPassRainWetnessEffects
{
	bool EnableEffect;
	bool EnableRainWetnessEffects;
	float RainShininessMultiplierDay;
	float RainSpecularMultiplierDay;
	float RainDiffuseMultiplierDay;
	float RainShininessMultiplierNight;
	float RainSpecularMultiplierNight;
	float RainDiffuseMultiplierNight;
	float WeatherTransitionPercentage;
	float DayNightTransition;
	float ShininessMultiplierCurrentDay;
	float ShininessMultiplierPreviousDay;
	float SpecularMultiplierCurrentDay;
	float SpecularMultiplierPreviousDay;
	float DiffuseMultiplierCurrentDay;
	float DiffuseMultiplierPreviousDay;
	float ShininessMultiplierCurrentNight;
	float ShininessMultiplierPreviousNight;
	float SpecularMultiplierCurrentNight;
	float SpecularMultiplierPreviousNight;
	float DiffuseMultiplierCurrentNight;
	float DiffuseMultiplierPreviousNight;
};

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);
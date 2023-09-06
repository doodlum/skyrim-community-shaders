#pragma once

#include "Buffer.h"
#include "Feature.h"

struct RainWetnessEffects : Feature
{
public:
	static RainWetnessEffects* GetSingleton()
	{
		static RainWetnessEffects singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "Rain Wetness Effects"; }
	virtual inline std::string GetShortName() { return "RainWetnessEffects"; }

	struct Settings
	{
		uint32_t EnableRainWetnessEffects = 1;
		float RainShininessMultiplier = 2.0f;
		float RainSpecularMultiplier = 7.5f;
		float RainDiffuseMultiplier = 0.9f;
	};

	struct alignas(16) PerPass
	{
		uint32_t IsOutdoors;
		Settings settings;
		float TransitionPercentage;
		float ShininessMultiplierCurrent;
		float ShininessMultiplierPrevious;
		float SpecularMultiplierCurrent;
		float SpecularMultiplierPrevious;
		float DiffuseMultiplierCurrent;
		float DiffuseMultiplierPrevious;
	};

	Settings settings;

	std::unique_ptr<Buffer> perPass = nullptr;

	virtual void SetupResources();
	virtual inline void Reset() {}

	virtual void DrawSettings();

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
};

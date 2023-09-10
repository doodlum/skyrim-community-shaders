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
		float RainShininessMultiplierDay = 2.0f;
		float RainSpecularMultiplierDay = 15.0f;
		float RainDiffuseMultiplierDay = 0.5f;
		float RainShininessMultiplierNight = 2.0f;
		float RainSpecularMultiplierNight = 15.0f;
		float RainDiffuseMultiplierNight = 0.5f;
	};

	struct alignas(16) PerPass
	{
		Settings settings;
		float RainShininessMultiplier;
		float RainSpecularMultiplier;
		float RainDiffuseMultiplier;
		float pad[2];
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

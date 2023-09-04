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
		float RainSpecularMultiplier = 10.0f;
		float RainDiffuseMultiplier = 0.9f;
	};

	struct alignas(16) PerPass
	{
		uint32_t IsRaining;
		Settings settings;
		float pad[3];
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

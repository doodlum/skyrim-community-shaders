#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "State.h"

struct WetnessEffects : Feature
{
public:
	static WetnessEffects* GetSingleton()
	{
		static WetnessEffects singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "Wetness Effects"; }
	virtual inline std::string GetShortName() override { return "WetnessEffects"; }
	virtual inline std::string_view GetShaderDefineName() override { return "WETNESS_EFFECTS"; }

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	struct Settings
	{
		uint EnableWetnessEffects = true;
		float MaxRainWetness = 1.0f;
		float MaxPuddleWetness = 2.667f;
		float MaxShoreWetness = 0.5f;
		uint ShoreRange = 32;
		float PuddleRadius = 1.0f;
		float PuddleMaxAngle = 0.95f;
		float PuddleMinWetness = 0.85f;
		float MinRainWetness = 0.65f;
		float SkinWetness = 0.95f;
		float WeatherTransitionSpeed = 3.0f;

		// Raindrop fx settings
		uint EnableRaindropFx = true;
		uint EnableSplashes = true;
		uint EnableRipples = true;
		uint EnableChaoticRipples = true;
		float RaindropFxRange = 1000.f;
		float RaindropGridSize = 4.f;
		float RaindropInterval = .5f;
		float RaindropChance = .3f;
		float SplashesLifetime = 10.0f;
		float SplashesStrength = 1.05f;
		float SplashesMinRadius = .3f;
		float SplashesMaxRadius = .5f;
		float RippleStrength = 1.f;
		float RippleRadius = 1.f;
		float RippleBreadth = .5f;
		float RippleLifetime = .15f;
		float ChaoticRippleStrength = .1f;
		float ChaoticRippleScale = 1.f;
		float ChaoticRippleSpeed = 20.f;
	};

	struct alignas(16) PerFrame
	{
		float Time;
		float Raining;
		float Wetness;
		float PuddleWetness;
		Settings settings;
		uint pad0[2];
	};

	Settings settings;

	PerFrame GetCommonBufferData();

	bool requiresUpdate = true;
	float wetnessDepth = 0.0f;
	float puddleDepth = 0.0f;
	float lastGameTimeValue = 0.0f;
	uint32_t currentWeatherID = 0;
	uint32_t lastWeatherID = 0;
	float previousWeatherTransitionPercentage = 0.0f;

	virtual void Reset() override;

	virtual void DrawSettings() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;

	virtual void RestoreDefaultSettings() override;
	float CalculateWeatherTransitionPercentage(float skyCurrentWeatherPct, float beginFade, bool fadeIn);
	void CalculateWetness(RE::TESWeather* weather, RE::Sky* sky, float seconds, float& wetness, float& puddleWetness);

	virtual bool SupportsVR() override { return true; };
};

#pragma once

#include "Buffer.h"
#include "Feature.h"

struct PBR : Feature
{
	static PBR* GetSingleton()
	{
		static PBR singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "PBR"; }
	virtual inline std::string GetShortName() { return "PBR"; }

	struct Settings
	{
		float GlossinessScale = 0.005f;
		float MinRoughness = 0.3f;
		float MaxRoughness = 0.8f;
		float NonmetalThreshold = 0.2f;
		float MetalThreshold = 0.3f;
		float SunShadowAO = 0.7f;
		float ParallaxAO = 0.5f;
		float ParallaxScale = 0.2f;
	};

	struct alignas(16) PerFrame
	{
		Settings Settings;
	};

	Settings settings;

	bool updatePerFrame = false;
	ConstantBuffer* perFrame = nullptr;
	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	void ModifyLighting(const RE::BSShader* shader, const uint32_t descriptor);
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
};

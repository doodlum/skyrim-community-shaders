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
		float NonMetalGlossiness = 0.005f;
		float MetalGlossiness = 0.009f;
		float MinRoughness = 0.1f;
		float MaxRoughness = 0.7f;
		float NonMetalThreshold = 0.1f;
		float MetalThreshold = 0.2f;
		float SunShadowAO = 0.7f;
		float ParallaxAO = 0.5f;
		float ParallaxScale = 0.08f;
		float Exposure = 1.0f;
		float GrassRoughness = 0.7f;
		float GrassBentNormal = 1.0f;
		float FogIntensity = 0.0f;
		float AmbientDiffuse = 1.0f;
		float AmbientSpecular = 1.0f;
		float CubemapIntensity = 1.0f;
	};

	struct alignas(16) PerFrame
	{	
		DirectX::XMFLOAT4 EyePosition;
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

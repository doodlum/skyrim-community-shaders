#pragma once

#include "RE/BSGraphicsTypes.h"

#include "Buffer.h"

#include <RE/BSGraphicsTypes.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class GrassLighting
{
public:
	static GrassLighting* GetSingleton()
	{
		static GrassLighting singleton;
		return &singleton;
	}

	struct Settings
	{
		float Glossiness = 20;
		float SpecularStrength = 0.5;
		float SubsurfaceScatteringSaturation = 1.5;
		float SubsurfaceScatteringAmount = 0.5;
		float DirectionalLightDimmer = 1;
		float PointLightDimmer = 3;
		std::uint32_t EnableDirLightFix = 1;
		std::uint32_t EnablePointLights = 1;
	};

	struct alignas(16) PerFrame
	{
		DirectX::XMFLOAT4 EyePosition;
		DirectX::XMFLOAT3X4 DirectionalAmbient;
		float SunlightScale;
		Settings Settings;
		float pad0;
		float pad1;
		float pad2;
	};

	Settings settings;

	bool updatePerFrame = false;
	ConstantBuffer* perFrame = nullptr;
	void SetupResources();
	void Reset();

	void DrawSettings();
	void ModifyGrass(const RE::BSShader* shader, const uint32_t descriptor);
	void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	void Load(json& o_json);
	void Save(json& o_json);

private:
	bool enabled = true;
};

#pragma once

#include "Buffer.h"
#include "Feature.h"

struct WetnessEffects : Feature
{
public:
	static WetnessEffects* GetSingleton()
	{
		static WetnessEffects singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "Wetness Effects"; }
	virtual inline std::string GetShortName() { return "WetnessEffects"; }
	inline std::string_view GetShaderDefineName() override { return "WETNESS_EFFECTS"; }

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	struct Settings
	{
		uint32_t EnableWetnessEffects = 1;
		float DarkeningAmount = 2.0;
		float MinimumRoughness = 0.2f;
		uint32_t WaterEdgeRange = 32;
	};

	struct PerPass
	{
		bool Reflections;
		float Wetness;
		float WaterHeight;
		DirectX::XMFLOAT3X4 DirectionalAmbientWS;
		float DarkeningAmount;
		float MinimumRoughness;
		uint32_t WaterEdgeRange;
	};

	Settings settings;

	std::unique_ptr<Buffer> perPass = nullptr;

	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
};

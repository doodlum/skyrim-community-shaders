#pragma once
#include "Buffer.h"
#include "Feature.h"

struct WaterParallax : Feature
{
public:
	static WaterParallax* GetSingleton()
	{
		static WaterParallax singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "Water Parallax"; }
	virtual inline std::string GetShortName() { return "WaterParallax"; }
	virtual inline std::string_view GetShaderDefineName() override { return "WATER_PARALLAX"; }

	bool HasShaderDefine(RE::BSShader::Type shaderType) override;

	virtual void SetupResources() override;

	virtual void DrawSettings() override;

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor) override;

	virtual void RestoreDefaultSettings() override;
	bool SupportsVR() override { return true; };
};

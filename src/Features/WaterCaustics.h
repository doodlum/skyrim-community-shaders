#pragma once

#include "Feature.h"

struct WaterCaustics : Feature
{
public:
	static WaterCaustics* GetSingleton()
	{
		static WaterCaustics singleton;
		return &singleton;
	}

	ID3D11ShaderResourceView* causticsView;

	virtual inline std::string GetName() { return "Water Caustics"; }
	virtual inline std::string GetShortName() { return "WaterCaustics"; }
	inline std::string_view GetShaderDefineName() override { return "WATER_CAUSTICS"; }

	bool HasShaderDefine(RE::BSShader::Type shaderType) override;

	virtual void SetupResources() override;

	virtual void DrawSettings();

	virtual void Prepass() override;

	virtual void RestoreDefaultSettings();

	bool SupportsVR() override { return true; };
};

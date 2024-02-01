#pragma once

#include "Buffer.h"
#include "Feature.h"

struct CloudShadow : Feature
{
	static CloudShadow* GetSingleton()
	{
		static CloudShadow singleton;
		return &singleton;
	}

	virtual inline std::string GetName() { return "Cloud Shadow"; }
	virtual inline std::string GetShortName() { return "CloudShadow"; }
	virtual inline std::string_view GetShaderDefineName() { return "CLOUD_SHADOW"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type) { return true; }

	struct Settings
	{
		uint EnableCloudShadow = true;

		float CloudHeight = 2e3f / 1.428e-2f;
		float PlanetRadius = 6371e3f / 1.428e-2f;

		float ShadowBlend = 1.f;
		float DiffuseLightBrightness = 0.5f;
		float DiffuseLightSaturation = 1.f;
	} settings;

	struct alignas(16) PerPass
	{
		Settings Settings;

		float RcpHPlusR;

		float padding;
	};
	std::unique_ptr<Buffer> perPass = nullptr;

	std::set<ID3D11BlendState*> mappedBlendStates;
	std::map<ID3D11BlendState*, ID3D11BlendState*> modifiedBlendStates;

	Texture2D* texCubemapCloudOcc = nullptr;
	ID3D11RenderTargetView* cubemapCloudOccRTVs[6] = { nullptr };
	ID3D11ShaderResourceView* cubemapCloudOccDebugSRV = nullptr;

	virtual void SetupResources();
	virtual inline void Reset() {}

	virtual void DrawSettings();

	void CheckResourcesSide(int side);
	void ModifySky(const RE::BSShader* shader, const uint32_t descriptor);
	void ModifyLighting();
	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();
};

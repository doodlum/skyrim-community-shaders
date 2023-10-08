#pragma once

#include "Buffer.h"
#include "Feature.h"

struct DynamicCubemaps : Feature
{
public:
	static DynamicCubemaps* GetSingleton()
	{
		static DynamicCubemaps singleton;
		return &singleton;
	}

	bool renderedScreenCamera = false;

	// Compute pre-filtered specular environment map.
	ID3D11SamplerState* computeSampler = nullptr;

	// Specular irradiance
	struct alignas(16) SpecularMapFilterSettingsCB
	{
		float roughness;
		float padding[3];
	};

	ID3D11ComputeShader* spmapProgram = nullptr;
	ConstantBuffer* spmapCB = nullptr;
	Texture2D* envTexture = nullptr;
	Texture2D* unfilteredEnvTexture = nullptr;

	// Diffuse irradiance

	ID3D11PixelShader* irmapProgramPS = nullptr;
	ID3D11ComputeShader* irmapProgram = nullptr;
	Texture2D* irmapTexture = nullptr;

	//BRDF 2D LUT

	ID3D11ComputeShader* spBRDFProgram = nullptr;
	Texture2D* spBRDFLUT = nullptr;
	ID3D11SamplerState* spBRDFSampler = nullptr;

	void UpdateCubemap();
	void CreateResources();

	virtual inline std::string GetName() { return "Dynamic Cubemaps"; }
	virtual inline std::string GetShortName() { return "DynamicCubemaps"; }

	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
};

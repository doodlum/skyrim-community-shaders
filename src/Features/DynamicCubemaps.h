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
	inline std::string_view GetShaderDefineName() override { return "DYNAMIC_CUBEMAPS"; }
	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();
	virtual void DataLoaded() override;

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);
	std::vector<std::string> iniVRCubeMapSettings{
		{ "bAutoWaterSilhouetteReflections:Water" },  //IniSettings 0x1eaa018
		{ "bForceHighDetailReflections:Water" },      //IniSettings 0x1eaa030
	};

	std::map<std::string, uintptr_t> hiddenVRCubeMapSettings{
		{ "bReflectExplosions:Water", 0x1eaa000 },
		{ "bReflectLODLand:Water", 0x1eaa060 },
		{ "bReflectLODObjects:Water", 0x1eaa078 },
		{ "bReflectLODTrees:Water", 0x1eaa090 },
		{ "bReflectSky:Water", 0x1eaa0a8 },
		{ "bUseWaterRefractions:Water", 0x1eaa0c0 },
	};
};

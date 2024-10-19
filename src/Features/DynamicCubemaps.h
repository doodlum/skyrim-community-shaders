#pragma once

#include "Buffer.h"
#include "Feature.h"
#include "Util.h"

class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource);
	static bool Register();
};

struct DynamicCubemaps : Feature
{
public:
	static DynamicCubemaps* GetSingleton()
	{
		static DynamicCubemaps singleton;
		return &singleton;
	}

	const std::string defaultDynamicCubeMapSavePath = "Data\\textures\\DynamicCubemaps";

	// Specular irradiance

	ID3D11SamplerState* computeSampler = nullptr;

	struct alignas(16) SpecularMapFilterSettingsCB
	{
		float roughness;
		float pad[3];
	};
	STATIC_ASSERT_ALIGNAS_16(SpecularMapFilterSettingsCB);

	ID3D11ComputeShader* specularIrradianceCS = nullptr;
	ConstantBuffer* spmapCB = nullptr;
	Texture2D* envTexture = nullptr;
	Texture2D* envReflectionsTexture = nullptr;
	ID3D11UnorderedAccessView* uavArray[7];
	ID3D11UnorderedAccessView* uavReflectionsArray[7];

	// Reflection capture

	struct alignas(16) UpdateCubemapCB
	{
		uint Reset;
		float3 CameraPreviousPosAdjust;
	};
	STATIC_ASSERT_ALIGNAS_16(UpdateCubemapCB);
	ID3D11ComputeShader* updateCubemapCS = nullptr;
	ConstantBuffer* updateCubemapCB = nullptr;

	ID3D11ComputeShader* inferCubemapCS = nullptr;
	ID3D11ComputeShader* inferCubemapReflectionsCS = nullptr;

	Texture2D* envCaptureTexture = nullptr;
	Texture2D* envCaptureRawTexture = nullptr;
	Texture2D* envCapturePositionTexture = nullptr;
	Texture2D* envInferredTexture = nullptr;

	ID3D11ShaderResourceView* defaultCubemap = nullptr;

	bool activeReflections = false;
	bool resetCapture = true;
	bool recompileFlag = false;

	enum class NextTask
	{
		kCapture,
		kInferrence,
		kInferrence2,
		kIrradiance,
		kIrradiance2
	};

	NextTask nextTask = NextTask::kCapture;

	// Editor window

	struct Settings
	{
		uint EnabledCreator = false;
		uint EnabledSSR = true;
		uint MaxIterations = static_cast<uint>(REL::Relocate(16, 16, 48));
		uint pad0{};
		float4 CubemapColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	};

	Settings settings;
	std::string maxIterationsString = "";  // required to avoid string going out of scope for defines
	bool enabledAtBoot = false;
	void UpdateCubemap();

	void PostDeferred();

	virtual inline std::string GetName() override { return "Dynamic Cubemaps"; }
	virtual inline std::string GetShortName() override { return "DynamicCubemaps"; }
	virtual inline std::string_view GetShaderDefineName() override { return "DYNAMIC_CUBEMAPS"; }
	virtual inline std::vector<std::pair<std::string_view, std::string_view>> GetShaderDefineOptions() override;

	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	virtual void SetupResources() override;
	virtual void Reset() override;

	virtual void SaveSettings(json&) override;
	virtual void LoadSettings(json&) override;
	virtual void RestoreDefaultSettings() override;
	virtual void DrawSettings() override;
	virtual void DataLoaded() override;
	virtual void PostPostLoad() override;

	std::map<std::string, Util::GameSetting> SSRSettings{
		{ "fWaterSSRNormalPerturbationScale:Display", { "Water Normal Perturbation Scale", "Controls the scale of normal perturbations for Screen Space Reflections (SSR) on water surfaces.", 0, 0.05f, 0.f, 1.f } },
		{ "fWaterSSRBlurAmount:Display", { "Water SSR Blur Amount", "Defines the amount of blur applied to Screen Space Reflections on water surfaces.", 0, 0.3f, 0.f, 1.f } },
		{ "fWaterSSRIntensity:Display", { "Water SSR Intensity", "Adjusts the intensity or strength of Screen Space Reflections on water.", 0, 1.3f, 0.f, 5.f } },
		{ "bDownSampleNormalSSR:Display", { "Down Sample Normal SSR", "Enables or disables downsampling of normals for SSR to improve performance.", 0, true, false, true } }
	};

	std::map<std::string, Util::GameSetting> iniVRCubeMapSettings{
		{ "bAutoWaterSilhouetteReflections:Water", { "Auto Water Silhouette Reflections", "Automatically reflects silhouettes on water surfaces.", 0, true, false, true } },
		{ "bForceHighDetailReflections:Water", { "Force High Detail Reflections", "Forces the use of high-detail reflections on water surfaces.", 0, true, false, true } }
	};

	std::map<std::string, Util::GameSetting> hiddenVRCubeMapSettings{
		{ "bReflectExplosions:Water", { "Reflect Explosions", "Enables reflection of explosions on water surfaces.", 0x1eaa000, true, false, true } },
		{ "bReflectLODLand:Water", { "Reflect LOD Land", "Enables reflection of low-detail (LOD) terrain on water surfaces.", 0x1eaa060, true, false, true } },
		{ "bReflectLODObjects:Water", { "Reflect LOD Objects", "Enables reflection of low-detail (LOD) objects on water surfaces.", 0x1eaa078, true, false, true } },
		{ "bReflectLODTrees:Water", { "Reflect LOD Trees", "Enables reflection of low-detail (LOD) trees on water surfaces.", 0x1eaa090, true, false, true } },
		{ "bReflectSky:Water", { "Reflect Sky", "Enables reflection of the sky on water surfaces.", 0x1eaa0a8, true, false, true } },
		{ "bUseWaterRefractions:Water", { "Use Water Refractions", "Enables refractions for water surfaces, affecting how light bends through water.", 0x1eaa0c0, true, false, true } }
	};

	virtual void ClearShaderCache() override;
	ID3D11ComputeShader* GetComputeShaderUpdate();
	ID3D11ComputeShader* GetComputeShaderInferrence();
	ID3D11ComputeShader* GetComputeShaderInferrenceReflections();
	ID3D11ComputeShader* GetComputeShaderSpecularIrradiance();

	void UpdateCubemapCapture();

	void Inferrence(bool a_reflections);

	void Irradiance(bool a_reflections);

	virtual bool SupportsVR() override { return true; };
};

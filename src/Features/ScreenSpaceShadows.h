#pragma once

#include "Buffer.h"

struct ScreenSpaceShadows : Feature
{
public:
	virtual inline std::string GetName() override { return "Screen Space Shadows"; }
	virtual std::string GetDisplayName() override { return T("feature.screen_space_shadows.name", "Screen Space Shadows"); }
	virtual inline std::string GetShortName() override { return "ScreenSpaceShadows"; }
	virtual inline std::string_view GetShaderDefineName() override { return "SCREEN_SPACE_SHADOWS"; }
	virtual std::string_view GetCategory() const override { return FeatureCategories::kLighting; }

	/** @brief Returns a localized description and list of key features for the UI summary panel. */
	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return { T("feature.screen_space_shadows.description", "Screen Space Shadows enhances shadow quality by adding detailed contact shadows and improving shadow accuracy.\nThis technique adds fine-detail shadows that traditional shadow mapping might miss."),
			{ T("feature.screen_space_shadows.key_feature_1", "Enhanced contact shadows"),
				T("feature.screen_space_shadows.key_feature_2", "Improved shadow detail"),
				T("feature.screen_space_shadows.key_feature_3", "Better shadow accuracy"),
				T("feature.screen_space_shadows.key_feature_4", "Fine-scale shadow effects"),
				T("feature.screen_space_shadows.key_feature_5", "Configurable shadow contrast") } };
	}

	bool HasShaderDefine(RE::BSShader::Type shaderType) override;

	struct Settings
	{
		bool Enabled = true;
		bool BlurDepthPyramid = false;
		float SurfaceThickness = 2.0f;
		float ShadowContrast = 1.0f;
		float RayLength = 100.0f;
		int SampleCount = 16;
	};
	Settings settings;

	static constexpr float kReferenceRayLength = 100.0f;
	static constexpr int kMinBaseSamples = 8;
	static constexpr int kMaxBaseSamples = 128;
	static constexpr uint kShaderCompilationDebounceFrames = 2;

	struct alignas(16) SSSCB
	{
		float2 FrameDim;
		float2 RcpTexDim;

		float2 TexDim;
		float2 DynamicRes;

		float SurfaceThickness;
		float MaxThicknessDistance;  // world units; quadratic thickness reaches SurfaceThickness at this distance from the receiver
		float SegmentStart;          // world units along the ray where this dispatch's segment begins
		uint CurrentMip;

		float3 LightWorldDir;
		float SegmentLength;  // world units length of this dispatch's segment

		float ShadowContrast;
		float3 pad;
	};
	STATIC_ASSERT_ALIGNAS_16(SSSCB);

	eastl::unique_ptr<ConstantBuffer> sssCB;

	eastl::unique_ptr<Texture2D> texDepthMipPrefiltered[4];  // R32G32_FLOAT (linearZ, linearZ²); raw output of PrefilterDepthsCS — used for accurate per-pixel position reconstruction.
	eastl::unique_ptr<Texture2D> texDepthMip[4];             // R32G32_FLOAT; optional blurred copy of prefiltered, used for VSM sampling along the ray.
	eastl::unique_ptr<Texture2D> texShadowMip[4];            // R8_UNORM, raymarched visibility (Chebyshev applied inside ShadowsCS)
	eastl::unique_ptr<Texture2D> texShadowWork[4];           // R8_UNORM, blur/upscale working set (mip 0-3)
	eastl::unique_ptr<Texture2D> screenSpaceShadowsTexture;

	winrt::com_ptr<ID3D11SamplerState> pointClampSampler;

	winrt::com_ptr<ID3D11ComputeShader> prefilterDepthsCS;
	bool prefilterUsesTerrainBlending = false;
	winrt::com_ptr<ID3D11ComputeShader> blurDepthCS;
	// One ShadowsCS variant per mip — each compiled with its own MIP_SAMPLE_COUNT
	// define so the loop bound is a constant the compiler can unroll.  Mip 3 has
	// the highest sample count; each step toward mip 0 halves the count.
	winrt::com_ptr<ID3D11ComputeShader> shadowsCS[4];
	int compiledBaseSampleCount = -1;
	int attemptedBaseSampleCount = -1;
	uint shaderCompilationDelayFrames = 0;
	/** @brief Compiles the per-mip shadow ray-march variants for the requested base sample count. */
	void CompileShadowsCS(int baseSampleCount);
	winrt::com_ptr<ID3D11ComputeShader> upscaleCS;
	winrt::com_ptr<ID3D11ComputeShader> blurCS;

	/** @brief Creates the shadow-pipeline buffers, samplers, and intermediate textures. */
	virtual void SetupResources() override;
	/** @brief Draws the Screen Space Shadows settings and developer buffer viewer. */
	virtual void DrawSettings() override;
	/** @brief Releases and recompiles the feature's compute shaders. */
	virtual void ClearShaderCache() override;
	/** @brief Compiles the fixed compute-shader stages used by the shadow pipeline. */
	void CompileComputeShaders();
	/** @brief Compiles the depth prefilter for the currently active scene-depth format. */
	void CompilePrefilterDepthsCS();

	/** @brief Clears the shadow targets, renders contact shadows, and binds the final mask. */
	virtual void Prepass() override;

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void RestoreDefaultSettings() override;

	/** @brief Dispatches the depth pyramid, ray-march, and reconstruction passes. */
	void DrawShadows();
};

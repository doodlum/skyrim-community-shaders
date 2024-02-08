#pragma once

#include "Buffer.h"
#include "Feature.h"

struct TerrainBlending : Feature
{
public:
	static TerrainBlending* GetSingleton()
	{
		static TerrainBlending singleton;
		return &singleton;
	}

	static void InstallHooks()
	{
		Hooks::Install();
	}

	ID3D11DepthStencilState* depthStencilStateBackup;
	UINT stencilRefBackup;

	bool enableBlending = true;
	int optimisationDistance = 4096;
	float objectDistance = 0;

	std::set<ID3D11BlendState*> mappedBlendStates;
	std::map<ID3D11BlendState*, ID3D11BlendState*> modifiedBlendStates;

	std::set<ID3D11DepthStencilState*> mappedDepthStencilStates;
	std::map<ID3D11DepthStencilState*, ID3D11DepthStencilState*> modifiedDepthStencilStates;

	virtual inline std::string GetName() { return "Terrain Blending"; }
	virtual inline std::string GetShortName() { return "TerrainBlending"; }
	virtual inline std::string_view GetShaderDefineName() { return "TERRAIN_BLENDING"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type) { return true; }

	virtual void SetupResources();
	virtual void Reset();

	virtual void DrawSettings();

	virtual void Draw(const RE::BSShader* shader, const uint32_t descriptor);

	virtual void Load(json& o_json);
	virtual void Save(json& o_json);

	virtual void RestoreDefaultSettings();
	virtual void PostPostLoad() override;

	void BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* Pass);

	struct Hooks
	{
		struct BSLightingShader_SetupGeometry
		{
			static void thunk(RE::BSShader* This, RE::BSRenderPass* Pass, uint32_t RenderFlags)
			{
				GetSingleton()->BSLightingShader_SetupGeometry_Before(Pass);
				func(This, Pass, RenderFlags);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<0x6, BSLightingShader_SetupGeometry>(RE::VTABLE_BSLightingShader[0]);

			logger::info("[SSS] Installed hooks");
		}
	};
};

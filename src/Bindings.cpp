#include "Bindings.h"

#define DEFINE_VALUE(a_value) \
	auto& a_value = !REL::Module::IsVR() ? state->GetRuntimeData().a_value : state->GetVRRuntimeData().a_value;

void Bindings::DepthStencilStateSetDepthMode(RE::BSGraphics::DepthStencilDepthMode a_mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	DEFINE_VALUE(depthStencilDepthMode)
	DEFINE_VALUE(depthStencilDepthModePrevious)
	DEFINE_VALUE(stateUpdateFlags)

	if (depthStencilDepthMode != a_mode) {
		depthStencilDepthMode = a_mode;
		if (depthStencilDepthModePrevious != a_mode)
			stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		else
			stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
	}
}

void Bindings::AlphaBlendStateSetMode(uint32_t a_mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	DEFINE_VALUE(alphaBlendMode)
	DEFINE_VALUE(stateUpdateFlags)

	if (alphaBlendMode != a_mode) {
		alphaBlendMode = a_mode;
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetAlphaToCoverage(uint32_t a_value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	DEFINE_VALUE(alphaBlendAlphaToCoverage)
	DEFINE_VALUE(stateUpdateFlags)

	if (alphaBlendAlphaToCoverage != a_value) {
		alphaBlendAlphaToCoverage = a_value;
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetWriteMode(uint32_t a_value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	DEFINE_VALUE(alphaBlendWriteMode)
	DEFINE_VALUE(stateUpdateFlags)

	if (alphaBlendWriteMode != a_value) {
		alphaBlendWriteMode = a_value;
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::SetOverwriteTerrainMode(bool a_enable)
{
	if (overrideTerrain != a_enable) {
		overrideTerrain = a_enable;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
		DEFINE_VALUE(stateUpdateFlags)
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::SetOverwriteTerrainMaskingMode(TerrainMaskMode a_mode)
{
	if (terrainMask != a_mode) {
		terrainMask = a_mode;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
		DEFINE_VALUE(stateUpdateFlags)
		stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
	}
}

struct DepthStates
{
	ID3D11DepthStencilState* a[6][40];
};

struct BlendStates
{
	ID3D11BlendState* a[7][2][13][2];
};

// Reimplementation of elements of the renderer's bindings system to support additional features

void Bindings::SetDirtyStates(bool)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	DEFINE_VALUE(depthStencilStencilMode)
	DEFINE_VALUE(depthStencilDepthMode)
	DEFINE_VALUE(alphaBlendAlphaToCoverage)
	DEFINE_VALUE(alphaBlendMode)
	DEFINE_VALUE(alphaBlendModeExtra)
	DEFINE_VALUE(alphaBlendWriteMode)
	DEFINE_VALUE(cubeMapRenderTarget)
	DEFINE_VALUE(cubeMapRenderTargetView)
	DEFINE_VALUE(depthStencil)
	DEFINE_VALUE(depthStencilSlice)
	DEFINE_VALUE(renderTargets)
	DEFINE_VALUE(setCubeMapRenderTargetMode)
	DEFINE_VALUE(setDepthStencilMode)
	DEFINE_VALUE(setRenderTargetMode)
	DEFINE_VALUE(stateUpdateFlags)
	DEFINE_VALUE(stencilRef)

	auto rendererData = RE::BSGraphics::Renderer::GetSingleton();

	static DepthStates* depthStates = (DepthStates*)REL::RelocationID(524747, 411362).address();
	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364).address();

	if (stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET)) {
		// Build active render target view array
		ID3D11RenderTargetView* renderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		uint32_t viewCount = 0;

		if (cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kNONE) {
			// This loops through all 8 RTs or until a RENDER_TARGET_NONE entry is hit
			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
				if (renderTargets[i] == RE::RENDER_TARGETS::kNONE) {
					if (terrainMask == TerrainMaskMode::kWrite) {
						if (i == 3 || i == 4) {
							renderTargetViews[i] = terrainBlendingMask->rtv.get();
							viewCount++;
						}
					}
					break;
				} else {
					renderTargetViews[i] = rendererData->GetRuntimeData().renderTargets[renderTargets[i]].RTV;
				}

				viewCount++;

				if (setRenderTargetMode[i] == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
					context->ClearRenderTargetView(renderTargetViews[i], rendererData->GetRendererData().clearColor);
					setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
				}
			}
		} else {
			// Use a single RT for the cubemap
			renderTargetViews[0] = rendererData->GetRendererData().cubemapRenderTargets[cubeMapRenderTarget].cubeSideRTV[cubeMapRenderTargetView];
			viewCount = 1;

			if (setCubeMapRenderTargetMode == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
				context->ClearRenderTargetView(renderTargetViews[0], rendererData->GetRendererData().clearColor);
				setCubeMapRenderTargetMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
			}
		}

		switch (setDepthStencilMode) {
		case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR:
		case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_DEPTH:
		case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_STENCIL:
		case RE::BSGraphics::SetRenderTargetMode::SRTM_INIT:
			rendererData->GetRuntimeData().readOnlyDepth = false;
			break;
		}

		//
		// Determine which depth stencil to render to. When there's no active depth stencil,
		// simply send a nullptr to dx11.
		//
		ID3D11DepthStencilView* newDepthStencil = nullptr;

		if (depthStencil != -1) {
			if (rendererData->GetRuntimeData().readOnlyDepth)
				newDepthStencil = rendererData->GetDepthStencilData().depthStencils[depthStencil].readOnlyViews[depthStencilSlice];
			else
				newDepthStencil = rendererData->GetDepthStencilData().depthStencils[depthStencil].views[depthStencilSlice];

			// Only clear the stencil if specific flags are set
			if (newDepthStencil) {
				uint32_t clearFlags = 0;

				switch (setDepthStencilMode) {
				case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR:
				case RE::BSGraphics::SetRenderTargetMode::SRTM_INIT:
					clearFlags = D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
					break;

				case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_DEPTH:
					clearFlags = D3D11_CLEAR_DEPTH;
					break;

				case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_STENCIL:
					clearFlags = D3D11_CLEAR_STENCIL;
					break;
				}

				if (clearFlags) {
					context->ClearDepthStencilView(newDepthStencil, clearFlags, 1.0f, 0);
					setDepthStencilMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
				}
			}
		}

		context->OMSetRenderTargets(viewCount, renderTargetViews, newDepthStencil);
		stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
	}

	if (stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE)) {
		stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		if (overrideTerrain)
			context->OMSetDepthStencilState(depthStates->a[std::to_underlying(RE::BSGraphics::DepthStencilDepthMode::kTestGreaterEqual)][depthStencilStencilMode], stencilRef);
		else
			context->OMSetDepthStencilState(depthStates->a[std::to_underlying(depthStencilDepthMode)][depthStencilStencilMode], stencilRef);
	}

	if (stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND)) {
		stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
		if (overrideTerrain)
			context->OMSetBlendState(blendStates->a[1][alphaBlendAlphaToCoverage][alphaBlendWriteMode][alphaBlendModeExtra], nullptr, 0xFFFFFFFF);
		else
			context->OMSetBlendState(blendStates->a[alphaBlendMode][alphaBlendAlphaToCoverage][alphaBlendWriteMode][alphaBlendModeExtra], nullptr, 0xFFFFFFFF);
	}
}

void Bindings::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto mainTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	{
		D3D11_TEXTURE2D_DESC texDesc{};
		mainTexture.texture->GetDesc(&texDesc);
		terrainBlendingMask = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		mainTexture.SRV->GetDesc(&srvDesc);
		terrainBlendingMask->CreateSRV(srvDesc);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		mainTexture.RTV->GetDesc(&rtvDesc);
		terrainBlendingMask->CreateRTV(rtvDesc);
	}
}

void Bindings::Reset()
{
	SetOverwriteTerrainMode(false);
	SetOverwriteTerrainMaskingMode(TerrainMaskMode::kNone);

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	FLOAT clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->ClearRenderTargetView(terrainBlendingMask->rtv.get(), clear);
}

#include "Bindings.h"

void Bindings::DepthStencilStateSetDepthMode(RE::BSGraphics::DepthStencilDepthMode a_mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().depthStencilDepthMode != a_mode) {
		state->GetRuntimeData().depthStencilDepthMode = a_mode;
		if (state->GetRuntimeData().depthStencilDepthModePrevious != a_mode)
			state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		else
			state->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
	}
}

void Bindings::AlphaBlendStateSetMode(uint32_t a_mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendMode != a_mode) {
		state->GetRuntimeData().alphaBlendMode = a_mode;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetAlphaToCoverage(uint32_t a_value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendAlphaToCoverage != a_value) {
		state->GetRuntimeData().alphaBlendAlphaToCoverage = a_value;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetWriteMode(uint32_t a_value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendWriteMode != a_value) {
		state->GetRuntimeData().alphaBlendWriteMode = a_value;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::SetOverwriteTerrainMode(bool a_enable)
{
	if (overrideTerrain != a_enable) {
		overrideTerrain = a_enable;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::SetOverwriteTerrainMaskingMode(TerrainMaskMode a_mode)
{
	if (terrainMask != a_mode) {
		terrainMask = a_mode;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
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
	auto& runtimeData = state->GetRuntimeData();

	auto rendererData = RE::BSGraphics::Renderer::GetSingleton();

	static DepthStates* depthStates = (DepthStates*)REL::RelocationID(524747, 411362, 0x317EA60).address();
	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364, 0x317F6C0).address();

	if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET)) {
		// Build active render target view array
		ID3D11RenderTargetView* renderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		uint32_t viewCount = 0;

		if (state->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kNONE) {
			// This loops through all 8 RTs or until a RENDER_TARGET_NONE entry is hit
			for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
				if (state->GetRuntimeData().renderTargets[i] == RE::RENDER_TARGETS::kNONE) {
					if (terrainMask == TerrainMaskMode::kWrite) {
						if (i == 3 || i == 4) {
							renderTargetViews[i] = terrainBlendingMask->rtv.get();
							viewCount++;
						}
					}
					break;
				} else {
					renderTargetViews[i] = rendererData->GetRuntimeData().renderTargets[state->GetRuntimeData().renderTargets[i]].RTV;
				}

				viewCount++;

				if (state->GetRuntimeData().setRenderTargetMode[i] == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
					context->ClearRenderTargetView(renderTargetViews[i], rendererData->GetRendererData().clearColor);
					state->GetRuntimeData().setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
				}
			}
		} else {
			// Use a single RT for the cubemap
			renderTargetViews[0] = rendererData->GetRendererData().cubemapRenderTargets[state->GetRuntimeData().cubeMapRenderTarget].cubeSideRTV[state->GetRuntimeData().cubeMapRenderTargetView];
			viewCount = 1;

			if (state->GetRuntimeData().setCubeMapRenderTargetMode == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
				context->ClearRenderTargetView(renderTargetViews[0], rendererData->GetRendererData().clearColor);
				state->GetRuntimeData().setCubeMapRenderTargetMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
			}
		}

		switch (state->GetRuntimeData().setDepthStencilMode) {
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
		ID3D11DepthStencilView* depthStencil = nullptr;

		if (state->GetRuntimeData().depthStencil != -1) {
			if (rendererData->GetRuntimeData().readOnlyDepth)
				depthStencil = rendererData->GetDepthStencilData().depthStencils[state->GetRuntimeData().depthStencil].readOnlyViews[state->GetRuntimeData().depthStencilSlice];
			else
				depthStencil = rendererData->GetDepthStencilData().depthStencils[state->GetRuntimeData().depthStencil].views[state->GetRuntimeData().depthStencilSlice];

			// Only clear the stencil if specific flags are set
			if (depthStencil) {
				uint32_t clearFlags = 0;

				switch (state->GetRuntimeData().setDepthStencilMode) {
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
					context->ClearDepthStencilView(depthStencil, clearFlags, 1.0f, 0);
					state->GetRuntimeData().setDepthStencilMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
				}
			}
		}

		context->OMSetRenderTargets(viewCount, renderTargetViews, depthStencil);
		runtimeData.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
	}

	if (runtimeData.stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE)) {
		runtimeData.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		if (overrideTerrain)
			context->OMSetDepthStencilState(depthStates->a[(uint32_t)RE::BSGraphics::DepthStencilDepthMode::kTestGreaterEqual][runtimeData.depthStencilStencilMode], runtimeData.stencilRef);
		else
			context->OMSetDepthStencilState(depthStates->a[(uint32_t)runtimeData.depthStencilDepthMode][runtimeData.depthStencilStencilMode], runtimeData.stencilRef);
	}

	if (runtimeData.stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND)) {
		runtimeData.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
		if (overrideTerrain)
			context->OMSetBlendState(blendStates->a[1][runtimeData.alphaBlendAlphaToCoverage][runtimeData.alphaBlendWriteMode][runtimeData.alphaBlendModeExtra], nullptr, 0xFFFFFFFF);
		else
			context->OMSetBlendState(blendStates->a[runtimeData.alphaBlendMode][runtimeData.alphaBlendAlphaToCoverage][runtimeData.alphaBlendWriteMode][runtimeData.alphaBlendModeExtra], nullptr, 0xFFFFFFFF);
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

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
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (overrideTerrain != a_enable) {
		overrideTerrain = a_enable;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
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

void Bindings::SetDirtyStates(bool )
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	auto& runtimeData = state->GetRuntimeData();

	static DepthStates* depthStates = (DepthStates*)REL::RelocationID(524747, 411362, 0x317EA60).address(); 
	static BlendStates* blendStates = (BlendStates*)REL::RelocationID(524749, 411364, 0x317F6C0).address();

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
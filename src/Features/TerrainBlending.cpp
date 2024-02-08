#include "TerrainBlending.h"
#include <ShaderCache.h>

void TerrainBlending::DrawSettings()
{
	ImGui::Checkbox("Enable", &enableBlending);
}

void TerrainBlending::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	if (!enableBlending)
		return;

	if (shader->shaderType.get() == RE::BSShader::Type::Lighting && objectDistance <= optimisationDistance)
	{
		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
		auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();

		auto reflections = (!REL::Module::IsVR() ?
								   RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().cubeMapRenderTarget :
								   RE::BSGraphics::RendererShadowState::GetSingleton()->GetVRRuntimeData().cubeMapRenderTarget) == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS;

		if (!reflections && accumulator->GetRuntimeData().activeShadowSceneNode == RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]) {
			auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			auto mainTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

			const auto technique = static_cast<SIE::ShaderCache::LightingShaderTechniques>(0x3F & (descriptor >> 24));

			if (technique == SIE::ShaderCache::LightingShaderTechniques::MTLand || technique == SIE::ShaderCache::LightingShaderTechniques::MTLandLODBlend || technique == SIE::ShaderCache::LightingShaderTechniques::Snow) {
				ID3D11BlendState* blendState;
				FLOAT blendFactor[4];
				UINT sampleMask;

				context->OMGetBlendState(&blendState, blendFactor, &sampleMask);

				if (!mappedBlendStates.contains(blendState)) {
					if (!modifiedBlendStates.contains(blendState)) {
						D3D11_BLEND_DESC blendDesc;
						blendState->GetDesc(&blendDesc);

						blendDesc.AlphaToCoverageEnable = FALSE;
						blendDesc.IndependentBlendEnable = FALSE;

						for (uint32_t i = 0; i < 8; ++i) {
							blendDesc.RenderTarget[i].BlendEnable = true;
							blendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
							blendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
							blendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
							blendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
							blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
							blendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
							blendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
						}

						auto device = renderer->GetRuntimeData().forwarder;
						ID3D11BlendState* modifiedBlendState;
						DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &modifiedBlendState));
						mappedBlendStates.insert(modifiedBlendState);
						modifiedBlendStates.insert({ blendState, modifiedBlendState });
					}
					blendState = modifiedBlendStates[blendState];
					context->OMSetBlendState(blendState, blendFactor, sampleMask);
				}

				ID3D11DepthStencilState* depthStencilState;
				UINT stencilRef;
				context->OMGetDepthStencilState(&depthStencilState, &stencilRef);

				if (!mappedDepthStencilStates.contains(depthStencilState)) {
					depthStencilStateBackup = depthStencilState;
					stencilRefBackup = stencilRef;
					if (!modifiedDepthStencilStates.contains(depthStencilState)) {
						D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
						depthStencilState->GetDesc(&depthStencilDesc);
						depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
						auto device = renderer->GetRuntimeData().forwarder;
						ID3D11DepthStencilState* modifiedDepthStencilState;
						DX::ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, &modifiedDepthStencilState));
						mappedDepthStencilStates.insert(modifiedDepthStencilState);
						modifiedDepthStencilStates.insert({ depthStencilState, modifiedDepthStencilState });
					}
					depthStencilState = modifiedDepthStencilStates[depthStencilState];
					context->OMSetDepthStencilState(depthStencilState, stencilRef);
				}

				auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
				state->GetRuntimeData().stateUpdateFlags |= RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE;
				state->GetRuntimeData().stateUpdateFlags |= RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND;
			}
		}
	}
}

void TerrainBlending::SetupResources()
{
}

void TerrainBlending::Reset()
{
}

void TerrainBlending::Load(json& o_json)
{
	Feature::Load(o_json);
}

void TerrainBlending::Save(json&)
{
}

void TerrainBlending::RestoreDefaultSettings()
{
}

void TerrainBlending::PostPostLoad()
{
	Hooks::Install();
}

void TerrainBlending::BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* Pass)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	auto eyePosition = state->GetRuntimeData().posAdjust.getEye(0);
	auto objectPosition = Pass->geometry->world.translate;
	objectPosition = objectPosition - eyePosition;
	objectDistance = objectPosition.Length();
}
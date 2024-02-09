#include "TerrainBlending.h"

#include "Bindings.h"
#include "State.h"

void TerrainBlending::DrawSettings()
{
	ImGui::Checkbox("Enable", &enableBlending);
}

bool TerrainBlending::TerrainBlendingPass(RE::BSRenderPass* a_pass)
{
	if (a_pass->shaderProperty->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kMultiTextureLandscape)) {
		auto eyePosition = RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().posAdjust.getEye(0);
		auto objectPosition = a_pass->geometry->world.translate;
		objectPosition = objectPosition - eyePosition;
		auto objectDistance = objectPosition.Length();

		if (objectDistance < optimisationDistance) {
			auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();

			auto reflections = (!REL::Module::IsVR() ?
									   RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().cubeMapRenderTarget :
									   RE::BSGraphics::RendererShadowState::GetSingleton()->GetVRRuntimeData().cubeMapRenderTarget) == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS;

			return !reflections && accumulator->GetRuntimeData().activeShadowSceneNode == RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		}
	}
	return false;
}

void TerrainBlending::Draw(const RE::BSShader*, const uint32_t)
{
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

void TerrainBlending::SetupGeometry(RE::BSRenderPass* a_pass)
{
	if (enableBlending && TerrainBlendingPass(a_pass))
		Bindings::GetSingleton()->SetOverwriteTerrainMode(true);
	else
		Bindings::GetSingleton()->SetOverwriteTerrainMode(false);
}

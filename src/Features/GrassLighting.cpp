#include "GrassLighting.h"
#include "Features/Clustered.h"
#include <State.h>

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "imgui.h"
#include "reshade/reshade.hpp"
#include <Util.h>

enum class GrassShaderTechniques
{
	RenderDepth = 8,
};

void GrassLighting::DrawSettings()
{
	if (ImGui::BeginTabItem("Grass Lighting")) {
		if (ImGui::TreeNodeEx("Complex Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Specular highlights for complex grass.\nFunctions the same as typical meshes.");
			ImGui::SliderFloat("Glossiness", &settings.Glossiness, 1.0f, 20.0f);
			ImGui::SliderFloat("Specular Strength", &settings.SpecularStrength, 0.0f, 1.0f);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Soft lighting controls how evenly lit grass is.");
			ImGui::Text("Back lighting illuminates the back face of grass.");
			ImGui::Text("Combined to model the transport of light through the surface.");
			ImGui::SliderFloat("Subsurface Scattering Amount", &settings.SubsurfaceScatteringAmount, 0.0f, 1.0f);

			ImGui::Text("The vividness of the light transport. Not physically accurate.");
			ImGui::SliderFloat("Subsurface Scattering Saturation", &settings.SubsurfaceScatteringSaturation, 0.0f, 3.0f);

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Fix for grass not being affected by sunlight scale.");
			ImGui::Checkbox("Enable Directional Light Fix", (bool*)&settings.EnableDirLightFix);

			ImGui::Text("Enable point lighting on grass. Can impact performance.");
			ImGui::Checkbox("Enable Point Lights", (bool*)&settings.EnablePointLights);

			ImGui::Text("Reduces the brightness of directional light sources.");
			ImGui::SliderFloat("Directional Light Dimmer", &settings.DirectionalLightDimmer, 1.0f, 4.0f);

			ImGui::Text("Reduces the brightness of point light sources.");
			ImGui::SliderFloat("Point Light Dimmer", &settings.PointLightDimmer, 1.0f, 4.0f);
			ImGui::TreePop();
		}

		ImGui::EndTabItem();
	}
}


struct TESImagespaceManager
{
	float pad0[42];
	RE::ImageSpaceBaseData* baseData0;
	RE::ImageSpaceBaseData* baseData1;
	float pad1;
	float pad2;
	float pad3;
	float pad4;
	RE::ImageSpaceBaseData hdrData;

	static TESImagespaceManager* GetSingleton()
	{
		REL::Relocation<TESImagespaceManager**> singleton{ REL::RelocationID(527731, 414660) };
		return *singleton;
	}
};




void GrassLighting::ModifyGrass(const RE::BSShader*, const uint32_t descriptor)
{
	const auto technique = descriptor & 0b1111;
	if (technique != static_cast<uint32_t>(GrassShaderTechniques::RenderDepth)) {
		if (updatePerFrame) {
			PerFrame perFrameData{};
			ZeroMemory(&perFrameData, sizeof(perFrameData));

			auto shaderState = BSGraphics::ShaderState::QInstance();
			RE::NiTransform& dalcTransform = shaderState->DirectionalAmbientTransform;

			Util::StoreTransform3x4NoScale(perFrameData.DirectionalAmbient, dalcTransform);

			auto accumulator = State::GetCurrentAccumulator();
			auto& position = accumulator->m_EyePosition;
			auto state = BSGraphics::RendererShadowState::QInstance();

			perFrameData.EyePosition.x = position.x - state->m_PosAdjust.x;
			perFrameData.EyePosition.y = position.y - state->m_PosAdjust.y;
			perFrameData.EyePosition.z = position.z - state->m_PosAdjust.z;

			auto manager = TESImagespaceManager::GetSingleton();
			perFrameData.SunlightScale = manager->hdrData.hdr.sunlightScale;

			perFrameData.Settings = settings;

			perFrame->Update(perFrameData);

			updatePerFrame = false;
		}

		Clustered::GetSingleton()->Bind(true);
		auto context = RE::BSRenderManager::GetSingleton()->GetRuntimeData().context;

		ID3D11Buffer* buffers[2];
		context->VSGetConstantBuffers(2, 1, buffers);  // buffers[0]
		buffers[1] = perFrame->CB();
		context->VSSetConstantBuffers(2, ARRAYSIZE(buffers), buffers);
		context->PSSetConstantBuffers(2, ARRAYSIZE(buffers), buffers);
	}
}

void GrassLighting::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	switch (shader->shaderType.get()) {
	case RE::BSShader::Type::Grass:
		ModifyGrass(shader, descriptor);
		break;
	}
}

void GrassLighting::Load(json& o_json)
{
	if (o_json["Grass Lighting"].is_object()) {
		json& grassLighting = o_json["Grass Lighting"];
		if (grassLighting["Glossiness"].is_number())
			settings.Glossiness = grassLighting["Glossiness"];
		if (grassLighting["SpecularStrength"].is_number())
			settings.SpecularStrength = grassLighting["SpecularStrength"];
		if (grassLighting["SubsurfaceScatteringSaturation"].is_number())
			settings.SubsurfaceScatteringSaturation = grassLighting["SubsurfaceScatteringSaturation"];
		if (grassLighting["SubsurfaceScatteringAmount"].is_number())
			settings.SubsurfaceScatteringAmount = grassLighting["SubsurfaceScatteringAmount"];
		if (grassLighting["DirectionalLightDimmer"].is_number())
			settings.DirectionalLightDimmer = grassLighting["DirectionalLightDimmer"];
		if (grassLighting["PointLightDimmer"].is_number())
			settings.PointLightDimmer = grassLighting["PointLightDimmer"];
		if (grassLighting["EnableDirLightFix"].is_boolean())
			settings.EnableDirLightFix = grassLighting["EnableDirLightFix"];
		if (grassLighting["EnablePointLights"].is_boolean())
			settings.EnablePointLights = grassLighting["EnablePointLights"];
	}
}

void GrassLighting::Save(json& o_json)
{
	json grassLighting;
	grassLighting["Glossiness"] = settings.Glossiness;
	grassLighting["SpecularStrength"] = settings.SpecularStrength;
	grassLighting["SubsurfaceScatteringSaturation"] = settings.SubsurfaceScatteringSaturation;
	grassLighting["SubsurfaceScatteringAmount"] = settings.SubsurfaceScatteringAmount;
	grassLighting["DirectionalLightDimmer"] = settings.DirectionalLightDimmer;
	grassLighting["PointLightDimmer"] = settings.PointLightDimmer;
	grassLighting["EnableDirLightFix"] = (bool)settings.EnableDirLightFix;
	grassLighting["EnablePointLights"] = (bool)settings.EnablePointLights;
	o_json["Grass Lighting"] = grassLighting;
}

void GrassLighting::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
}

void GrassLighting::Reset()
{
	updatePerFrame = true;
}
#include "PBR.h"

#include "State.h"
#include "Util.h"

#include "Features/Clustered.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PBR::Settings,
	GlossinessScale,
	MinRoughness,
	MaxRoughness,
	NonmetalThreshold,
	MetalThreshold,
	SunShadowAO,
	ParallaxAO,
	ParallaxScale)

void PBR::DrawSettings()
{
	if (ImGui::TreeNodeEx("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::SliderFloat("Glossiness Scale", &settings.GlossinessScale, 0.0f, 0.01f);
		ImGui::SliderFloat("Min Roughness", &settings.MinRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Max Roughness", &settings.MaxRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Nonmetal Threshold", &settings.NonmetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Metal Threshold", &settings.MetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Sun Shadow AO", &settings.SunShadowAO, 0.0f, 1.0f);
		ImGui::SliderFloat("Parallax AO", &settings.ParallaxAO, 0.0f, 1.0f);
		ImGui::SliderFloat("Parallax Scale", &settings.ParallaxScale, 0.0f, 0.5f);

		ImGui::TreePop();
	}

	ImGui::EndTabItem();
}

void PBR::ModifyLighting(const RE::BSShader*, const uint32_t)
{
	if (updatePerFrame) {
		PerFrame perFrameData{};
		ZeroMemory(&perFrameData, sizeof(perFrameData));
		perFrameData.Settings = settings;
		perFrame->Update(perFrameData);
		updatePerFrame = false;
	}

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	ID3D11Buffer* buffers[1];
	buffers[0] = perFrame->CB();
	context->VSSetConstantBuffers(3, 1, buffers);
	context->PSSetConstantBuffers(3, 1, buffers);
}

void PBR::Draw(const RE::BSShader* shader, const uint32_t descriptor)
{
	switch (shader->shaderType.get()) {
	case RE::BSShader::Type::Lighting:
		ModifyLighting(shader, descriptor);
		break;
	}
}

void PBR::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void PBR::Save(json& o_json)
{
	o_json[GetName()] = settings;
}

void PBR::SetupResources()
{
	perFrame = new ConstantBuffer(ConstantBufferDesc<PerFrame>());
}

void PBR::Reset()
{
	updatePerFrame = true;
}
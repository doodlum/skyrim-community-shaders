#include "PBR.h"

#include "State.h"
#include "Util.h"

#include "Features/Clustered.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PBR::Settings,
	NonMetalGlossiness,
	MetalGlossiness,
	MinRoughness,
	MaxRoughness,
	NonMetalThreshold,
	MetalThreshold,
	SunShadowAO,
	ParallaxAO,
	ParallaxScale,
	Exposure,
	GrassRoughness,
	GrassBentNormal,
	FogIntensity,
	AmbientDiffuse,
	AmbientSpecular,
	CubemapIntensity
	)

void PBR::DrawSettings()
{
	if (ImGui::TreeNodeEx("PBR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::SliderFloat("NonMetal Glossiness", &settings.NonMetalGlossiness, 0.0f, 0.01f);
		ImGui::SliderFloat("Metal Glossiness", &settings.MetalGlossiness, 0.0f, 0.01f);
		ImGui::SliderFloat("Min Roughness", &settings.MinRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Max Roughness", &settings.MaxRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("NonMetal Threshold", &settings.NonMetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Metal Threshold", &settings.MetalThreshold, 0.0f, 1.0f);
		ImGui::SliderFloat("Sun Shadow AO", &settings.SunShadowAO, 0.0f, 1.0f);
		ImGui::SliderFloat("Parallax AO", &settings.ParallaxAO, 0.0f, 1.0f);
		ImGui::SliderFloat("Parallax Scale", &settings.ParallaxScale, 0.0f, 0.5f);
		ImGui::SliderFloat("Exposure", &settings.Exposure, 0.0f, 2.0f);
		ImGui::SliderFloat("Grass Roughness", &settings.GrassRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Grass Bent Normal", &settings.GrassBentNormal, 0.0f, 1.0f);
		ImGui::SliderFloat("Fog Intensity", &settings.FogIntensity, 0.0f, 1.0f);
		ImGui::SliderFloat("Ambient Diffuse", &settings.AmbientDiffuse, 0.0f, 2.0f);
		ImGui::SliderFloat("Ambient Specular", &settings.AmbientSpecular, 0.0f, 2.0f);
		ImGui::SliderFloat("Cubemap Intensity", &settings.CubemapIntensity, 0.0f, 10.0f);

		ImGui::TreePop();
	}

	ImGui::EndTabItem();
}

void PBR::ModifyLighting(const RE::BSShader*, const uint32_t)
{
	if (updatePerFrame) {
		PerFrame perFrameData{};
		ZeroMemory(&perFrameData, sizeof(perFrameData));
		//获取渲染设置
		perFrameData.Settings = settings;
		//获取相机位置
		auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
		auto& position = accumulator->GetRuntimeData().eyePosition;
		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

		RE::NiPoint3 eyePosition{};
		if (REL::Module::IsVR()) {
			// find center of eye position
			eyePosition = state->GetVRRuntimeData().posAdjust.getEye() + state->GetVRRuntimeData().posAdjust.getEye(1);
			eyePosition /= 2;
		} else
			eyePosition = state->GetRuntimeData().posAdjust.getEye();
		perFrameData.EyePosition.x = position.x - eyePosition.x;
		perFrameData.EyePosition.y = position.y - eyePosition.y;
		perFrameData.EyePosition.z = position.z - eyePosition.z;

		perFrame->Update(perFrameData);
		updatePerFrame = false;
	}

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	ID3D11Buffer* buffers[1];
	buffers[0] = perFrame->CB();
	context->VSSetConstantBuffers(5, 1, buffers);
	context->PSSetConstantBuffers(5, 1, buffers);
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
#include "VolumetricLighting.h"
#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	VolumetricLighting::Settings,
	EnabledVL);

void VolumetricLighting::DrawSettings()
{
	if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (!State::GetSingleton()->isVR) {
			RenderImGuiSettingsTree(VLSettings, "Skyrim Settings");
		} else {
			if (ImGui::Checkbox("Enable Volumetric Lighting in VR", reinterpret_cast<bool*>(&settings.EnabledVL))) {
				if (auto _tt = Util::HoverTooltipWrapper()) {
					ImGui::Text("Enable Volumetric Lighting in VR");
				}
				SetBooleanSettings(hiddenVRSettings, GetName(), settings.EnabledVL);
			}
			if (settings.EnabledVL) {
				RenderImGuiSettingsTree(VLSettings, "Skyrim Settings");
				RenderImGuiSettingsTree(hiddenVRSettings, "hiddenVR");
			}
		}
		ImGui::Spacing();
		ImGui::TreePop();
	}
}

void VolumetricLighting::LoadSettings(json& o_json)
{
	settings = o_json;
	LoadGameSettings(VLSettings);
}

void VolumetricLighting::SaveSettings(json& o_json)
{
	o_json = settings;
	SaveGameSettings(VLSettings);
}

void VolumetricLighting::RestoreDefaultSettings()
{
	settings = {};
	ResetGameSettingsToDefaults(VLSettings);
	if (State::GetSingleton()->isVR) {
		ResetGameSettingsToDefaults(hiddenVRSettings);
	}
}

void VolumetricLighting::DataLoaded()
{
	// if (REL::Module::IsVR() && settings.EnabledVL) {
	// 	EnableBooleanSettings(hiddenVRSettings, GetName());
	// 	enabledAtBoot = true;
	// }
}

void VolumetricLighting::PostPostLoad()
{
	if (REL::Module::IsVR() && settings.EnabledVL) {
		EnableBooleanSettings(hiddenVRSettings, GetName());
	}
}

void VolumetricLighting::Reset()
{
}

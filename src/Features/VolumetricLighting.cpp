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
}

void VolumetricLighting::PostPostLoad()
{
	if (REL::Module::IsVR()) {
		if (settings.EnabledVL)
			EnableBooleanSettings(hiddenVRSettings, GetName());
		auto address = REL::RelocationID(100475, 0).address() + 0x45b;
		logger::info("[{}] Hooking CopyResource at {:x}", GetName(), address);
		REL::safe_fill(address, REL::NOP, 7);
		stl::write_thunk_call<CopyResource, 6>(address);  // AE not needed, VR only hook
	}
}

void VolumetricLighting::Reset()
{
}
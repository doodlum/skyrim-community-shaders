#include "Menu.h"

#include <magic_enum.hpp>

#include "ShaderCache.h"
#include "State.h"
#include "Features/GrassLighting.h"

void SetupImGuiStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Alpha = 0.9f;
}

void Menu::DrawSettings()
{
	ImGuiStyle oldStyle = ImGui::GetStyle();
	SetupImGuiStyle();
	static bool visible = false;
	ImGui::Checkbox("Show Settings", &visible);
	if (visible) {
		ImGui::SetNextWindowSize({ 1024, 1024 }, ImGuiCond_Once);
		ImGui::Begin("Skyrim Community Shaders", &visible);

		auto& shaderCache = SIE::ShaderCache::Instance();

		if (ImGui::Button("Save Settings")) {
			State::GetSingleton()->Save();
		}

		ImGui::SameLine();

		if (ImGui::Button("Load Settings")) {
			State::GetSingleton()->Load();
		}
		ImGui::SameLine();

		if (ImGui::Button("Clear Shader Cache")) {
			shaderCache.Clear();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("The Shader Cache is the collection of compiled shaders which replace the vanilla shaders at runtime.");
			ImGui::Text("Clearing the shader cache will mean that shaders are recompiled only when the game re-encounters them.");
			ImGui::Text("This is only needed for hot - loading shaders for development purposes.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SameLine();

		if (ImGui::Button("Clear Disk Cache")) {
			shaderCache.DeleteDiskCache();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("The Disk Cache is a collection of compiled shaders on disk, which are automatically created when shaders are added to the Shader Cache.");
			ImGui::Text("If you do not have a Disk Cache you will see \"Compiling Shaders\" in the upper-left corner.");
			ImGui::Text("After this has completed you will no longer see this message apart from when loading from the Disk Cache.");
			ImGui::Text("Only delete the Disk Cache manually if you are encountering issues.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::Spacing();

		if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool useCustomShaders = shaderCache.IsEnabled();
			if (ImGui::Checkbox("Enable Shaders", &useCustomShaders)) {
				shaderCache.SetEnabled(useCustomShaders);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::Text("Disabling this effectively disables all features.");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			bool useDiskCache = shaderCache.IsDiskCache();
			if (ImGui::Checkbox("Enable Disk Cache", &useDiskCache)) {
				shaderCache.SetDiskCache(useDiskCache);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::Text("Disabling this stops shaders from being loaded from disk, as well as stops shaders from being saved to it.");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			bool useAsync = shaderCache.IsAsync();
			if (ImGui::Checkbox("Enable Async", &useAsync)) {
				shaderCache.SetAsync(useAsync);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::Text("Skips a shader being replaced if it hasn't been compiled yet. Also makes compilation blazingly fast!");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}

		if (ImGui::CollapsingHeader("Replace Original Shaders", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto state = State::GetSingleton();
			for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
				ImGui::Checkbox(std::format("{}", magic_enum::enum_name((RE::BSShader::Type)(classIndex + 1))).c_str(), &state->enabledClasses[classIndex]);
			}
		}

		if (ImGui::BeginTabBar("Features", ImGuiTabBarFlags_None)) {
			GrassLighting::GetSingleton()->DrawSettings();
		}

		ImGui::End();
	}
	ImGuiStyle& style = ImGui::GetStyle();
	style = oldStyle;
}

void Menu::DrawOverlay()
{
	uint64_t totalShaders = 0;
	uint64_t compiledShaders = 0;

	auto& shaderCache = SIE::ShaderCache::Instance();

	compiledShaders = shaderCache.GetCompletedTasks();
	totalShaders = shaderCache.GetTotalTasks();

	if (compiledShaders != totalShaders) {
		ImGui::SetNextWindowBgAlpha(1);
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		if (!ImGui::Begin("ShaderCompilationInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
			ImGui::End();
			return;
		}

		ImGui::Text("Compiling Shaders: %d / %d", compiledShaders, totalShaders);

		ImGui::End();
	}
}
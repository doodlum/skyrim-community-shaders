#include "Menu.h"

#include <magic_enum.hpp>

#include "ShaderCache.h"


void Menu::DrawSettings()
{
	static bool visible = false;
	ImGui::Checkbox("Show Settings", &visible);
	if (visible) {
		ImGui::SetNextWindowSize({ 512, 1024 }, ImGuiCond_Once);
		ImGui::Begin("Skyrim Community Shaders", &visible);

		auto& shaderCache = SIE::ShaderCache::Instance();

		bool useCustomShaders = shaderCache.IsEnabled();
		if (ImGui::Checkbox("Use Custom Shaders", &useCustomShaders))
		{
			shaderCache.SetEnabled(useCustomShaders);
		}

		bool useDiskCache = shaderCache.IsDiskCache();
		if (ImGui::Checkbox("Use Disk Cache", &useDiskCache))
		{
			shaderCache.SetDiskCache(useDiskCache);
		}

		bool useAsync = shaderCache.IsAsync();
		if (ImGui::Checkbox("Use Async", &useAsync))
		{
			shaderCache.SetAsync(useAsync);
		}

		if (ImGui::Button("Clear Shader Cache"))
		{
			shaderCache.Clear();
		}

		ImGui::SameLine();

		if (ImGui::Button("Delete Disk Cache"))
		{
			shaderCache.DeleteDiskCache();
		}

		if (useCustomShaders)
		{
			for (size_t classIndex = 0; classIndex < static_cast<size_t>(SIE::ShaderClass::Total); ++classIndex)
			{
				const auto shaderClass = static_cast<SIE::ShaderClass>(classIndex);
				bool useCustomClassShaders = shaderCache.IsEnabledForClass(shaderClass);
				if (ImGui::Checkbox(std::format("Use Custom {} Shaders", magic_enum::enum_name(shaderClass))
					.c_str(),
					&useCustomClassShaders))
				{
					shaderCache.SetEnabledForClass(shaderClass, useCustomClassShaders);
				}
			}
		}

		ImGui::End();
	}
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
		if (!ImGui::Begin("ShaderCompilationInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("Compiling Shaders: %d / %d", compiledShaders, totalShaders);

		ImGui::End();
	}
}


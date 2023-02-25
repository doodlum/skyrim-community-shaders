#include "Menu.h"

#include <magic_enum.hpp>

#include "ShaderCache.h"

void Menu::Draw()
{
	static bool visible = false;
	ImGui::Checkbox("Show Settings", &visible);
	if (visible) {
		ImGui::SetNextWindowSize({ 512, 1024 }, ImGuiCond_Once);
		ImGui::Begin("Skyrim Community Shaders", &visible);

		auto& shaderCache = SIE::ShaderCache::Instance();
		bool isAsync = shaderCache.IsAsync();
		if (ImGui::Checkbox("Async Shaders Loading", &isAsync))
		{
			shaderCache.SetAsync(isAsync);
		}

		bool useCustomShaders = shaderCache.IsEnabled();
		if (ImGui::Checkbox("Use Custom Shaders", &useCustomShaders))
		{
			shaderCache.SetEnabled(useCustomShaders);
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

		if (ImGui::Button("Clear Shader Cache"))
		{
			shaderCache.Clear();
		}

		ImGui::End();
	}
}
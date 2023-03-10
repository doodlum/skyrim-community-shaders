#include "State.h"

#include <magic_enum.hpp>

#include "ShaderCache.h"

#include "Features/Clustered.h"
#include "Features/GrassLighting.h"
#include "Features/DistantTreeLighting.h"

void State::Draw()
{
	auto& shaderCache = SIE::ShaderCache::Instance();
	if (shaderCache.IsEnabled() && currentShader) {
		auto type = currentShader->shaderType.get();
		if (type > 0 && type < RE::BSShader::Type::Total) {
			if (enabledClasses[type - 1]) {
				auto context = RE::BSRenderManager::GetSingleton()->GetRuntimeData().context;

				if (auto vertexShader = shaderCache.GetVertexShader(*currentShader, currentVertexDescriptor)) {
					context->VSSetShader(vertexShader->shader, NULL, NULL);
				}

				if (auto pixelShader = shaderCache.GetPixelShader(*currentShader, currentPixelDescriptor)) {
					context->PSSetShader(pixelShader->shader, NULL, NULL);
				}

				GrassLighting::GetSingleton()->Draw(currentShader, currentPixelDescriptor);
				DistantTreeLighting::GetSingleton()->Draw(currentShader, currentPixelDescriptor);
			}
		}
	}

	currentShader = nullptr;
}

void State::Reset()
{
	Clustered::GetSingleton()->Reset();
	GrassLighting::GetSingleton()->Reset();
}

void State::Setup()
{
	GrassLighting::GetSingleton()->SetupResources();
	DistantTreeLighting::GetSingleton()->SetupResources();
}

void State::Load()
{
	auto& shaderCache = SIE::ShaderCache::Instance();
	std::ifstream i(L"Data\\SKSE\\Plugins\\CommunityShaders.json");
	json settings;
	i >> settings;

	if (settings["General"].is_object()) {
		json& general = settings["General"];

		if (general["Enable Shaders"].is_boolean())
			shaderCache.SetEnabled(general["Enable Shaders"]);

		if (general["Enable Disk Cache"].is_boolean())
			shaderCache.SetEnabled(general["Enable Disk Cache"]);

		if (general["Enable Async"].is_boolean())
			shaderCache.SetEnabled(general["Enable Async"]);
	}

	if (settings["Replace Original Shaders"].is_object()) {
		json& originalShaders = settings["Replace Original Shaders"];
		for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
			auto name = magic_enum::enum_name((RE::BSShader::Type)(classIndex + 1));
			if (originalShaders[name].is_boolean()) {
				enabledClasses[classIndex] = originalShaders[name];
			}
		}
	}

	GrassLighting::GetSingleton()->Load(settings);
	DistantTreeLighting::GetSingleton()->Load(settings);
}

void State::Save()
{
	auto& shaderCache = SIE::ShaderCache::Instance();
	std::ofstream o(L"Data\\SKSE\\Plugins\\CommunityShaders.json");
	json settings;

	json general;
	general["Enable Shaders"] = shaderCache.IsEnabled();
	general["Enable Disk Cache"] = shaderCache.IsDiskCache();
	general["Enable Async"] = shaderCache.IsAsync();

	settings["General"] = general;

	json originalShaders;
	for (int classIndex = 0; classIndex < RE::BSShader::Type::Total - 1; ++classIndex) {
		originalShaders[magic_enum::enum_name((RE::BSShader::Type)(classIndex + 1))] = enabledClasses[classIndex];
	}
	settings["Replace Original Shaders"] = originalShaders;

	settings["Version"] = Plugin::VERSION.string();

	GrassLighting::GetSingleton()->Save(settings);
	DistantTreeLighting::GetSingleton()->Save(settings);
	o << settings.dump(1);
}

bool State::ValidateCache(CSimpleIniA& a_ini)
{
	bool valid = true;
	if (!GrassLighting::GetSingleton()->ValidateCache(a_ini)) {
		valid = false;
	}
	if (!DistantTreeLighting::GetSingleton()->ValidateCache(a_ini)) {
		valid = false;
	}
	return valid;
}

void State::WriteDiskCacheInfo(CSimpleIniA& a_ini)
{
	GrassLighting::GetSingleton()->WriteDiskCacheInfo(a_ini);
	DistantTreeLighting::GetSingleton()->WriteDiskCacheInfo(a_ini);
}

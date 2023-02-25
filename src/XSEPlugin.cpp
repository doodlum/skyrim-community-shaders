#include "Hooks.h"
#include "ShaderCache.h"

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "imgui.h"
#include "reshade/reshade.hpp"
#include "Menu.h"

extern "C" DLLEXPORT const char* NAME = "Skyrim Community Shaders";
extern "C" DLLEXPORT const char* DESCRIPTION = "";

HMODULE m_hModule;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID)
{
	if (dwReason == DLL_PROCESS_ATTACH) m_hModule = hModule;
	return TRUE;
}

void DrawSettingsCallback(reshade::api::effect_runtime*)
{
	Menu::GetSingleton()->Draw();
}

namespace D3D11 {
	void PatchD3D11();
}

bool Load()
{
	Hooks::Install();
	D3D11::PatchD3D11();

	SIE::ShaderCache::Instance().SetAsync(true);
	SIE::ShaderCache::Instance().SetEnabled(true);
	SIE::ShaderCache::Instance().SetEnabledForClass(SIE::ShaderClass::Compute, true);
	SIE::ShaderCache::Instance().SetEnabledForClass(SIE::ShaderClass::Pixel, true);
	SIE::ShaderCache::Instance().SetEnabledForClass(SIE::ShaderClass::Vertex, true);
	
	if (reshade::register_addon(m_hModule)) {
		logger::info("Registered addon");
		reshade::register_overlay(nullptr, DrawSettingsCallback);
	}
	else {
		logger::info("ReShade not present");
	}

	return true;
}
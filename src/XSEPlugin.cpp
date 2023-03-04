#include "Hooks.h"
#include "ShaderCache.h"

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "Menu.h"
#include "imgui.h"
#include "reshade/reshade.hpp"
#include "State.h"

extern "C" DLLEXPORT const char* NAME = "Skyrim Community Shaders";
extern "C" DLLEXPORT const char* DESCRIPTION = "";

HMODULE m_hModule;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		m_hModule = hModule;
	return TRUE;
}

void DrawSettingsCallback(reshade::api::effect_runtime*)
{
	Menu::GetSingleton()->DrawSettings();
}

void DrawOverlayCallback(reshade::api::effect_runtime*)
{
	Menu::GetSingleton()->DrawOverlay();
}

void MessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kNewGame:
	case SKSE::MessagingInterface::kPostLoadGame:
		{
			auto& shaderCache = SIE::ShaderCache::Instance();

			while (shaderCache.GetCompletedTasks() != shaderCache.GetTotalTasks()) {
				std::this_thread::sleep_for(100ms);
			}

			if (shaderCache.IsDiskCache()) {
				shaderCache.WriteDiskCacheInfo();
			}

			break;
		}
	}
}

bool Load()
{
	auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener("SKSE", MessageHandler);

	Hooks::Install();

	auto& shaderCache = SIE::ShaderCache::Instance();

	shaderCache.ValidateDiskCache();

	shaderCache.SetEnabled(true);
	shaderCache.SetAsync(true);
	shaderCache.SetDiskCache(true);

	State::GetSingleton()->Load();

	if (reshade::register_addon(m_hModule)) {
		logger::info("Registered addon");
		reshade::register_overlay(nullptr, DrawSettingsCallback);
		reshade::register_event<reshade::addon_event::reshade_overlay>(DrawOverlayCallback);
	} else {
		logger::info("ReShade not present");
	}

	return true;
}
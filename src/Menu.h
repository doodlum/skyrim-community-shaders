#pragma once

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#include "imgui.h"
#include "reshade/reshade.hpp"

class Menu
{
public:
	static Menu* GetSingleton()
	{
		static Menu menu;
		return &menu;
	}

	void DrawSettings();
	void DrawOverlay();
};

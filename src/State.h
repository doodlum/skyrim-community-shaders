#pragma once

#include <RE/BSGraphicsTypes.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class State
{
public:
	static State* GetSingleton()
	{
		static State singleton;
		return &singleton;
	}

	bool enabledClasses[RE::BSShader::Type::Total - 1];

	RE::BSShader* currentShader = nullptr;
	uint32_t currentVertexDescriptor = 0;
	uint32_t currentPixelDescriptor = 0;

	static BSGraphics::BSShaderAccumulator* GetCurrentAccumulator();

	void Draw();
	void Reset();
	void Setup();

	void Load();
	void Save();
};

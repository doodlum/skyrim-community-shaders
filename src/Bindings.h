#pragma once

class Bindings
{
public:
	static Bindings* GetSingleton()
	{
		static Bindings singleton;
		return &singleton;
	}

	bool overrideTerrain = false;

	void DepthStencilStateSetDepthMode(RE::BSGraphics::DepthStencilDepthMode Mode);

	void AlphaBlendStateSetMode(uint32_t Mode);
	void AlphaBlendStateSetAlphaToCoverage(uint32_t Value);
	void AlphaBlendStateSetWriteMode(uint32_t Value);

	void SetOverwriteTerrainMode(bool enable);

	void SetDirtyStates(bool IsComputeShader);
};

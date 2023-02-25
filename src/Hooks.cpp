#include "Hooks.h"

#include "ShaderCache.h"

struct BSShader_BeginTechnique
{
	static bool thunk(RE::BSShader* shader, int vertexDescriptor, int pixelDescriptor,
		bool skipPixelShader)
	{
		static REL::Relocation<void(void*, RE::BSGraphics::VertexShader*)> SetVertexShader(
			RELOCATION_ID(75550, 77351));
		static REL::Relocation<void(void*, RE::BSGraphics::PixelShader*)> SetPixelShader(
			RELOCATION_ID(75555, 77356));

		auto& shaderCache = SIE::ShaderCache::Instance();
		RE::BSGraphics::VertexShader* vertexShader = nullptr;
		if (shaderCache.IsEnabledForClass(SIE::ShaderClass::Vertex))
		{
			vertexShader = shaderCache.GetVertexShader(*shader, vertexDescriptor);
		}
		if (vertexShader == nullptr)
		{
			for (auto item : shader->vertexShaders)
			{
				if (item->id == (uint32_t)vertexDescriptor)
				{
					vertexShader = item;
					break;
				}
			}
		}
		else
		{
			for (auto item : shader->vertexShaders)
			{
				if (item->id == (uint32_t)vertexDescriptor)
				{
					if (vertexShader->shaderDesc != item->shaderDesc)
					{
						logger::info("{} {}", vertexShader->shaderDesc, item->shaderDesc);
					}
					break;
				}
			}
		}
		RE::BSGraphics::PixelShader* pixelShader = nullptr;
		if (shaderCache.IsEnabledForClass(SIE::ShaderClass::Pixel))
		{
			pixelShader = shaderCache.GetPixelShader(*shader, pixelDescriptor);
		}
		if (pixelShader == nullptr)
		{
			for (auto item : shader->pixelShaders)
			{
				if (item->id == (uint32_t)pixelDescriptor)
				{
					pixelShader = item;
					break;
				}
			}
		}
		/*else
		{
			for (auto item : shader->pixelShaders)
			{
				if (item->id == pixelDescriptor)
				{
					pixelShader->constantBuffers[0] = item->constantBuffers[0];
					pixelShader->constantBuffers[1] = item->constantBuffers[1];
					pixelShader->constantBuffers[2] = item->constantBuffers[2];
					break;
				}
			}
		}*/
		if (vertexShader == nullptr || pixelShader == nullptr)
		{
			return false;
		}
		SetVertexShader(nullptr, vertexShader);
		if (skipPixelShader)
		{
			pixelShader = nullptr;
		}
		SetPixelShader(nullptr, pixelShader);
		return true;
	}

	static inline REL::Relocation<decltype(thunk)> func;
};

namespace Hooks
{
	void Install()
	{
		const std::array targets{
			REL::Relocation<std::uintptr_t>(RELOCATION_ID(101341, 108328), 0),
		};
		for (const auto& target : targets)
		{
			stl::write_thunk_jmp<BSShader_BeginTechnique>(target.address());
		}
	}

}

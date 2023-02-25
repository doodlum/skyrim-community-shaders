#pragma comment(lib, "d3d11.lib")
#include <d3d11.h>

#include <detours/Detours.h>

#include "RE/BSGraphicsTypes.h"
#include "Util.h"

namespace D3D11
{
	decltype(&IDXGISwapChain::Present) ptrPresent;

	decltype(&ID3D11DeviceContext::DrawIndexed)          ptrDrawIndexed;
	decltype(&ID3D11DeviceContext::DrawIndexedInstanced) ptrDrawIndexedInstanced;

	HRESULT WINAPI hk_IDXGISwapChain_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
	{
		return (This->*ptrPresent)(SyncInterval, Flags);
	}

	void hk_ID3D11DeviceContext_DrawIndexed(ID3D11DeviceContext* This, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
	{
		(This->*ptrDrawIndexed)(IndexCount, StartIndexLocation, BaseVertexLocation);
	}

	void hk_ID3D11DeviceContext_DrawIndexedInstanced(ID3D11DeviceContext* This, UINT IndexCountPerInstance,
		UINT InstanceCount,
		UINT StartIndexLocation,
		INT  BaseVertexLocation,
		UINT StartInstanceLocation)
	{
		(This->*ptrDrawIndexedInstanced)(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	void AddTextureDebugInformation()
	{
		auto r = BSGraphics::Renderer::QInstance();
		for (int i = 0; i < RenderTargets::RENDER_TARGET_COUNT; i++) {
			auto rt = r->pRenderTargets[i];
			auto name = RTNames[i];
			if (rt.RTV) {
				Util::SetResourceName(rt.RTV, "%s RTV", name);
			}
			if (rt.SRV) {
				Util::SetResourceName(rt.SRV, "%s SRV", name);
			}
			if (rt.SRVCopy) {
				Util::SetResourceName(rt.SRVCopy, "%s COPY SRV", name);
			}
			if (rt.Texture) {
				Util::SetResourceName(rt.Texture, "%s TEXTURE", name);
			}
		}
		for (int i = 0; i < RenderTargetsCubemaps::RENDER_TARGET_CUBEMAP_COUNT; i++) {
			auto rt = r->pCubemapRenderTargets[i];
			auto name = RTCubemapNames[i];
			if (rt.SRV) {
				Util::SetResourceName(rt.SRV, "%s SRV", name);
			}
			for (int k = 0; k < 6; k++) {
				if (rt.CubeSideRTV[k]) {
					Util::SetResourceName(rt.CubeSideRTV[k], "%s SIDE SRV", name);
				}
			}
			if (rt.Texture) {
				Util::SetResourceName(rt.Texture, "%s TEXTURE", name);
			}
		}

		for (int i = 0; i < RenderTargetsDepthStencil::DEPTH_STENCIL_COUNT; i++) {
			auto rt = r->pDepthStencils[i];
			auto name = DepthNames[i];
			if (rt.DepthSRV) {
				Util::SetResourceName(rt.DepthSRV, "%s DEPTH SRV", name);
			}
			if (rt.StencilSRV) {
				Util::SetResourceName(rt.StencilSRV, "%s STENCIL SRV", name);
			}
			for (int k = 0; k < 8; k++) {
				if (rt.Views[k]) {
					Util::SetResourceName(rt.Views[k], "%s VIEWS SRV", name);
				}
				if (rt.ReadOnlyViews[k]) {
					Util::SetResourceName(rt.Views[k], "%s READONLY VIEWS SRV", name);
				}
			}
			if (rt.Texture) {
				Util::SetResourceName(rt.Texture, "%s TEXTURE", name);
			}
		}
	}

	typedef void (*LoadShaders_t)(BSGraphics::BSShader* shader, std::uintptr_t stream);
	LoadShaders_t oLoadShaders;
	bool          loadinit = false;

	void hk_LoadShaders(BSGraphics::BSShader* bsShader, std::uintptr_t stream)
	{
		oLoadShaders(bsShader, stream);
		if (!loadinit) {
			AddTextureDebugInformation();
			loadinit = true;
		}

		for (const auto& entry : bsShader->m_PixelShaderTable) {
			Util::SetResourceName(entry->m_Shader, "%s %u PS", bsShader->m_LoaderType, entry->m_TechniqueID);
		}
		for (const auto& entry : bsShader->m_VertexShaderTable) {
			Util::SetResourceName(entry->m_Shader, "%s %u VS", bsShader->m_LoaderType, entry->m_TechniqueID);
		}
	};

	uintptr_t LoadShaders;

	void InstallLoadShaders()
	{
		logger::info("Installing BSShader::LoadShaders hook");
		{
			LoadShaders = RELOCATION_ID(101339, 108326).address();
			struct Patch : Xbyak::CodeGenerator
			{
				Patch()
				{
					Xbyak::Label origFuncJzLabel;

					test(rdx, rdx);
					jz(origFuncJzLabel);
					jmp(ptr[rip]);
					dq(LoadShaders + 0x9);

					L(origFuncJzLabel);
					jmp(ptr[rip]);
					dq(LoadShaders + 0xF0);
				}
			};

			Patch p;
			p.ready();

			SKSE::AllocTrampoline(1 << 6);

			auto& trampoline = SKSE::GetTrampoline();
			oLoadShaders = static_cast<LoadShaders_t>(trampoline.allocate(p));
			trampoline.write_branch<6>(
				LoadShaders,
				hk_LoadShaders);
		}
		logger::info("Installed");
	}

	struct Hooks
	{
		struct BSGraphics_Renderer_Init_InitD3D
		{
			static void thunk()
			{
				logger::info("Calling original Init3D");

				func();

				logger::info("Accessing render device information");

				auto manager = RE::BSRenderManager::GetSingleton();

				auto context = manager->GetRuntimeData().context;
				auto swapchain = manager->GetRuntimeData().swapChain;

				logger::info("Detouring virtual function tables");

				*(uintptr_t*)&ptrPresent = Detours::X64::DetourClassVTable(*(uintptr_t*)swapchain, &hk_IDXGISwapChain_Present, 8);
				*(uintptr_t*)&ptrDrawIndexed = Detours::X64::DetourClassVTable(*(uintptr_t*)context, &hk_ID3D11DeviceContext_DrawIndexed, 12);
				*(uintptr_t*)&ptrDrawIndexedInstanced = Detours::X64::DetourClassVTable(*(uintptr_t*)context, &hk_ID3D11DeviceContext_DrawIndexedInstanced, 20);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<BSGraphics_Renderer_Init_InitD3D>(REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC));
			InstallLoadShaders();
			logger::info("Installed render startup hooks");
		}
	};

	void PatchD3D11()
	{
		Hooks::Install();
	}
}
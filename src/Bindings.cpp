#include "Bindings.h"

void Bindings::DepthStencilStateSetDepthMode(RE::BSGraphics::DepthStencilDepthMode Mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().depthStencilDepthMode != Mode) {
		state->GetRuntimeData().depthStencilDepthMode = Mode;
		if (state->GetRuntimeData().depthStencilDepthModePrevious != Mode)
			state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		else
			state->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
	}
}

void Bindings::AlphaBlendStateSetMode(uint32_t Mode)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendMode != Mode) {
		state->GetRuntimeData().alphaBlendMode = Mode;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetAlphaToCoverage(uint32_t Value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendAlphaToCoverage != Value) {
		state->GetRuntimeData().alphaBlendAlphaToCoverage = Value;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::AlphaBlendStateSetWriteMode(uint32_t Value)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (state->GetRuntimeData().alphaBlendWriteMode != Value) {
		state->GetRuntimeData().alphaBlendWriteMode = Value;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

void Bindings::SetOverwriteTerrainMode(bool enable)
{
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();

	if (overrideTerrain != enable) {
		overrideTerrain = enable;
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
		state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
	}
}

// Not an actual class
class HACK_Globals
{
public:
	float m_PreviousClearColor[4];  // Render target clear color
	void* qword_14304BF00;          // RendererData *
	ID3D11Device* m_Device;
	HWND m_Window;

	//
	// These are pools for efficient data uploads to the GPU. Each frame can use any buffer as long as there
	// is sufficient space. If there's no space left, delay execution until m_DynamicVertexBufferAvailQuery[] says a buffer
	// is no longer in use.
	//
	ID3D11Buffer* m_DynamicVertexBuffers[3];  // DYNAMIC (VERTEX | INDEX) CPU_ACCESS_WRITE
	uint32_t m_CurrentDynamicVertexBuffer;

	uint32_t m_CurrentDynamicVertexBufferOffset;  // Used in relation with m_DynamicVertexBufferAvailQuery[]
	ID3D11Buffer* m_SharedParticleIndexBuffer;    // DEFAULT INDEX CPU_ACCESS_NONE
	ID3D11Buffer* m_SharedParticleStaticBuffer;   // DEFAULT VERTEX CPU_ACCESS_NONE
	ID3D11InputLayout* m_ParticleShaderInputLayout;
	ID3D11InputLayout* m_UnknownInputLayout2;
	uint32_t m_UnknownCounter;   // 0 to 63
	uint32_t m_UnknownCounter2;  // No limits
	void* m_UnknownStaticBuffer[64];
	uint32_t m_UnknownCounter3;  // 0 to 5
	bool m_DynamicEventQueryFinished[3];
	ID3D11Query* m_DynamicVertexBufferAvailQuery[3];  // D3D11_QUERY_EVENT (Waits for a series of commands to finish execution)

	float m_DepthBiasFactors[3][4];

	ID3D11DepthStencilState* m_DepthStates[6][40];       // OMSetDepthStencilState
	ID3D11RasterizerState* m_RasterStates[2][3][12][2];  // RSSetState
	ID3D11BlendState* m_BlendStates[7][2][13][2];        // OMSetBlendState
	ID3D11SamplerState* m_SamplerStates[6][5];           // Samplers[AddressMode][FilterMode] (Used for PS and CS)

	//
	// Vertex/Pixel shader constant buffers. Set during load-time (CreateShaderBundle).
	//
	uint32_t m_NextConstantBufferIndex;
	ID3D11Buffer* m_ConstantBuffers1[4];   // Sizes: 3840 bytes
	ID3D11Buffer* m_AlphaTestRefCB;        // CONSTANT_GROUP_LEVEL_ALPHA_TEST_REF (Index 11) - 16 bytes
	ID3D11Buffer* m_ConstantBuffers2[20];  // Sizes: 0, 16, 32, 48, ... 304 bytes
	ID3D11Buffer* m_ConstantBuffers3[10];  // Sizes: 0, 16, 32, 48, ... 144 bytes
	ID3D11Buffer* m_ConstantBuffers4[28];  // Sizes: 0, 16, 32, 48, ... 432 bytes
	ID3D11Buffer* m_ConstantBuffers5[20];  // Sizes: 0, 16, 32, 48, ... 304 bytes
	ID3D11Buffer* m_ConstantBuffers6[20];  // Sizes: 0, 16, 32, 48, ... 304 bytes
	ID3D11Buffer* m_ConstantBuffers7[40];  // Sizes: 0, 16, 32, 48, ... 624 bytes
	ID3D11Buffer* m_TempConstantBuffer2;   // 576 bytes
	ID3D11Buffer* m_TempConstantBuffer3;   // CONSTANT_GROUP_LEVEL_PERFRAME (Index 12) - 720 bytes
	ID3D11Buffer* m_TempConstantBuffer4;   // 16 bytes

	IDXGIOutput* m_DXGIAdapterOutput;
	ID3D11DeviceContext* m_DeviceContext;

	void* m_FrameDurationStringHandle;  // "Frame Duration" but stored in their global string pool
};

#define AutoPtr(Type, Name, Offset) static Type& Name = (*(Type*)(Offset))

// Reimplementation of the renderer's bindings system with additional features

void Bindings::SetDirtyStates(bool IsComputeShader)
{
	AutoPtr(HACK_Globals, Globals, REL::RelocationID(524724, 411387).address());

	auto rendererData = RE::BSGraphics::Renderer::GetSingleton();
	auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
	auto context = rendererData->GetRuntimeData().context;

	if (state->GetRuntimeData().stateUpdateFlags.get() != 0) {
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET)) {
			// Build active render target view array
			ID3D11RenderTargetView* renderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			uint32_t viewCount = 0;

			if (state->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kNONE) {
				// This loops through all 8 RTs or until a RENDER_TARGET_NONE entry is hit
				for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
					if (state->GetRuntimeData().renderTargets[i] == RE::RENDER_TARGETS::kNONE)
						break;

					renderTargetViews[i] = rendererData->GetRuntimeData().renderTargets[state->GetRuntimeData().renderTargets[i]].RTV;
					viewCount++;

					if (state->GetRuntimeData().setRenderTargetMode[i] == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
						context->ClearRenderTargetView(renderTargetViews[i], rendererData->GetRendererData().clearColor);
						state->GetRuntimeData().setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
					}
				}
			} else {
				// Use a single RT for the cubemap
				renderTargetViews[0] = rendererData->GetRendererData().cubemapRenderTargets[state->GetRuntimeData().cubeMapRenderTarget].cubeSideRTV[state->GetRuntimeData().cubeMapRenderTargetView];
				viewCount = 1;

				if (state->GetRuntimeData().setCubeMapRenderTargetMode == RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR) {
					context->ClearRenderTargetView(renderTargetViews[0], rendererData->GetRendererData().clearColor);
					state->GetRuntimeData().setCubeMapRenderTargetMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
				}
			}

			switch (state->GetRuntimeData().setDepthStencilMode) {
			case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR:
			case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_DEPTH:
			case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_STENCIL:
			case RE::BSGraphics::SetRenderTargetMode::SRTM_INIT:
				rendererData->GetRuntimeData().readOnlyDepth = false;
				break;
			}

			//
			// Determine which depth stencil to render to. When there's no active depth stencil,
			// simply send a nullptr to dx11.
			//
			ID3D11DepthStencilView* depthStencil = nullptr;

			if (state->GetRuntimeData().depthStencil != -1) {
				if (rendererData->GetRuntimeData().readOnlyDepth)
					depthStencil = rendererData->GetDepthStencilData().depthStencils[state->GetRuntimeData().depthStencil].readOnlyViews[state->GetRuntimeData().depthStencilSlice];
				else
					depthStencil = rendererData->GetDepthStencilData().depthStencils[state->GetRuntimeData().depthStencil].views[state->GetRuntimeData().depthStencilSlice];

				// Only clear the stencil if specific flags are set
				if (depthStencil) {
					uint32_t clearFlags = 0;

					switch (state->GetRuntimeData().setDepthStencilMode) {
					case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR:
					case RE::BSGraphics::SetRenderTargetMode::SRTM_INIT:
						clearFlags = D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL;
						break;

					case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_DEPTH:
						clearFlags = D3D11_CLEAR_DEPTH;
						break;

					case RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR_STENCIL:
						clearFlags = D3D11_CLEAR_STENCIL;
						break;
					}

					if (clearFlags) {
						context->ClearDepthStencilView(depthStencil, clearFlags, 1.0f, 0);
						state->GetRuntimeData().setDepthStencilMode = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
					}
				}
			}

			context->OMSetRenderTargets(viewCount, renderTargetViews, depthStencil);
		}

		// OMSetDepthStencilState
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE)) {
			if (overrideTerrain)
				context->OMSetDepthStencilState(Globals.m_DepthStates[(uint32_t)RE::BSGraphics::DepthStencilDepthMode::kTestGreaterEqual][state->GetRuntimeData().depthStencilStencilMode], state->GetRuntimeData().stencilRef);
			else
				context->OMSetDepthStencilState(Globals.m_DepthStates[(uint32_t)state->GetRuntimeData().depthStencilDepthMode][state->GetRuntimeData().depthStencilStencilMode], state->GetRuntimeData().stencilRef);
		}

		// RSSetState
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN2, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_DEPTH_BIAS, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_CULL_MODE, RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN1)) {
			context->RSSetState(Globals.m_RasterStates[state->GetRuntimeData().rasterStateFillMode][state->GetRuntimeData().rasterStateCullMode][state->GetRuntimeData().rasterStateDepthBiasMode][state->GetRuntimeData().rasterStateScissorMode]);

			if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_RASTER_DEPTH_BIAS)) {
				if (state->GetRuntimeData().viewPort.MinDepth != state->GetRuntimeData().cameraData.getEye().viewDepthRange.x || state->GetRuntimeData().viewPort.MaxDepth != state->GetRuntimeData().cameraData.getEye().viewDepthRange.y) {
					state->GetRuntimeData().viewPort.MinDepth = state->GetRuntimeData().cameraData.getEye().viewDepthRange.x;
					state->GetRuntimeData().viewPort.MaxDepth = state->GetRuntimeData().cameraData.getEye().viewDepthRange.y;
					state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT);
				}

				if (state->GetRuntimeData().rasterStateDepthBiasMode) {
					state->GetRuntimeData().viewPort.MaxDepth -= Globals.m_DepthBiasFactors[0][state->GetRuntimeData().rasterStateDepthBiasMode];
					state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT);
				}
			}
		}

		// RSSetViewports
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT)) {
			context->RSSetViewports(1, &state->GetRuntimeData().viewPort);
		}

		// OMSetBlendState
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND)) {
			const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			if (overrideTerrain)
				context->OMSetBlendState(Globals.m_BlendStates[1][state->GetRuntimeData().alphaBlendAlphaToCoverage][state->GetRuntimeData().alphaBlendWriteMode][state->GetRuntimeData().alphaBlendModeExtra], blendFactor, 0xFFFFFFFF);
			else
				context->OMSetBlendState(Globals.m_BlendStates[state->GetRuntimeData().alphaBlendMode][state->GetRuntimeData().alphaBlendAlphaToCoverage][state->GetRuntimeData().alphaBlendWriteMode][state->GetRuntimeData().alphaBlendModeExtra], blendFactor, 0xFFFFFFFF);
		}

		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF)) {
			D3D11_MAPPED_SUBRESOURCE resource;
			context->Map(Globals.m_AlphaTestRefCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);

			if (state->GetRuntimeData().alphaTestEnabled)
				*(float*)resource.pData = state->GetRuntimeData().alphaTestRef;
			else
				*(float*)resource.pData = 0.0f;

			context->Unmap(Globals.m_AlphaTestRefCB, 0);
		}

		// Shader input layout creation + updates
		//if (!IsComputeShader && state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_VERTEX_DESC))
		//{
		//	// Exists in original function
		//}

		// IASetPrimitiveTopology
		if (state->GetRuntimeData().stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_PRIMITIVE_TOPO)) {
			context->IASetPrimitiveTopology(state->GetRuntimeData().topology);
		}

		// Compute shaders are pretty much custom and always require state to be reset
		if (IsComputeShader) {
			state->GetRuntimeData().stateUpdateFlags.reset(
				RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
				RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT,
				RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN1, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_CULL_MODE, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_DEPTH_BIAS,
				RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST, RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN2);

			state->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_PRIMITIVE_TOPO);

		} else {
			state->GetRuntimeData().stateUpdateFlags.reset(
				RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
				RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT,
				RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE, RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE, RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN1, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_CULL_MODE, RE::BSGraphics::ShaderFlags::DIRTY_RASTER_DEPTH_BIAS,
				RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST, RE::BSGraphics::ShaderFlags::DIRTY_PRIMITIVE_TOPO, RE::BSGraphics::ShaderFlags::DIRTY_UNKNOWN2);
		}
	}
}
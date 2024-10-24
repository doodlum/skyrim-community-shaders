#include "Deferred.h"

#include "ShaderCache.h"
#include "State.h"
#include "TruePBR.h"
#include "Util.h"

#include "Features/DynamicCubemaps.h"
#include "Features/ScreenSpaceGI.h"
#include "Features/Skylighting.h"
#include "Features/SubsurfaceScattering.h"
#include "Features/TerrainBlending.h"

struct DepthStates
{
	ID3D11DepthStencilState* a[6][40];
};

struct BlendStates
{
	ID3D11BlendState* a[7][2][13][2];

	static BlendStates* GetSingleton()
	{
		static auto blendStates = reinterpret_cast<BlendStates*>(REL::RelocationID(524749, 411364).address());
		return blendStates;
	}

	static std::array<ID3D11BlendState**, 6> GetBlendStates()
	{
		auto blendStates = GetSingleton();
		return {
			&blendStates->a[0][0][1][0],
			&blendStates->a[0][0][10][0],
			&blendStates->a[1][0][1][0],
			&blendStates->a[1][0][11][0],
			&blendStates->a[2][0][1][0],
			&blendStates->a[3][0][11][0]
		};
	}
};

void SetupRenderTarget(RE::RENDER_TARGET target, D3D11_TEXTURE2D_DESC texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc, D3D11_RENDER_TARGET_VIEW_DESC rtvDesc, D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc, DXGI_FORMAT format)
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& device = State::GetSingleton()->device;

	texDesc.Format = format;
	srvDesc.Format = format;
	rtvDesc.Format = format;
	uavDesc.Format = format;

	auto& data = renderer->GetRuntimeData().renderTargets[target];
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, nullptr, &data.texture));
	DX::ThrowIfFailed(device->CreateShaderResourceView(data.texture, &srvDesc, &data.SRV));
	DX::ThrowIfFailed(device->CreateRenderTargetView(data.texture, &rtvDesc, &data.RTV));
	DX::ThrowIfFailed(device->CreateUnorderedAccessView(data.texture, &uavDesc, &data.UAV));
}

void Deferred::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	{
		auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

		D3D11_TEXTURE2D_DESC texDesc{};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		main.texture->GetDesc(&texDesc);
		main.SRV->GetDesc(&srvDesc);
		main.RTV->GetDesc(&rtvDesc);
		main.UAV->GetDesc(&uavDesc);

		// Available targets:
		// MAIN ONLY ALPHA
		// WATER REFLECTIONS
		// BLURFULL_BUFFER
		// LENSFLAREVIS
		// SAO DOWNSCALED
		// SAO CAMERAZ+MIP_LEVEL_0_ESRAM
		// SAO_RAWAO_DOWNSCALED
		// SAO_RAWAO_PREVIOUS_DOWNSCALDE
		// SAO_TEMP_BLUR_DOWNSCALED
		// INDIRECT
		// INDIRECT_DOWNSCALED
		// RAWINDIRECT
		// RAWINDIRECT_DOWNSCALED
		// RAWINDIRECT_PREVIOUS
		// RAWINDIRECT_PREVIOUS_DOWNSCALED
		// RAWINDIRECT_SWAP
		// VOLUMETRIC_LIGHTING_HALF_RES
		// VOLUMETRIC_LIGHTING_BLUR_HALF_RES
		// VOLUMETRIC_LIGHTING_QUARTER_RES
		// VOLUMETRIC_LIGHTING_BLUR_QUARTER_RES
		// TEMPORAL_AA_WATER_1
		// TEMPORAL_AA_WATER_2

		// Albedo
		SetupRenderTarget(ALBEDO, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Specular
		SetupRenderTarget(SPECULAR, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R11G11B10_FLOAT);
		// Reflectance
		SetupRenderTarget(REFLECTANCE, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Normal + Roughness
		SetupRenderTarget(NORMALROUGHNESS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Masks
		SetupRenderTarget(MASKS, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
		// Additional Masks
		SetupRenderTarget(MASKS2, texDesc, srvDesc, rtvDesc, uavDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
	}

	{
		auto& device = State::GetSingleton()->device;

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &linearSampler));
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags = 0;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;

		std::uint32_t numElements = 1;

		sbDesc.StructureByteStride = sizeof(PerGeometry);
		sbDesc.ByteWidth = sizeof(PerGeometry) * numElements;
		perShadow = new Buffer(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		perShadow->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		perShadow->CreateUAV(uavDesc);

		copyShadowCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\ShadowTest\\CopyShadowData.hlsl", {}, "cs_5_0"));
	}

	{
		D3D11_TEXTURE2D_DESC texDesc;
		auto mainTex = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];
		mainTex.texture->GetDesc(&texDesc);

		texDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1 }
		};
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = texDesc.Format,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
			.Texture2D = { .MipSlice = 0 }
		};

		prevDiffuseAmbientTexture = new Texture2D(texDesc);
		prevDiffuseAmbientTexture->CreateSRV(srvDesc);
		prevDiffuseAmbientTexture->CreateUAV(uavDesc);
	}
}

void Deferred::CopyShadowData()
{
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "CopyShadowData");

	auto& context = State::GetSingleton()->context;

	ID3D11UnorderedAccessView* uavs[1]{ perShadow->uav.get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	ID3D11Buffer* buffers[3];
	context->PSGetConstantBuffers(0, 3, buffers);
	context->PSGetConstantBuffers(12, 1, buffers + 1);

	context->CSSetConstantBuffers(0, 3, buffers);

	context->CSSetShader(copyShadowCS, nullptr, 0);

	context->Dispatch(1, 1, 1);

	uavs[0] = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	std::fill(buffers, buffers + ARRAYSIZE(buffers), nullptr);
	context->CSSetConstantBuffers(0, 3, buffers);

	context->CSSetShader(nullptr, nullptr, 0);

	{
		context->PSGetShaderResources(4, 1, &shadowView);

		ID3D11ShaderResourceView* srvs[2]{
			shadowView,
			perShadow->srv.get(),
		};

		context->PSSetShaderResources(25, ARRAYSIZE(srvs), srvs);
	}
}

void Deferred::PrepassPasses()
{
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Prepass");

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;
	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	TruePBR::GetSingleton()->PrePass();
	for (auto* feature : Feature::GetFeatureList()) {
		if (feature->loaded) {
			feature->Prepass();
		}
	}
}

void Deferred::StartDeferred()
{
	if (!inWorld)
		return;

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	State::GetSingleton()->UpdateSharedData();

	static std::once_flag setup;
	std::call_once(setup, [&]() {
		auto& device = State::GetSingleton()->device;

		auto blendStates = BlendStates::GetBlendStates();

		for (int i = 0; i < blendStates.size(); ++i) {
			forwardBlendStates[i] = *blendStates[i];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[i]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[i]));
		}
	});

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(setRenderTargetMode, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// Backup original render targets
	for (uint i = 0; i < 4; i++) {
		forwardRenderTargets[i] = renderTargets[i];
	}

	RE::RENDER_TARGET targets[8]{
		RE::RENDER_TARGET::kMAIN,
		RE::RENDER_TARGET::kMOTION_VECTOR,
		NORMALROUGHNESS,
		ALBEDO,
		SPECULAR,
		REFLECTANCE,
		MASKS,
		MASKS2
	};

	for (uint i = 2; i < 8; i++) {
		renderTargets[i] = targets[i];                                             // We must use unused targets to be indexable
		setRenderTargetMode[i] = RE::BSGraphics::SetRenderTargetMode::SRTM_CLEAR;  // Dirty from last frame, this calls ClearRenderTargetView once
	}

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	deferredPass = true;

	{
		auto& context = State::GetSingleton()->context;

		static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };
		ID3D11Buffer* buffers[1] = { *perFrame.get() };

		ID3D11Buffer* vrBuffer = nullptr;

		if (REL::Module::IsVR()) {
			static REL::Relocation<ID3D11Buffer**> VRValues{ REL::Offset(0x3180688) };
			vrBuffer = *VRValues.get();
		}
		if (vrBuffer) {
			context->CSSetConstantBuffers(12, 1, buffers);
			context->CSSetConstantBuffers(13, 1, &vrBuffer);
		} else {
			context->CSSetConstantBuffers(12, 1, buffers);
		}
	}

	PrepassPasses();
}

void Deferred::DeferredPasses()
{
	ZoneScoped;
	TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Deferred");

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto& context = State::GetSingleton()->context;

	{
		static REL::Relocation<ID3D11Buffer**> perFrame{ REL::RelocationID(524768, 411384) };
		ID3D11Buffer* buffers[1] = { *perFrame.get() };
		ID3D11Buffer* vrBuffer = nullptr;

		if (REL::Module::IsVR()) {
			static REL::Relocation<ID3D11Buffer**> VRValues{ REL::Offset(0x3180688) };
			vrBuffer = *VRValues.get();
		}
		if (vrBuffer) {
			context->CSSetConstantBuffers(12, 1, buffers);
			context->CSSetConstantBuffers(13, 1, &vrBuffer);
		} else {
			context->CSSetConstantBuffers(12, 1, buffers);
		}
	}

	auto specular = renderer->GetRuntimeData().renderTargets[SPECULAR];
	auto albedo = renderer->GetRuntimeData().renderTargets[ALBEDO];
	auto normalRoughness = renderer->GetRuntimeData().renderTargets[NORMALROUGHNESS];
	auto masks = renderer->GetRuntimeData().renderTargets[MASKS];
	auto masks2 = renderer->GetRuntimeData().renderTargets[MASKS2];

	auto main = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[0]];
	auto normals = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[2]];
	auto snow = renderer->GetRuntimeData().renderTargets[forwardRenderTargets[3]];

	auto depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto reflectance = renderer->GetRuntimeData().renderTargets[REFLECTANCE];

	bool interior = true;
	if (auto sky = RE::Sky::GetSingleton())
		interior = sky->mode.get() != RE::Sky::Mode::kFull;

	auto skylighting = Skylighting::GetSingleton();

	auto ssgi = ScreenSpaceGI::GetSingleton();

	auto dispatchCount = Util::GetScreenDispatchCount();

	if (ssgi->loaded) {
		ssgi->DrawSSGI(prevDiffuseAmbientTexture);

		// Ambient Composite
		{
			TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Ambient Composite");

			ID3D11ShaderResourceView* srvs[6]{
				albedo.SRV,
				normalRoughness.SRV,
				skylighting->loaded || REL::Module::IsVR() ? depth.depthSRV : nullptr,
				skylighting->loaded ? skylighting->texProbeArray->srv.get() : nullptr,
				ssgi->settings.Enabled ? ssgi->texGI[ssgi->outputGIIdx]->srv.get() : nullptr,
				masks2.SRV,
			};

			context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

			ID3D11UnorderedAccessView* uavs[2]{ main.UAV, prevDiffuseAmbientTexture->uav.get() };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			auto shader = interior ? GetComputeAmbientCompositeInterior() : GetComputeAmbientComposite();
			context->CSSetShader(shader, nullptr, 0);

			context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
		}

		// Clear
		{
			ID3D11ShaderResourceView* views[6]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
			context->CSSetShaderResources(0, ARRAYSIZE(views), views);

			ID3D11UnorderedAccessView* uavs[2]{ nullptr, nullptr };
			context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

			context->CSSetShader(nullptr, nullptr, 0);
		}
	}

	auto sss = SubsurfaceScattering::GetSingleton();
	if (sss->loaded)
		sss->DrawSSS();

	auto dynamicCubemaps = DynamicCubemaps::GetSingleton();
	if (dynamicCubemaps->loaded)
		dynamicCubemaps->UpdateCubemap();

	auto terrainBlending = TerrainBlending::GetSingleton();

	// Deferred Composite
	{
		TracyD3D11Zone(State::GetSingleton()->tracyCtx, "Deferred Composite");

		bool doSSGISpecular = ssgi->loaded && ssgi->settings.Enabled && ssgi->settings.EnableGI && ssgi->settings.EnableSpecularGI;

		ID3D11ShaderResourceView* srvs[11]{
			specular.SRV,
			albedo.SRV,
			normalRoughness.SRV,
			masks.SRV,
			masks2.SRV,
			dynamicCubemaps->loaded || REL::Module::IsVR() ? (terrainBlending->loaded ? terrainBlending->blendedDepthTexture16->srv.get() : depth.depthSRV) : nullptr,
			dynamicCubemaps->loaded ? reflectance.SRV : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded ? dynamicCubemaps->envReflectionsTexture->srv.get() : nullptr,
			dynamicCubemaps->loaded && skylighting->loaded ? skylighting->texProbeArray->srv.get() : nullptr,
			doSSGISpecular ? ssgi->texGISpecular[ssgi->outputGIIdx]->srv.get() : nullptr,
		};

		if (dynamicCubemaps->loaded)
			context->CSSetSamplers(0, 1, &linearSampler);

		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

		ID3D11UnorderedAccessView* uavs[3]{ main.UAV, normals.UAV, snow.UAV };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		auto shader = interior ? GetComputeMainCompositeInterior() : GetComputeMainComposite();
		context->CSSetShader(shader, nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);
	}

	// Clear
	{
		ID3D11ShaderResourceView* views[10]{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[3]{ nullptr, nullptr, nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11Buffer* buffers[1] = { nullptr };
		context->CSSetConstantBuffers(12, 1, buffers);

		context->CSSetShader(nullptr, nullptr, 0);
	}

	if (dynamicCubemaps->loaded)
		dynamicCubemaps->PostDeferred();
}

void Deferred::EndDeferred()
{
	if (!inWorld)
		return;

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (!shaderCache.IsEnabled())
		return;

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(renderTargets, shadowState)
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	// Do not render to our targets past this point
	for (uint i = 0; i < 4; i++) {
		renderTargets[i] = forwardRenderTargets[i];
	}

	for (uint i = 4; i < 8; i++) {
		renderTargets[i] = RE::RENDER_TARGET::kNONE;
	}

	auto& context = State::GetSingleton()->context;
	context->OMSetRenderTargets(0, nullptr, nullptr);  // Unbind all bound render targets

	DeferredPasses();  // Perform deferred passes and composite forward buffers

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);  // Run OMSetRenderTargets again

	deferredPass = false;
}

void Deferred::OverrideBlendStates()
{
	auto blendStates = BlendStates::GetBlendStates();

	static std::once_flag setup;
	std::call_once(setup, [&]() {
		auto& device = State::GetSingleton()->device;

		for (int i = 0; i < blendStates.size(); ++i) {
			forwardBlendStates[i] = *blendStates[i];

			D3D11_BLEND_DESC blendDesc;
			forwardBlendStates[i]->GetDesc(&blendDesc);

			blendDesc.IndependentBlendEnable = false;

			DX::ThrowIfFailed(device->CreateBlendState(&blendDesc, &deferredBlendStates[i]));
		}
	});

	// Set modified blend states
	for (int i = 0; i < blendStates.size(); ++i)
		*blendStates[i] = deferredBlendStates[i];

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

void Deferred::ResetBlendStates()
{
	auto blendStates = BlendStates::GetBlendStates();

	// Restore modified blend states
	for (int i = 0; i < blendStates.size(); ++i)
		*blendStates[i] = forwardBlendStates[i];

	auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
	GET_INSTANCE_MEMBER(stateUpdateFlags, shadowState)

	stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
}

void Deferred::ClearShaderCache()
{
	if (ambientCompositeCS) {
		ambientCompositeCS->Release();
		ambientCompositeCS = nullptr;
	}
	if (ambientCompositeInteriorCS) {
		ambientCompositeInteriorCS->Release();
		ambientCompositeInteriorCS = nullptr;
	}
	if (mainCompositeCS) {
		mainCompositeCS->Release();
		mainCompositeCS = nullptr;
	}
	if (mainCompositeInteriorCS) {
		mainCompositeInteriorCS->Release();
		mainCompositeInteriorCS = nullptr;
	}
}

ID3D11ComputeShader* Deferred::GetComputeAmbientComposite()
{
	if (!ambientCompositeCS) {
		logger::debug("Compiling AmbientCompositeCS");

		std::vector<std::pair<const char*, const char*>> defines;

		if (Skylighting::GetSingleton()->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		ambientCompositeCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return ambientCompositeCS;
}

ID3D11ComputeShader* Deferred::GetComputeAmbientCompositeInterior()
{
	if (!ambientCompositeInteriorCS) {
		logger::debug("Compiling AmbientCompositeCS INTERIOR");

		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		ambientCompositeInteriorCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\AmbientCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return ambientCompositeInteriorCS;
}

ID3D11ComputeShader* Deferred::GetComputeMainComposite()
{
	if (!mainCompositeCS) {
		logger::debug("Compiling DeferredCompositeCS");

		std::vector<std::pair<const char*, const char*>> defines;

		if (DynamicCubemaps::GetSingleton()->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		if (Skylighting::GetSingleton()->loaded)
			defines.push_back({ "SKYLIGHTING", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		mainCompositeCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return mainCompositeCS;
}

ID3D11ComputeShader* Deferred::GetComputeMainCompositeInterior()
{
	if (!mainCompositeInteriorCS) {
		logger::debug("Compiling DeferredCompositeCS INTERIOR");

		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "INTERIOR", nullptr });

		if (DynamicCubemaps::GetSingleton()->loaded)
			defines.push_back({ "DYNAMIC_CUBEMAPS", nullptr });

		if (ScreenSpaceGI::GetSingleton()->loaded)
			defines.push_back({ "SSGI", nullptr });

		if (REL::Module::IsVR())
			defines.push_back({ "FRAMEBUFFER", nullptr });

		mainCompositeInteriorCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\DeferredCompositeCS.hlsl", defines, "cs_5_0"));
	}
	return mainCompositeInteriorCS;
}
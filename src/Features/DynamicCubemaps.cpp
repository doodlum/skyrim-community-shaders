#include "DynamicCubemaps.h"
#include <Util.h>

constexpr auto MIPLEVELS = 10;

void DynamicCubemaps::DataLoaded()
{
	if (REL::Module::IsVR()) {
		// enable cubemap settings in VR
		for (const auto& settingName : iniVRCubeMapSettings) {
			if (auto setting = RE::INISettingCollection::GetSingleton()->GetSetting(settingName); setting) {
				if (!setting->data.b) {
					logger::info("[DC] Changing {} from {} to {} to support Dynamic Cubemaps", settingName, setting->data.b, true);
					setting->data.b = true;
				}
			}
		}
		for (const auto& settingPair : hiddenVRCubeMapSettings) {
			const auto& settingName = settingPair.first;
			const auto address = REL::Offset{ settingPair.second }.address();
			bool* setting = reinterpret_cast<bool*>(address);
			if (!*setting) {
				logger::info("[DC] Changing {} from {} to {} to support Dynamic Cubemaps", settingName, *setting, true);
				*setting = true;
			}
		}
	}
}

void DynamicCubemaps::ClearShaderCache()
{
	if (updateCubemapCS) {
		updateCubemapCS->Release();
		updateCubemapCS = nullptr;
	}
	if (inferCubemapCS) {
		inferCubemapCS->Release();
		inferCubemapCS = nullptr;
	}
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderUpdate()
{
	if (!updateCubemapCS) {
		logger::debug("Compiling UpdateCubemapCS");
		updateCubemapCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\UpdateCubemapCS.hlsl", {}, "cs_5_0");
	}
	return updateCubemapCS;
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderInferrence()
{
	if (!inferCubemapCS)
	{
		logger::debug("Compiling InferCubemapCS");
		inferCubemapCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\InferCubemapCS.hlsl", {}, "cs_5_0");
	}
	return inferCubemapCS;
}

void DynamicCubemaps::GenerateCubemap()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto context = renderer->GetRuntimeData().context;

	ID3D11ShaderResourceView* noView = nullptr;
	context->PSSetShaderResources(64, 1, &noView);

	auto depth = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY];
	auto snowSwap = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kSNOW_SWAP];

	context->CopyResource(colorTextureTemp->resource.get(), renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kSNOW_SWAP].texture);

	ID3D11ShaderResourceView* srvs[2] = { depth.depthSRV, colorTextureTemp->srv.get() };
	context->CSSetShaderResources(0, 2, srvs);

	ID3D11UnorderedAccessView* uav = envCaptureTexture->uav.get();
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	ID3D11Buffer* buffer;
	context->PSGetConstantBuffers(12, 1, &buffer);
	context->CSSetConstantBuffers(0, 1, &buffer);

	context->CSSetSamplers(0, 1, &computeSampler);

	context->CSSetShader(GetComputeShaderUpdate(), nullptr, 0);
	context->Dispatch((uint32_t)std::ceil(envCaptureTexture->desc.Width / 32.0f), (uint32_t)std::ceil(envCaptureTexture->desc.Height / 32.0f), 6);
	
	uav = envInferredTexture->uav.get();
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	context->GenerateMips(envCaptureTexture->srv.get());

	srvs[0] = envCaptureTexture->srv.get();
	srvs[1] = nullptr;
	context->CSSetShaderResources(0, 2, srvs);

	context->CSSetShader(GetComputeShaderInferrence(), nullptr, 0);
	context->Dispatch((uint32_t)std::ceil(envInferredTexture->desc.Width / 32.0f), (uint32_t)std::ceil(envInferredTexture->desc.Height / 32.0f), 6);

	srvs[0] = nullptr;
	srvs[1] = nullptr;
	context->CSSetShaderResources(0, 2, srvs);

	uav = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

	buffer = nullptr;
	context->CSSetConstantBuffers(0, 1, &buffer);

	context->CSSetShader(nullptr, nullptr, 0);

	ID3D11SamplerState* nullSampler = nullptr ;
	context->CSSetSamplers(0, 1, &nullSampler);
}

void DynamicCubemaps::DrawDeferred()
{
	GenerateCubemap();
}

void DynamicCubemaps::DrawSettings()
{
	if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (REL::Module::IsVR()) {
			if (ImGui::BeginTable("##SettingsToggles", 3, ImGuiTableFlags_SizingStretchSame)) {
				for (const auto& settingName : iniVRCubeMapSettings) {
					if (auto setting = RE::INISettingCollection::GetSingleton()->GetSetting(settingName); setting) {
						ImGui::TableNextColumn();
						ImGui::Checkbox(settingName.c_str(), &setting->data.b);
						if (ImGui::IsItemHovered()) {
							ImGui::BeginTooltip();
							ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
							//ImGui::Text(fmt::format(fmt::runtime("{} {0:x}"), settingName, &setting->data.b).c_str());
							ImGui::Text(settingName.c_str());
							ImGui::PopTextWrapPos();
							ImGui::EndTooltip();
						}
					}
				}
				for (const auto& settingPair : hiddenVRCubeMapSettings) {
					const auto& settingName = settingPair.first;
					const auto address = REL::Offset{ settingPair.second }.address();
					bool* setting = reinterpret_cast<bool*>(address);
					ImGui::TableNextColumn();
					ImGui::Checkbox(settingName.c_str(), setting);
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
						ImGui::Text(settingName.c_str());
						//ImGui::Text(fmt::format(fmt::runtime("{} {0:x}"), settingName, address).c_str());
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}
				}
				ImGui::EndTable();
			}
		}
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}
}

void DynamicCubemaps::UpdateCubemap()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto context = renderer->GetRuntimeData().context;

	auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	cubemap.texture = envInferredTexture->resource.get();
	cubemap.SRV = envInferredTexture->srv.get();

	// Compute diffuse irradiance cubemap.
	{
		for (uint face = 0; face < 6; face++) {
			uint srcSubresourceIndex = D3D11CalcSubresource(0, face, MIPLEVELS);
			context->CopySubresourceRegion(unfilteredEnvTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, cubemap.texture, srcSubresourceIndex, nullptr);
		}

		context->GenerateMips(unfilteredEnvTexture->srv.get());

		for (uint face = 0; face < 6; face++) {
			uint srcSubresourceIndex = D3D11CalcSubresource(0, face, MIPLEVELS);
			context->CopySubresourceRegion(envTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, cubemap.texture, srcSubresourceIndex, nullptr);
		}

		// Compute pre-filtered specular environment map.
		{
			auto srv = unfilteredEnvTexture->srv.get();
			context->CSSetShaderResources(0, 1, &srv);
			context->CSSetSamplers(0, 1, &computeSampler);
			context->CSSetShader(spmapProgram, nullptr, 0);

			ID3D11Buffer* buffers[1];
			buffers[0] = spmapCB->CB();
			context->CSSetConstantBuffers(0, ARRAYSIZE(buffers), buffers);

			float const delta_roughness = 1.0f / std::max(float(MIPLEVELS - 1), 1.0f);

			std::uint32_t size = std::max(envTexture->desc.Width, envTexture->desc.Height);

			for (std::uint32_t level = 1; level < MIPLEVELS; ++level, size /= 2) {
				const UINT numGroups = (UINT)std::max(1u, size / 32);

				const SpecularMapFilterSettingsCB spmapConstants = { level * delta_roughness };
				spmapCB->Update(spmapConstants);

				auto uav = uavArray[level-1].get();

				context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
				context->Dispatch(numGroups, numGroups, 6);
			}
		}

		// Compute diffuse irradiance cubemap.
		//{
		//	auto env_srv = cubemap.SRV;
		//	context->CSSetShaderResources(0, 1, &env_srv);
		//	auto uav = irmapTexture->uav.get();
		//	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		//	context->CSSetShader(irmapProgram, nullptr, 0);
		//	context->Dispatch(irmapTexture->desc.Width / 32, irmapTexture->desc.Height / 32, 6);
		//	uav = nullptr;
		//	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		//	context->CSSetShader(nullptr, nullptr, 0);
		//}
	}

	ID3D11ShaderResourceView* nullSRV = { nullptr };
	ID3D11SamplerState* nullSampler = { nullptr };
	ID3D11Buffer* nullBuffer = { nullptr };
	ID3D11UnorderedAccessView* nullUAV = { nullptr };

	context->CSSetShaderResources(0, 1, &nullSRV);
	context->CSSetSamplers(0, 1, &nullSampler);
	context->CSSetShader(nullptr, 0, 0);
	context->CSSetConstantBuffers(0, 1, &nullBuffer);
	context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}

void DynamicCubemaps::CreateResources()
{

	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto context = renderer->GetRuntimeData().context;

	auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];
	
	{
		spmapProgram = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\SpmapCS.hlsl", {}, "cs_5_0");
		spmapCB = new ConstantBuffer(ConstantBufferDesc<SpecularMapFilterSettingsCB>());

		D3D11_TEXTURE2D_DESC texDesc{};
		cubemap.texture->GetDesc(&texDesc);
		texDesc.MipLevels = MIPLEVELS;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
		envTexture = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = srvDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = MIPLEVELS;
		envTexture->CreateSRV(srvDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = texDesc.ArraySize;
		envTexture->CreateUAV(uavDesc);

		unfilteredEnvTexture = new Texture2D(texDesc);
		unfilteredEnvTexture->CreateSRV(srvDesc);
		unfilteredEnvTexture->CreateUAV(uavDesc);

		envCaptureTexture = new Texture2D(texDesc);
		envCaptureTexture->CreateSRV(srvDesc);
		envCaptureTexture->CreateUAV(uavDesc);

		resetCubemapCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\ResetCubemapCS.hlsl", {}, "cs_5_0");

		ID3D11UnorderedAccessView* uav = envCaptureTexture->uav.get();
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

		context->CSSetShader(resetCubemapCS, nullptr, 0);
		context->Dispatch((uint32_t)std::ceil(envCaptureTexture->desc.Width / 32.0f), (uint32_t)std::ceil(envCaptureTexture->desc.Height / 32.0f), 6);

		uav = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

		envInferredTexture = new Texture2D(texDesc);
		envInferredTexture->CreateSRV(srvDesc);
		envInferredTexture->CreateUAV(uavDesc);
	}

	{
		auto snowSwapTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kSNOW_SWAP];

		D3D11_TEXTURE2D_DESC texDesc{};
		snowSwapTexture.texture->GetDesc(&texDesc);

		colorTextureTemp = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		snowSwapTexture.SRV->GetDesc(&srvDesc);
		colorTextureTemp->CreateSRV(srvDesc);
	}

	{
		D3D11_TEXTURE2D_DESC texDesc{};
		cubemap.texture->GetDesc(&texDesc);
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;

		auto device = renderer->GetRuntimeData().forwarder;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = envTexture->desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;

		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = envTexture->desc.ArraySize;

		for (std::uint32_t level = 1; level < MIPLEVELS; ++level) {
			uavDesc.Texture2DArray.MipSlice = level;
			DX::ThrowIfFailed(device->CreateUnorderedAccessView(envTexture->resource.get(), &uavDesc, uavArray[level-1].put()));
		}
	}
}

void DynamicCubemaps::Draw(const RE::BSShader* shader, const uint32_t)
{
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	if (!envTexture) {
		CreateResources();
	}

	if (shader->shaderType.any(RE::BSShader::Type::Lighting)) {
		// During world cubemap generation we cannot use the cubemap
		auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
		auto cubeMapRenderTarget = !REL::Module::IsVR() ? shadowState->GetRuntimeData().cubeMapRenderTarget : shadowState->GetVRRuntimeData().cubeMapRenderTarget;
		if (cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS) {
			ID3D11ShaderResourceView* views[4]{};
			views[0] = nullptr;
			views[1] = nullptr;
			views[2] = nullptr;
			views[3] = nullptr;
			context->PSSetShaderResources(64, ARRAYSIZE(views), views);
		} else if (!renderedScreenCamera) {
			UpdateCubemap();
			renderedScreenCamera = true;
			ID3D11ShaderResourceView* views[4]{};
			views[0] = unfilteredEnvTexture->srv.get();
			views[1] = irmapTexture->srv.get();
			views[2] = envTexture->srv.get();
			views[3] = spBRDFLUT->srv.get();
			context->PSSetShaderResources(64, ARRAYSIZE(views), views);
		}
	}
}

void DynamicCubemaps::SetupResources()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();
	auto context = renderer->GetRuntimeData().context;
	auto device = renderer->GetRuntimeData().forwarder;

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &computeSampler));
	}

	{
		irmapProgram = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\IrmapCS.hlsl", {}, "cs_5_0");

		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = 32;
		texDesc.Height = 32;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		irmapTexture = new Texture2D(texDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = texDesc.ArraySize;

		irmapTexture->CreateUAV(uavDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = (UINT)-1;
		irmapTexture->CreateSRV(srvDesc);
	}

	{
		spBRDFProgram = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\SpbrdfCS.hlsl", {}, "cs_5_0");

		D3D11_TEXTURE2D_DESC texDesc{};
		texDesc.Width = 256;
		texDesc.Height = 256;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		spBRDFLUT = new Texture2D(texDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		spBRDFLUT->CreateUAV(uavDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		spBRDFLUT->CreateSRV(srvDesc);

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &spBRDFSampler));

		{
			// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
			auto uav = spBRDFLUT->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			context->CSSetShader(spBRDFProgram, nullptr, 0);
			context->Dispatch(spBRDFLUT->desc.Width / 32, spBRDFLUT->desc.Height / 32, 1);
			uav = nullptr;
			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		}
	}
}

void DynamicCubemaps::Reset()
{
	renderedScreenCamera = false;
}

void DynamicCubemaps::Load(json& o_json)
{
	Feature::Load(o_json);
}

void DynamicCubemaps::Save(json&)
{
}
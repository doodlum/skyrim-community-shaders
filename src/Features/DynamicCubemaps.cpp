#include "DynamicCubemaps.h"
#include <Util.h>

constexpr auto MIPLEVELS = 4;

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
	if (accumulateCubemapCS) {
		accumulateCubemapCS->Release();
		accumulateCubemapCS = nullptr;
	}
	if (updateCubemapCS) {
		updateCubemapCS->Release();
		updateCubemapCS = nullptr;
	}
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderAccumulate()
{
	if (!accumulateCubemapCS) {
		logger::debug("Compiling AccumulateCubemapCS");
		accumulateCubemapCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\AccumulateCubemapCS.hlsl", {}, "cs_5_0");
	}
	return accumulateCubemapCS;
}

ID3D11ComputeShader* DynamicCubemaps::GetComputeShaderUpdate()
{
	if (!updateCubemapCS) {
		logger::debug("Compiling UpdateCubemapCS");
		updateCubemapCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\DynamicCubemaps\\UpdateCubemapCS.hlsl", {}, "cs_5_0");
	}
	return updateCubemapCS;
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

	ID3D11UnorderedAccessView* uavs[5] = { 
		accumulationDataRedTexture->uav.get(), 
		accumulationDataGreenTexture->uav.get(), 
		accumulationDataBlueTexture->uav.get(), 
		accumulationDataCounterTexture->uav.get() , 
		envTexture->uav.get()
	};

	context->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);

	auto viewport = RE::BSGraphics::State::GetSingleton();

	float resolutionX = viewport->screenWidth * viewport->GetRuntimeData().dynamicResolutionCurrentWidthScale;
	float resolutionY = viewport->screenHeight * viewport->GetRuntimeData().dynamicResolutionCurrentHeightScale;

	{
		AccumulateCubemapCB data = {};
		data.RcpBufferDim.x = 1.0f / resolutionX;
		data.RcpBufferDim.y = 1.0f / resolutionY;

		data.CubeMapSize = accumulationDataRedTexture->desc.Width;

		auto state = RE::BSGraphics::RendererShadowState::GetSingleton();
		auto viewProjMatrix = state->GetRuntimeData().cameraData.getEye().viewProjMat;
		data.InvViewProjMatrix = DirectX::XMMatrixInverse(nullptr, viewProjMatrix);

		accumulateCubemapCB->Update(data);
	}

	ID3D11Buffer* buffer = spmapCB->CB();
	context->CSSetConstantBuffers(0, 1, &buffer);

	context->CSSetShader(GetComputeShaderAccumulate(), nullptr, 0);
	context->Dispatch((uint32_t)std::ceil(resolutionX / 32.0f), (uint32_t)std::ceil(resolutionY / 32.0f), 1);

	context->CSSetShader(GetComputeShaderUpdate(), nullptr, 0);
	context->Dispatch((uint32_t)std::ceil(envTexture->desc.Width / 32.0f), (uint32_t)std::ceil(envTexture->desc.Height / 32.0f), 6);

	srvs[0] = nullptr;
	srvs[1] = nullptr;
	context->CSSetShaderResources(0, 2, srvs);

	uavs[0] = nullptr;
	uavs[1] = nullptr;
	uavs[2] = nullptr;
	uavs[3] = nullptr;
	uavs[4] = nullptr;
	context->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);

	buffer = nullptr;
	context->CSSetConstantBuffers(0, 1, &buffer);

	context->CSSetShader(nullptr, nullptr, 0);
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

	auto device = renderer->GetRuntimeData().forwarder;
	auto context = renderer->GetRuntimeData().context;

	auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	cubemap.texture = envTexture->resource.get();
	cubemap.SRV = envTexture->srv.get();

	// Compute diffuse irradiance cubemap.
	{
		for (UINT face = 0; face < 6; face++) {
			D3D11_BOX srcBox = { 0 };
			srcBox.left = 0;
			srcBox.right = (envTexture->desc.Width >> 0);
			srcBox.top = 0;
			srcBox.bottom = (envTexture->desc.Height >> 0);
			srcBox.front = 0;
			srcBox.back = 1;

			// Calculate the subresource index for the current face and mip level
			UINT srcSubresourceIndex = D3D11CalcSubresource(0, face, 1);

			// Copy the subresource from the source to the destination texture
			context->CopySubresourceRegion(unfilteredEnvTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, cubemap.texture, srcSubresourceIndex, &srcBox);
		}

		context->GenerateMips(unfilteredEnvTexture->srv.get());

		//for (UINT face = 0; face < 6; face++) {
		//	D3D11_BOX srcBox = { 0 };
		//	srcBox.left = 0;
		//	srcBox.right = (envTexture->desc.Width >> 0);
		//	srcBox.top = 0;
		//	srcBox.bottom = (envTexture->desc.Height >> 0);
		//	srcBox.front = 0;
		//	srcBox.back = 1;

		//	// Calculate the subresource index for the current face and mip level
		//	UINT srcSubresourceIndex = D3D11CalcSubresource(0, face, 1);

		//	// Copy the subresource from the source to the destination texture
		//	context->CopySubresourceRegion(envTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, cubemap.texture, srcSubresourceIndex, &srcBox);
		//}

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

			winrt::com_ptr<ID3D11UnorderedAccessView> uav;
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = envTexture->desc.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;

			uavDesc.Texture2DArray.FirstArraySlice = 0;
			uavDesc.Texture2DArray.ArraySize = envTexture->desc.ArraySize;

			for (std::uint32_t level = 1; level < MIPLEVELS; ++level, size /= 2) {
				const UINT numGroups = (UINT)std::max(1u, size / 32);

				uavDesc.Texture2DArray.MipSlice = level;

				DX::ThrowIfFailed(device->CreateUnorderedAccessView(envTexture->resource.get(), &uavDesc, uav.put()));

				const SpecularMapFilterSettingsCB spmapConstants = { level * delta_roughness };
				spmapCB->Update(spmapConstants);

				auto uavPtr = uav.get();

				context->CSSetUnorderedAccessViews(0, 1, &uavPtr, nullptr);
				context->Dispatch(numGroups, numGroups, 6);
			}
		}

		// Compute diffuse irradiance cubemap.
		{
			auto env_srv = cubemap.SRV;
			context->CSSetShaderResources(0, 1, &env_srv);
			auto uav = irmapTexture->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			context->CSSetShader(irmapProgram, nullptr, 0);
			context->Dispatch(irmapTexture->desc.Width / 32, irmapTexture->desc.Height / 32, 6);
			uav = nullptr;
			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			context->CSSetShader(nullptr, nullptr, 0);
		}
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
	auto cubemap = RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

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
	}
}

void DynamicCubemaps::Draw(const RE::BSShader* shader, const uint32_t)
{
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	if (!accumulationDataRedTexture) {
		auto renderer = RE::BSGraphics::Renderer::GetSingleton();

		{
			auto snowSwapTexture = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kSNOW_SWAP];

			D3D11_TEXTURE2D_DESC texDesc{};
			snowSwapTexture.texture->GetDesc(&texDesc);

			colorTextureTemp = new Texture2D(texDesc);

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			snowSwapTexture.SRV->GetDesc(&srvDesc);
			colorTextureTemp->CreateSRV(srvDesc);
		}

		auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

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

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = 0;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = texDesc.ArraySize;


		//dynamicCubemapTexture = new Texture2D(texDesc);
	//	dynamicCubemapTexture->CreateSRV(srvDesc);

		texDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.Format = texDesc.Format;
		uavDesc.Format = texDesc.Format;

		accumulationDataRedTexture = new Texture2D(texDesc);
		accumulationDataRedTexture->CreateSRV(srvDesc);
		accumulationDataRedTexture->CreateUAV(uavDesc);

		accumulationDataGreenTexture = new Texture2D(texDesc);
		accumulationDataGreenTexture->CreateSRV(srvDesc);
		accumulationDataGreenTexture->CreateUAV(uavDesc);

		accumulationDataBlueTexture = new Texture2D(texDesc);
		accumulationDataBlueTexture->CreateSRV(srvDesc);
		accumulationDataBlueTexture->CreateUAV(uavDesc);

		accumulationDataCounterTexture = new Texture2D(texDesc);
		accumulationDataCounterTexture->CreateSRV(srvDesc);
		accumulationDataCounterTexture->CreateUAV(uavDesc);

		accumulateCubemapCB = new ConstantBuffer(ConstantBufferDesc<AccumulateCubemapCB>());
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
			if (!envTexture) {
				CreateResources();
			}
//			UpdateCubemap();
			renderedScreenCamera = true;
			ID3D11ShaderResourceView* views[4]{};
			//views[0] = unfilteredEnvTexture->srv.get();
			//views[1] = irmapTexture->srv.get();
			//views[2] = envTexture->srv.get();
			views[0] = envTexture->srv.get();
			views[1] = nullptr;
			views[2] = nullptr;
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
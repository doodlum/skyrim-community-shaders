#include "DynamicCubemaps.h"
#include <Util.h>

constexpr auto MIPLEVELS = 4;

void DynamicCubemaps::DrawSettings()
{
}

void DynamicCubemaps::UpdateCubemap()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto device = renderer->GetRuntimeData().forwarder;
	auto context = renderer->GetRuntimeData().context;

	auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

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
			context->CopySubresourceRegion(envTexture->resource.get(), D3D11CalcSubresource(0, face, MIPLEVELS), 0, 0, 0, cubemap.texture, srcSubresourceIndex, &srcBox);
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

		unfilteredEnvTexture = new Texture2D(texDesc);
		unfilteredEnvTexture->CreateSRV(srvDesc);
	}
}

void DynamicCubemaps::Draw(const RE::BSShader* shader, const uint32_t)
{
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	if (shader->shaderType.any(RE::BSShader::Type::Lighting)) {
		// During world cubemap generation we cannot use the cubemap
		auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
		if (shadowState->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS) {
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
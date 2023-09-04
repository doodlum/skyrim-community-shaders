#include "RainWetnessEffects.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	RainWetnessEffects::Settings,
	EnableRainWetnessEffects,
	RainShininessMultiplier,
	RainSpecularMultiplier,
	RainDiffuseMultiplier)

void RainWetnessEffects::DrawSettings()
{
	if (ImGui::TreeNodeEx("Rain Wetness Effect", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Rain Wetness Effects", (bool*)&settings.EnableRainWetnessEffects);

		ImGui::SliderFloat("Rain Shininess Multiplier", &settings.RainShininessMultiplier, 0, 4);

		ImGui::SliderFloat("Rain Specular Multiplier", &settings.RainSpecularMultiplier, 0, 50);
		
		ImGui::SliderFloat("Rain Diffuse Multiplier", &settings.RainDiffuseMultiplier, 0, 2);

		ImGui::TreePop();
	}
}

void RainWetnessEffects::Draw(const RE::BSShader* shader, const uint32_t)
{
	if (shader->shaderType.any(RE::BSShader::Type::Lighting)) {
		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

		PerPass data{};
		data.settings = settings;

		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			if (auto cell = player->GetParentCell()) {
				if (!cell->IsInteriorCell()) {
					if (auto sky = RE::Sky::GetSingleton()) {
						data.IsRaining = sky->IsRaining();
					}
				}
				else
				{
						data.IsRaining = false;
				}
			}
		}

		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(perPass->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(PerPass);
		memcpy_s(mapped.pData, bytes, &data, bytes);
		context->Unmap(perPass->resource.get(), 0);

		ID3D11ShaderResourceView* views[1]{};
		views[0] = perPass->srv.get();
		context->PSSetShaderResources(22, ARRAYSIZE(views), views);

		/*if (shader->shaderType.any(RE::BSShader::Type::Water)) {
			auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			ID3D11ShaderResourceView* views[2]{};
			views[0] = renderer->GetDepthStencilData().depthStencils[RE::RENDER_TARGETS_DEPTHSTENCIL::kPOST_ZPREPASS_COPY].depthSRV;
			views[1] = perPass->srv.get();
			context->PSSetShaderResources(31, ARRAYSIZE(views), views);
		} else {
			ID3D11ShaderResourceView* views[1]{};
			views[0] = perPass->srv.get();
			context->PSSetShaderResources(32, ARRAYSIZE(views), views);
		}*/
	}
}

void RainWetnessEffects::SetupResources()
{
	D3D11_BUFFER_DESC sbDesc{};
	sbDesc.Usage = D3D11_USAGE_DYNAMIC;
	sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbDesc.StructureByteStride = sizeof(PerPass);
	sbDesc.ByteWidth = sizeof(PerPass);
	perPass = std::make_unique<Buffer>(sbDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 1;
	perPass->CreateSRV(srvDesc);
}

void RainWetnessEffects::Load(json& o_json)
{
	if (o_json[GetName()].is_object())
		settings = o_json[GetName()];

	Feature::Load(o_json);
}

void RainWetnessEffects::Save(json& o_json)
{
	o_json[GetName()] = settings;
}
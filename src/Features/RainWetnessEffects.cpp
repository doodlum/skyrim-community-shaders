#include "RainWetnessEffects.h"

const float MIN_START_PERCENTAGE = 0.05f;
const float DEFAULT_TRANSITION_PERCENTAGE = 1.0f;
const float DRY_SHININESS_MULTIPLIER = 1.0f;
const float DRY_SPECULAR_MULTIPLIER = 1.0f;
const float DRY_DIFFUSE_MULTIPLIER = 1.0f;
const float TRANSITION_CURVE_MULTIPLIER = 3.0f;
const float TRANSITION_DENOMINATOR = 256.0f;
const float DAY = 0.0f;
const float NIGHT = 1.0f;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	RainWetnessEffects::Settings,
	EnableRainWetnessEffects)

void RainWetnessEffects::DrawSettings()
{
	if (ImGui::TreeNodeEx("Rain Wetness Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Rain Wetness", (bool*)&settings.EnableRainWetnessEffects);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Enables a wetness effect when it is raining.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SliderFloat("Albedo Color Pow", &settings.AlbedoColorPow, 1, 2);

		ImGui::SliderFloat("Wetness Water Edge Range", &settings.WetnessWaterEdgeRange, 1, 100);

		ImGui::TreePop();
	}
}

void RainWetnessEffects::UpdateCubemap()
{
	auto renderer = RE::BSGraphics::Renderer::GetSingleton();

	auto context = renderer->GetRuntimeData().context;
	auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

	for (UINT face = 0; face < 6; face++) {
		D3D11_BOX srcBox = { 0 };
		srcBox.left = 0;
		srcBox.right = (blurredReflectionsTexture->desc.Width >> 0);
		srcBox.top = 0;
		srcBox.bottom = (blurredReflectionsTexture->desc.Height >> 0);
		srcBox.front = 0;
		srcBox.back = 1;

		// Calculate the subresource index for the current face and mip level
		UINT srcSubresourceIndex = D3D11CalcSubresource(0, face, 1);

		// Copy the subresource from the source to the destination texture
		context->CopySubresourceRegion(blurredReflectionsTexture->resource.get(), D3D11CalcSubresource(0, face, 6), 0, 0, 0, cubemap.texture, srcSubresourceIndex, &srcBox);
	}

	context->GenerateMips(blurredReflectionsTexture->srv.get());
}

void RainWetnessEffects::Draw(const RE::BSShader* shader, const uint32_t)
{
	auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

	if (!blurredReflectionsTexture) {
		auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto cubemap = renderer->GetRendererData().cubemapRenderTargets[RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS];

		D3D11_TEXTURE2D_DESC texDesc{};
		cubemap.texture->GetDesc(&texDesc);
		texDesc.MipLevels = 6;
		texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		blurredReflectionsTexture = new Texture2D(texDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		cubemap.SRV->GetDesc(&srvDesc);
		srvDesc.TextureCube.MipLevels = 4;
		blurredReflectionsTexture->CreateSRV(srvDesc);
	}

	if (shader->shaderType.any(RE::BSShader::Type::Lighting)) {

		// During world cubemap generation we cannot use the cubemap
		auto shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
		if (shadowState->GetRuntimeData().cubeMapRenderTarget == RE::RENDER_TARGETS_CUBEMAP::kREFLECTIONS) {
			ID3D11ShaderResourceView* views[1]{};
			views[0] = nullptr;
			context->PSSetShaderResources(64, 1, views);
		} else if (!renderedScreenCamera) {
			UpdateCubemap();
			renderedScreenCamera = true;
			ID3D11ShaderResourceView* views[1]{};
			views[0] = blurredReflectionsTexture->srv.get();
			context->PSSetShaderResources(64, 1, views);
		}

		PerPass data{};
		data.settings = settings;

		data.wetness = 0;

		float WeatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
		float WetnessCurrentDay = 0.0f;
		float WetnessPreviousDay = 0.0f;

		if (settings.EnableRainWetnessEffects) {
			if (auto player = RE::PlayerCharacter::GetSingleton()) {
				if (auto cell = player->GetParentCell()) {
					if (!cell->IsInteriorCell()) {
						if (auto sky = RE::Sky::GetSingleton()) {
							if (auto currentWeather = sky->currentWeather) {
								if (currentWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
									// Currently raining
									WetnessCurrentDay = 1.0f;

									// Fade in gradually after precipitation has started
									float beginFade = currentWeather->data.precipitationBeginFadeIn;
									beginFade = beginFade > 0 ? beginFade : beginFade + TRANSITION_DENOMINATOR;
									float startPercentage = (TRANSITION_DENOMINATOR - beginFade) * (1.0f / TRANSITION_DENOMINATOR);
									startPercentage = startPercentage > MIN_START_PERCENTAGE ? startPercentage : MIN_START_PERCENTAGE;
									float currentPercentage = (sky->currentWeatherPct - startPercentage) / (1 - startPercentage);
									WeatherTransitionPercentage = std::clamp(currentPercentage, 0.0f, 1.0f);
								} else {				
									WetnessCurrentDay = 0.0f;
								}

								if (auto lastWeather = sky->lastWeather) {
									if (lastWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
										// Was raining before
										WetnessPreviousDay = 1.0f;

										// Fade out gradually
										WeatherTransitionPercentage = sky->currentWeatherPct;

									} else {
										WetnessPreviousDay = 0.0f;
									}
								} else {
									// No last weather, 100% transition
									WeatherTransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
									WetnessPreviousDay = WetnessCurrentDay;
								}

								// Adjust the transition curve to ease in to the transition
								WeatherTransitionPercentage = (exp2(TRANSITION_CURVE_MULTIPLIER * log2(WeatherTransitionPercentage)));

								data.wetness = std::lerp(WetnessPreviousDay, WetnessCurrentDay, WeatherTransitionPercentage);
							}
						}
					}
				}
			}
		}

		data.waterHeight = -FLT_MAX;

		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			if (auto cell = player->GetParentCell()) {
				if (!cell->IsInteriorCell()) {
					auto height = cell->GetExteriorWaterHeight();
					data.waterHeight = height - shadowState->GetRuntimeData().posAdjust.getEye().z;
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

void RainWetnessEffects::Reset()
{
	renderedScreenCamera = false;
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
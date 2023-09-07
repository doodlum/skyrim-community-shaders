#include "RainWetnessEffects.h"

const float MIN_START_PERCENTAGE = 0.05f;
const float DEFAULT_TRANSITION_PERCENTAGE = 1.0f;
const float DRY_SHININESS_MULTIPLIER = 1.0f;
const float DRY_SPECULAR_MULTIPLIER = 1.0f;
const float DRY_DIFFUSE_MULTIPLIER = 1.0f;
const float TRANSITION_CURVE_MULTIPLIER = 3.0f;

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	RainWetnessEffects::Settings,
	EnableRainWetnessEffects,
	RainShininessMultiplier,
	RainSpecularMultiplier,
	RainDiffuseMultiplier)

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

		ImGui::SliderFloat("Rain Specular Multiplier", &settings.RainSpecularMultiplier, 0, 50);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Controls the strength of the wetness effect.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SliderFloat("Rain Shininess Multiplier", &settings.RainShininessMultiplier, 0, 4);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Controls the appearance of shininess by focusing the reflection.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::SliderFloat("Rain Diffuse Multiplier", &settings.RainDiffuseMultiplier, 0, 2);
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::Text("Darkens the diffuse color to simulate the layer of water causing light to be reflected in a less diffuse and more specular manner.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::TreePop();
	}
}

void RainWetnessEffects::Draw(const RE::BSShader* shader, const uint32_t)
{
	if (shader->shaderType.any(RE::BSShader::Type::Lighting)) {
		auto context = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context;

		PerPass data{};
		data.settings = settings;
		
		data.EnableEffect = false;
		data.TransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
		data.ShininessMultiplierCurrent = DRY_SHININESS_MULTIPLIER;
		data.ShininessMultiplierPrevious = DRY_SHININESS_MULTIPLIER;
		data.SpecularMultiplierCurrent = DRY_SPECULAR_MULTIPLIER;
		data.SpecularMultiplierPrevious = DRY_SPECULAR_MULTIPLIER;
		data.DiffuseMultiplierCurrent = DRY_DIFFUSE_MULTIPLIER;
		data.DiffuseMultiplierPrevious = DRY_DIFFUSE_MULTIPLIER;

		if (settings.EnableRainWetnessEffects) {
			if (auto player = RE::PlayerCharacter::GetSingleton()) {
				if (auto cell = player->GetParentCell()) {
					if (!cell->IsInteriorCell()) {
						data.EnableEffect = true;
						if (auto sky = RE::Sky::GetSingleton()) {
							if (auto currentWeather = sky->currentWeather) {
								if (currentWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
									// Currently raining
									data.ShininessMultiplierCurrent = settings.RainShininessMultiplier;
									data.SpecularMultiplierCurrent = settings.RainSpecularMultiplier;
									data.DiffuseMultiplierCurrent = settings.RainDiffuseMultiplier;

									// Fade in gradually after precipitation has started
									float startPercentage = (currentWeather->data.precipitationBeginFadeIn + 256) * (1.0f / 255.0f);
									startPercentage = startPercentage > MIN_START_PERCENTAGE ? startPercentage : MIN_START_PERCENTAGE;
									float currentPercentage = (sky->currentWeatherPct - startPercentage) / (1 - startPercentage);
									data.TransitionPercentage = currentPercentage > 0.0f ? currentPercentage : 0.0f;
								} else {
									// Not currently raining
									data.ShininessMultiplierCurrent = 1.0f;
									data.SpecularMultiplierCurrent = 1.0f;
									data.DiffuseMultiplierCurrent = 1.0f;
								}

								if (auto lastWeather = sky->lastWeather) {
									if (lastWeather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy)) {
										// Was raining before
										data.ShininessMultiplierPrevious = settings.RainShininessMultiplier;
										data.SpecularMultiplierPrevious = settings.RainSpecularMultiplier;
										data.DiffuseMultiplierPrevious = settings.RainDiffuseMultiplier;

										// Fade out gradually
										data.TransitionPercentage = sky->currentWeatherPct;

									} else {
										// Wasn't raining before
										data.ShininessMultiplierPrevious = 1.0f;
										data.SpecularMultiplierPrevious = 1.0f;
										data.DiffuseMultiplierPrevious = 1.0f;
									}
								} else {
									// No last weather, 100% transition
									data.TransitionPercentage = DEFAULT_TRANSITION_PERCENTAGE;
									data.ShininessMultiplierPrevious = data.ShininessMultiplierCurrent;
									data.SpecularMultiplierPrevious = data.SpecularMultiplierCurrent;
									data.DiffuseMultiplierPrevious = data.DiffuseMultiplierCurrent;
								}
							} else {
								// No current weather, don't do anything
								data.EnableEffect = false;
							}
						} else {
							// Can't get weather, don't do anything
							data.EnableEffect = false;
						}
					}
				}
			}
			// Adjust the transition curve to ease in to the transition
			data.TransitionPercentage = exp2(TRANSITION_CURVE_MULTIPLIER * log2(data.TransitionPercentage));
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
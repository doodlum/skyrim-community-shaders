#include "Clustered.h"
#include <State.h>

void Clustered::Reset()
{
	rendered = false;
}

void Clustered::Bind(bool a_update)
{
	if (!rendered && a_update) {
		UpdateLights();
		rendered = true;
	}

	if (lights) {
		auto context = RE::BSRenderManager::GetSingleton()->GetRuntimeData().context;
		auto renderer = BSGraphics::Renderer::QInstance();
		ID3D11ShaderResourceView* views[2]{};
		views[0] = lights->srv.get();
		views[1] = renderer->pDepthStencils[DEPTH_STENCIL_POST_ZPREPASS_COPY].DepthSRV;
		context->PSSetShaderResources(17, ARRAYSIZE(views), views);
	}
}

void Clustered::UpdateLights()
{
	std::uint32_t current_light_count = 0;  // Max number of lights is 4294967295

	auto accumulator = State::GetCurrentAccumulator();
	auto shadowSceneNode = accumulator->m_ActiveShadowSceneNode;

	auto state = BSGraphics::RendererShadowState::QInstance();

	std::vector<LightSData> lights_data{};
	for (auto& e : shadowSceneNode->GetRuntimeData().activeSceneLights) {
		if (auto screenSpaceLight = e.get()) {
			if (auto& niLight = screenSpaceLight->light) {
				RE::NiPoint3 worldPos = niLight->world.translate;
				float dimmer = niLight->GetLightRuntimeData().fade * screenSpaceLight->lodDimmer;

				LightSData light{};

				DirectX::XMFLOAT3 color{};
				color.x = dimmer * niLight->GetLightRuntimeData().diffuse.red;
				color.y = dimmer * niLight->GetLightRuntimeData().diffuse.green;
				color.z = dimmer * niLight->GetLightRuntimeData().diffuse.blue;
				light.color = XMLoadFloat3(&color);

				worldPos = worldPos - BSGraphics::RendererShadowState::QInstance()->m_PosAdjust;

				DirectX::XMFLOAT3 position{};
				position.x = worldPos.x;
				position.y = worldPos.y;
				position.z = worldPos.z;
				light.positionWS = XMLoadFloat3(&position);
				light.positionVS = XMVector3TransformCoord(light.positionWS, state->m_CameraData.m_ViewMat);

				light.radius = niLight->GetLightRuntimeData().radius.x;

				light.active = true;
				light.shadow = false;
				light.mask = -1;

				lights_data.push_back(light);
				current_light_count++;
			}
		}
	}

	for (auto& e : shadowSceneNode->GetRuntimeData().shadowCasterLights) {
		if (auto shadowLight = e) {
			if (auto& niLight = shadowLight->light) {
				RE::NiPoint3 worldPos = niLight->world.translate;
				float dimmer = niLight->GetLightRuntimeData().fade * shadowLight->lodDimmer;

				LightSData light{};

				DirectX::XMFLOAT3 color{};
				color.x = dimmer * niLight->GetLightRuntimeData().diffuse.red;
				color.y = dimmer * niLight->GetLightRuntimeData().diffuse.green;
				color.z = dimmer * niLight->GetLightRuntimeData().diffuse.blue;
				light.color = XMLoadFloat3(&color);

				worldPos = worldPos - BSGraphics::RendererShadowState::QInstance()->m_PosAdjust;

				DirectX::XMFLOAT3 position{};
				position.x = worldPos.x;
				position.y = worldPos.y;
				position.z = worldPos.z;
				light.positionWS = XMLoadFloat3(&position);
				light.positionVS = XMVector3TransformCoord(light.positionWS, state->m_CameraData.m_ViewMat);

				light.radius = niLight->GetLightRuntimeData().radius.x;

				light.active = true;
				light.shadow = true;
				light.mask = (float)shadowLight->unk520;

				lights_data.push_back(light);
				current_light_count++;
			}
		}
	}

	if (!current_light_count) {
		LightSData data{};
		ZeroMemory(&data, sizeof(data));
		lights_data.push_back(data);
		current_light_count++;
	}

	static std::uint32_t light_count = 0;
	bool light_count_changed = current_light_count != light_count;

	if (!lights || light_count_changed) {
		light_count = current_light_count;

		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(LightSData);
		sbDesc.ByteWidth = sizeof(LightSData) * light_count;
		lights = std::make_unique<Buffer>(sbDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = light_count;
		lights->CreateSRV(srvDesc);
	}

	auto context = RE::BSRenderManager::GetSingleton()->GetRuntimeData().context;
	D3D11_MAPPED_SUBRESOURCE mapped;
	DX::ThrowIfFailed(context->Map(lights->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	size_t bytes = sizeof(LightSData) * light_count;
	memcpy_s(mapped.pData, bytes, lights_data.data(), bytes);
	context->Unmap(lights->resource.get(), 0);
}
#include "WaterCaustics.h"

#include <DDSTextureLoader.h>

void WaterCaustics::DrawSettings()
{
}

void WaterCaustics::Draw(const RE::BSShader*, const uint32_t)
{
	auto context = reinterpret_cast<ID3D11DeviceContext*>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context);
	context->PSSetShaderResources(70, 1, &causticsView);
}

void WaterCaustics::SetupResources()
{
	auto device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().forwarder);
	auto context = reinterpret_cast<ID3D11DeviceContext*>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context);

	DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\WaterCaustics\\watercaustics.dds", nullptr, &causticsView);
}

void WaterCaustics::Load(json& o_json)
{
	Feature::Load(o_json);
}

void WaterCaustics::Save(json&)
{
}

void WaterCaustics::RestoreDefaultSettings()
{
}

bool WaterCaustics::HasShaderDefine(RE::BSShader::Type)
{
	return true;
}
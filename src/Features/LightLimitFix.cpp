#include "LightLimitFix.h"

#include "Shadercache.h"
#include "State.h"
#include "Util.h"

static constexpr uint CLUSTER_MAX_LIGHTS = 128;
static constexpr uint MAX_LIGHTS = 2048;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	LightLimitFix::Settings,
	EnableContactShadows,
	EnableParticleLights,
	EnableParticleLightsCulling,
	EnableParticleLightsDetection,
	ParticleLightsSaturation,
	EnableParticleLightsOptimization,
	ParticleLightsOptimisationClusterRadius,
	ParticleBrightness,
	ParticleRadius,
	BillboardBrightness,
	BillboardRadius)

void LightLimitFix::DrawSettings()
{
	if (ImGui::TreeNodeEx("Particle Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Particle Lights", &settings.EnableParticleLights);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Enables Particle Lights.");
		}

		ImGui::Checkbox("Enable Culling", &settings.EnableParticleLightsCulling);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Significantly improves performance by not rendering empty textures. Only disable if you are encountering issues.");
		}

		ImGui::Checkbox("Enable Detection", &settings.EnableParticleLightsDetection);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Adds particle lights to the player light level, so that NPCs can detect them for stealth and gameplay.");
		}

		ImGui::Checkbox("Enable Optimization", &settings.EnableParticleLightsOptimization);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Merges vertices which are close enough to each other to significantly improve performance.");
		}
		ImGui::SliderInt("Optimisation Cluster Radius", (int*)&settings.ParticleLightsOptimisationClusterRadius, 1, 64);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Radius to use for clustering lights.");
		}
		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::TextWrapped("Particle Lights Customisation");
		ImGui::SliderFloat("Saturation", &settings.ParticleLightsSaturation, 1.0, 2.0, "%.2f");
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Particle light saturation.");
		}
		ImGui::SliderFloat("Particle Brightness", &settings.ParticleBrightness, 0.0, 10.0, "%.2f");
		ImGui::SliderFloat("Particle Radius", &settings.ParticleRadius, 0.0, 10.0, "%.2f");
		ImGui::SliderFloat("Billboard Brightness", &settings.BillboardBrightness, 0.0, 10.0, "%.2f");
		ImGui::SliderFloat("Billboard Radius", &settings.BillboardRadius, 0.0, 10.0, "%.2f");

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Contact Shadows", &settings.EnableContactShadows);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("All lights cast small shadows. Performance impact.");
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	auto& shaderCache = SIE::ShaderCache::Instance();

	if (ImGui::TreeNodeEx("Light Limit Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Lights Visualisation", &settings.EnableLightsVisualisation);
		if (auto _tt = Util::HoverTooltipWrapper()) {
			ImGui::Text("Enables visualization of the light limit\n");
		}

		{
			static const char* comboOptions[] = { "Light Limit", "Strict Lights Count", "Clustered Lights Count", "Shadow Mask" };
			ImGui::Combo("Lights Visualisation Mode", (int*)&settings.LightsVisualisationMode, comboOptions, 4);
			if (auto _tt = Util::HoverTooltipWrapper()) {
				ImGui::Text(
					" - Visualise the light limit. Red when the \"strict\" light limit is reached (portal-strict lights).\n"
					" - Visualise the number of strict lights.\n"
					" - Visualise the number of clustered lights.\n"
					" - Visualize the Shadow Mask.\n");
			}
		}
		currentEnableLightsVisualisation = settings.EnableLightsVisualisation;
		if (previousEnableLightsVisualisation != currentEnableLightsVisualisation) {
			State::GetSingleton()->SetDefines(settings.EnableLightsVisualisation ? "LLFDEBUG" : "");
			shaderCache.Clear(RE::BSShader::Type::Lighting);
			previousEnableLightsVisualisation = currentEnableLightsVisualisation;
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text(std::format("Clustered Light Count : {}", lightCount).c_str());
		ImGui::Text(std::format("Particle Lights Detection Count : {}", particleLightsDetectionHits).c_str());

		ImGui::TreePop();
	}
}

LightLimitFix::PerFrame LightLimitFix::GetCommonBufferData()
{
	PerFrame perFrame{};
	perFrame.EnableContactShadows = settings.EnableContactShadows;
	perFrame.EnableLightsVisualisation = settings.EnableLightsVisualisation;
	perFrame.LightsVisualisationMode = settings.LightsVisualisationMode;
	std::copy(clusterSize, clusterSize + 3, perFrame.ClusterSize);
	return perFrame;
}

void LightLimitFix::SetupResources()
{
	auto screenSize = Util::ConvertToDynamic(State::GetSingleton()->screenSize);
	if (REL::Module::IsVR())
		screenSize.x *= .5;
	clusterSize[0] = ((uint)screenSize.x + 63) / 64;
	clusterSize[1] = ((uint)screenSize.y + 63) / 64;
	clusterSize[2] = 32;
	uint clusterCount = clusterSize[0] * clusterSize[1] * clusterSize[2];

	{
		std::string clusterSizeStrs[3];
		for (int i = 0; i < 3; ++i)
			clusterSizeStrs[i] = std::format("{}", clusterSize[i]);

		std::vector<std::pair<const char*, const char*>> defines = {
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_X", clusterSizeStrs[0].c_str() },
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_Y", clusterSizeStrs[1].c_str() },
			{ "CLUSTER_BUILDING_DISPATCH_SIZE_Z", clusterSizeStrs[2].c_str() }
		};

		clusterBuildingCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\LightLimitFix\\ClusterBuildingCS.hlsl", defines, "cs_5_0");
		clusterCullingCS = (ID3D11ComputeShader*)Util::CompileShader(L"Data\\Shaders\\LightLimitFix\\ClusterCullingCS.hlsl", defines, "cs_5_0");

		lightBuildingCB = new ConstantBuffer(ConstantBufferDesc<LightBuildingCB>());
		lightCullingCB = new ConstantBuffer(ConstantBufferDesc<LightCullingCB>());
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

		std::uint32_t numElements = clusterCount;

		sbDesc.StructureByteStride = sizeof(ClusterAABB);
		sbDesc.ByteWidth = sizeof(ClusterAABB) * numElements;
		clusters = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		clusters->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		clusters->CreateUAV(uavDesc);

		numElements = 1;
		sbDesc.StructureByteStride = sizeof(uint32_t);
		sbDesc.ByteWidth = sizeof(uint32_t) * numElements;
		lightCounter = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightCounter->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightCounter->CreateUAV(uavDesc);

		numElements = clusterCount * CLUSTER_MAX_LIGHTS;
		sbDesc.StructureByteStride = sizeof(uint32_t);
		sbDesc.ByteWidth = sizeof(uint32_t) * numElements;
		lightList = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightList->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightList->CreateUAV(uavDesc);

		numElements = clusterCount;
		sbDesc.StructureByteStride = sizeof(LightGrid);
		sbDesc.ByteWidth = sizeof(LightGrid) * numElements;
		lightGrid = eastl::make_unique<Buffer>(sbDesc);
		srvDesc.Buffer.NumElements = numElements;
		lightGrid->CreateSRV(srvDesc);
		uavDesc.Buffer.NumElements = numElements;
		lightGrid->CreateUAV(uavDesc);
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(LightData);
		sbDesc.ByteWidth = sizeof(LightData) * MAX_LIGHTS;
		lights = eastl::make_unique<Buffer>(sbDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = MAX_LIGHTS;
		lights->CreateSRV(srvDesc);
	}

	{
		D3D11_BUFFER_DESC sbDesc{};
		sbDesc.Usage = D3D11_USAGE_DYNAMIC;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		sbDesc.StructureByteStride = sizeof(StrictLightData);
		sbDesc.ByteWidth = sizeof(StrictLightData);
		strictLightData = std::make_unique<Buffer>(sbDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = 1;
		strictLightData->CreateSRV(srvDesc);
	}
}

void LightLimitFix::Reset()
{
	for (auto& particleLight : particleLights) {
		if (const auto particleSystem = netimmerse_cast<RE::NiParticleSystem*>(particleLight.first)) {
			if (auto particleData = particleSystem->GetParticleRuntimeData().particleData.get()) {
				particleData->DecRefCount();
			}
		}
		particleLight.first->DecRefCount();
	}
	particleLights.clear();
	std::swap(particleLights, queuedParticleLights);
}

void LightLimitFix::LoadSettings(json& o_json)
{
	settings = o_json;
}

void LightLimitFix::SaveSettings(json& o_json)
{
	o_json = settings;
}

void LightLimitFix::RestoreDefaultSettings()
{
	settings = {};
}

RE::NiNode* GetParentRoomNode(RE::NiAVObject* object)
{
	if (object == nullptr) {
		return nullptr;
	}

	static const auto* roomRtti = REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSMultiBoundRoom }.get();
	static const auto* portalRtti = REL::Relocation<const RE::NiRTTI*>{ RE::NiRTTI_BSPortalSharedNode }.get();

	const auto* rtti = object->GetRTTI();
	if (rtti == roomRtti || rtti == portalRtti) {
		return static_cast<RE::NiNode*>(object);
	}

	return GetParentRoomNode(object->parent);
}

void LightLimitFix::BSLightingShader_SetupGeometry_Before(RE::BSRenderPass* a_pass)
{
	strictLightDataTemp.NumStrictLights = 0;

	strictLightDataTemp.RoomIndex = -1;
	if (!roomNodes.empty()) {
		if (RE::NiNode* roomNode = GetParentRoomNode(a_pass->geometry)) {
			if (auto it = roomNodes.find(roomNode); it != roomNodes.cend()) {
				strictLightDataTemp.RoomIndex = it->second;
			}
		}
	}
}

void LightLimitFix::BSLightingShader_SetupGeometry_GeometrySetupConstantPointLights(RE::BSRenderPass* a_pass, DirectX::XMMATRIX&, uint32_t, uint32_t, float, Space)
{
	auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();
	bool inWorld = accumulator->GetRuntimeData().activeShadowSceneNode == RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	strictLightDataTemp.NumStrictLights = inWorld ? 0 : (a_pass->numLights - 1);

	for (uint32_t i = 0; i < strictLightDataTemp.NumStrictLights; i++) {
		auto bsLight = a_pass->sceneLights[i + 1];
		auto niLight = bsLight->light.get();

		auto& runtimeData = niLight->GetLightRuntimeData();

		LightData light{};
		light.color = { runtimeData.diffuse.red, runtimeData.diffuse.green, runtimeData.diffuse.blue };
		light.color *= runtimeData.fade;
		light.color *= bsLight->lodDimmer;

		light.radius = runtimeData.radius.x;

		SetLightPosition(light, niLight->world.translate, inWorld);

		if (bsLight->IsShadowLight()) {
			auto* shadowLight = static_cast<RE::BSShadowLight*>(bsLight);
			GET_INSTANCE_MEMBER(shadowLightIndex, shadowLight);
			light.shadowMaskIndex = shadowLightIndex;
			light.lightFlags.set(LightFlags::Shadow);
		}

		strictLightDataTemp.StrictLights[i] = light;
	}
}

void LightLimitFix::BSLightingShader_SetupGeometry_After(RE::BSRenderPass*)
{
	auto& context = State::GetSingleton()->context;
	auto accumulator = RE::BSGraphics::BSShaderAccumulator::GetCurrentAccumulator();

	static bool wasEmpty = false;
	static bool wasWorld = false;
	static int previousRoomIndex = -1;

	const bool isEmpty = strictLightDataTemp.NumStrictLights == 0;
	const bool isWorld = accumulator->GetRuntimeData().activeShadowSceneNode == RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
	const int roomIndex = strictLightDataTemp.RoomIndex;

	if (!isEmpty || (isEmpty && !wasEmpty) || isWorld != wasWorld || previousRoomIndex != roomIndex) {
		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(strictLightData->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(StrictLightData);
		memcpy_s(mapped.pData, bytes, &strictLightDataTemp, bytes);
		context->Unmap(strictLightData->resource.get(), 0);

		wasEmpty = isEmpty;
		wasWorld = isWorld;
		previousRoomIndex = roomIndex;
	}

	ID3D11ShaderResourceView* view = strictLightData->srv.get();
	context->PSSetShaderResources(53, 1, &view);
}

void LightLimitFix::SetLightPosition(LightLimitFix::LightData& a_light, RE::NiPoint3 a_initialPosition, bool a_cached)
{
	for (int eyeIndex = 0; eyeIndex < eyeCount; eyeIndex++) {
		RE::NiPoint3 eyePosition;
		Matrix viewMatrix;

		if (a_cached) {
			eyePosition = eyePositionCached[eyeIndex];
			viewMatrix = viewMatrixCached[eyeIndex];
		} else {
			eyePosition = Util::GetEyePosition(eyeIndex);
			viewMatrix = Util::GetCameraData(eyeIndex).viewMat;
		}

		auto worldPos = a_initialPosition - eyePosition;
		a_light.positionWS[eyeIndex].data.x = worldPos.x;
		a_light.positionWS[eyeIndex].data.y = worldPos.y;
		a_light.positionWS[eyeIndex].data.z = worldPos.z;
		a_light.positionVS[eyeIndex].data = DirectX::SimpleMath::Vector3::Transform(a_light.positionWS[eyeIndex].data, viewMatrix);
	}
}

float LightLimitFix::CalculateLuminance(CachedParticleLight& light, RE::NiPoint3& point)
{
	// See BSLight::CalculateLuminance_14131D3D0
	// Performs lighting on the CPU which is identical to GPU code

	auto lightDirection = light.position - point;
	float lightDist = lightDirection.Length();
	float intensityFactor = std::clamp(lightDist / light.radius, 0.0f, 1.0f);
	float intensityMultiplier = 1 - intensityFactor * intensityFactor;

	return light.grey * intensityMultiplier;
}

void LightLimitFix::AddParticleLightLuminance(RE::NiPoint3& targetPosition, int& numHits, float& lightLevel)
{
	std::lock_guard<std::shared_mutex> lk{ cachedParticleLightsMutex };
	particleLightsDetectionHits = 0;
	if (settings.EnableParticleLightsDetection) {
		for (auto& light : cachedParticleLights) {
			auto luminance = CalculateLuminance(light, targetPosition);
			lightLevel += luminance;
			if (luminance > 0.0)
				particleLightsDetectionHits++;
		}
	}
	numHits += particleLightsDetectionHits;
}

void LightLimitFix::Prepass()
{
	auto& context = State::GetSingleton()->context;

	UpdateLights();

	ID3D11ShaderResourceView* views[3]{};
	views[0] = lights->srv.get();
	views[1] = lightList->srv.get();
	views[2] = lightGrid->srv.get();
	context->PSSetShaderResources(50, ARRAYSIZE(views), views);
}

bool LightLimitFix::IsValidLight(RE::BSLight* a_light)
{
	return a_light && !a_light->light->GetFlags().any(RE::NiAVObject::Flag::kHidden);
}

bool LightLimitFix::IsGlobalLight(RE::BSLight* a_light)
{
	return !(a_light->portalStrict || !a_light->portalGraph);
}

struct VertexColor
{
	std::uint8_t data[4];
};

struct VertexPosition
{
	std::uint8_t data[3];
};

std::string ExtractTextureStem(std::string_view a_path)
{
	if (a_path.size() < 1)
		return {};

	auto lastSeparatorPos = a_path.find_last_of("\\/");
	if (lastSeparatorPos == std::string::npos)
		return {};

	a_path = a_path.substr(lastSeparatorPos + 1);
	a_path.remove_suffix(4);  // Remove ".dds"

	auto textureNameView = a_path | std::views::transform([](auto c) { return (char)::tolower(c); });
	std::string textureName = { textureNameView.begin(), textureNameView.end() };

	return textureName;
}

std::optional<LightLimitFix::ConfigPair> LightLimitFix::GetParticleLightConfigs(RE::BSRenderPass* a_pass)
{
	// see https://www.nexusmods.com/skyrimspecialedition/articles/1391
	if (settings.EnableParticleLights) {
		if (auto shaderProperty = netimmerse_cast<RE::BSEffectShaderProperty*>(a_pass->shaderProperty)) {
			if (!shaderProperty->lightData) {
				if (auto material = shaderProperty->GetMaterial()) {
					if (!material->sourceTexturePath.empty()) {
						std::string textureName = ExtractTextureStem(material->sourceTexturePath.c_str());
						if (textureName.size() < 1)
							return std::nullopt;

						auto& configs = ParticleLights::GetSingleton()->particleLightConfigs;
						auto it = configs.find(textureName);
						if (it == configs.end())
							return std::nullopt;

						ParticleLights::Config* config = &it->second;
						ParticleLights::GradientConfig* gradientConfig = nullptr;
						if (!material->greyscaleTexturePath.empty()) {
							textureName = ExtractTextureStem(material->greyscaleTexturePath.c_str());
							if (textureName.size() < 1)
								return std::nullopt;

							auto& gradientConfigs = ParticleLights::GetSingleton()->particleLightGradientConfigs;
							auto itGradient = gradientConfigs.find(textureName);
							if (itGradient == gradientConfigs.end())
								return std::nullopt;
							gradientConfig = &itGradient->second;
						}
						return std::make_pair(config, gradientConfig);
					}
				}
			}
		}
	}
	return std::nullopt;
}

bool LightLimitFix::CheckParticleLights(RE::BSRenderPass* a_pass, uint32_t)
{
	auto configs = GetParticleLightConfigs(a_pass);
	if (configs.has_value()) {
		if (AddParticleLight(a_pass, configs.value())) {
			return !(settings.EnableParticleLightsCulling && configs->first->cull);
		}
	}
	return true;
}

bool LightLimitFix::AddParticleLight(RE::BSRenderPass* a_pass, LightLimitFix::ConfigPair a_config)
{
	auto shaderProperty = netimmerse_cast<RE::BSEffectShaderProperty*>(a_pass->shaderProperty);
	auto material = shaderProperty->GetMaterial();
	auto config = a_config.first;
	auto gradientConfig = a_config.second;

	a_pass->geometry->IncRefCount();
	if (const auto particleSystem = netimmerse_cast<RE::NiParticleSystem*>(a_pass->geometry)) {
		if (auto particleData = particleSystem->GetParticleRuntimeData().particleData.get()) {
			particleData->IncRefCount();
		}
	}

	RE::NiColorA color;
	color.red = material->baseColor.red * material->baseColorScale;
	color.green = material->baseColor.green * material->baseColorScale;
	color.blue = material->baseColor.blue * material->baseColorScale;
	color.alpha = material->baseColor.alpha * shaderProperty->alpha;

	if (auto emittance = shaderProperty->unk88) {
		color.red *= emittance->red;
		color.green *= emittance->green;
		color.blue *= emittance->blue;
	}

	if (auto rendererData = a_pass->geometry->GetGeometryRuntimeData().rendererData) {
		if (auto triShape = a_pass->geometry->AsTriShape()) {
			uint32_t vertexSize = rendererData->vertexDesc.GetSize();
			if (rendererData->vertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_COLORS)) {
				uint32_t offset = rendererData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_COLOR);

				uint8_t maxAlpha = 0u;
				VertexColor* vertexColor = nullptr;
				bool alphaOne = false;
				bool alphaZero = false;

				for (int v = 0; v < triShape->GetTrishapeRuntimeData().vertexCount; v++) {
					if (VertexColor* vertex = reinterpret_cast<VertexColor*>(&rendererData->rawVertexData[vertexSize * v + offset])) {
						uint8_t alpha = vertex->data[3];
						alphaZero = alphaOne || alpha == 0;
						alphaOne = alphaOne || alpha == 255;
						if (alpha > maxAlpha) {
							maxAlpha = alpha;
							vertexColor = vertex;
						}
					}
				}

				if (!vertexColor || !alphaZero || !alphaOne)
					return false;

				color.red *= vertexColor->data[0] / 255.f;
				color.green *= vertexColor->data[1] / 255.f;
				color.blue *= vertexColor->data[2] / 255.f;
				if (shaderProperty->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kVertexAlpha)) {
					color.alpha *= vertexColor->data[3] / 255.f;
				}
			}
		}
	}

	if (gradientConfig) {
		auto grey = float3(config->colorMult.red, config->colorMult.green, config->colorMult.blue).Dot(float3(0.3f, 0.59f, 0.11f));
		color.red *= grey * gradientConfig->color.red;
		color.green *= grey * gradientConfig->color.green;
		color.blue *= grey * gradientConfig->color.blue;
	} else {
		color.red *= config->colorMult.red;
		color.green *= config->colorMult.green;
		color.blue *= config->colorMult.blue;
	}

	queuedParticleLights.insert({ a_pass->geometry, { color, *config } });
	return true;
}

void LightLimitFix::PostPostLoad()
{
	ParticleLights::GetSingleton()->GetConfigs();
	Hooks::Install();
}

void LightLimitFix::DataLoaded()
{
	auto iMagicLightMaxCount = RE::GameSettingCollection::GetSingleton()->GetSetting("iMagicLightMaxCount");
	iMagicLightMaxCount->data.i = MAXINT32;
	logger::info("[LLF] Unlocked magic light limit");
}

float LightLimitFix::CalculateLightDistance(float3 a_lightPosition, float a_radius)
{
	return (a_lightPosition.x * a_lightPosition.x) + (a_lightPosition.y * a_lightPosition.y) + (a_lightPosition.z * a_lightPosition.z) - (a_radius * a_radius);
}

void LightLimitFix::AddCachedParticleLights(eastl::vector<LightData>& lightsData, LightLimitFix::LightData& light)
{
	static float& lightFadeStart = *reinterpret_cast<float*>(REL::RelocationID(527668, 414582).address());
	static float& lightFadeEnd = *reinterpret_cast<float*>(REL::RelocationID(527669, 414583).address());

	float distance = CalculateLightDistance(light.positionWS[0].data, light.radius);

	float dimmer = 0.0f;

	if (distance < lightFadeStart || lightFadeEnd == 0.0f) {
		dimmer = 1.0f;
	} else if (distance <= lightFadeEnd) {
		dimmer = 1.0f - ((distance - lightFadeStart) / (lightFadeEnd - lightFadeStart));
	} else {
		dimmer = 0.0f;
	}

	light.color *= dimmer;

	if ((light.color.x + light.color.y + light.color.z) > 1e-4 && light.radius > 1e-4) {
		for (int eyeIndex = 0; eyeIndex < eyeCount; eyeIndex++)
			light.positionVS[eyeIndex].data = DirectX::SimpleMath::Vector3::Transform(light.positionWS[eyeIndex].data, viewMatrixCached[eyeIndex]);

		lightsData.push_back(light);

		CachedParticleLight cachedParticleLight{};
		cachedParticleLight.grey = float3(light.color.x, light.color.y, light.color.z).Dot(float3(0.3f, 0.59f, 0.11f));
		cachedParticleLight.radius = light.radius;
		cachedParticleLight.position = { light.positionWS[0].data.x + eyePositionCached[0].x, light.positionWS[0].data.y + eyePositionCached[0].y, light.positionWS[0].data.z + eyePositionCached[0].z };

		cachedParticleLights.push_back(cachedParticleLight);
	}
}

float3 LightLimitFix::Saturation(float3 color, float saturation)
{
	float grey = color.Dot(float3(0.3f, 0.59f, 0.11f));
	color.x = std::max(std::lerp(grey, color.x, saturation), 0.0f);
	color.y = std::max(std::lerp(grey, color.y, saturation), 0.0f);
	color.z = std::max(std::lerp(grey, color.z, saturation), 0.0f);
	return color;
}

void LightLimitFix::UpdateLights()
{
	static float& cameraNear = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x40));
	static float& cameraFar = (*(float*)(REL::RelocationID(517032, 403540).address() + 0x44));

	lightsNear = cameraNear;
	lightsFar = cameraFar;

	auto shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];

	// Cache data since cameraData can become invalid in first-person

	for (int eyeIndex = 0; eyeIndex < eyeCount; eyeIndex++) {
		eyePositionCached[eyeIndex] = Util::GetEyePosition(eyeIndex);
		viewMatrixCached[eyeIndex] = Util::GetCameraData(eyeIndex).viewMat;
		viewMatrixCached[eyeIndex].Invert(viewMatrixInverseCached[eyeIndex]);
	}

	eastl::vector<LightData> lightsData{};
	lightsData.reserve(MAX_LIGHTS);

	// Process point lights

	roomNodes.empty();

	auto addRoom = [&](void* nodePtr, LightData& light) {
		uint8_t roomIndex = 0;
		auto* node = static_cast<RE::NiNode*>(nodePtr);
		if (auto it = roomNodes.find(node); it == roomNodes.cend()) {
			roomIndex = static_cast<uint8_t>(roomNodes.size());
			roomNodes.insert_or_assign(node, roomIndex);
		} else {
			roomIndex = it->second;
		}
		light.roomFlags.SetBit(roomIndex, 1);
	};

	auto addLight = [&](const RE::NiPointer<RE::BSLight>& e) {
		if (auto bsLight = e.get()) {
			if (auto niLight = bsLight->light.get()) {
				if (IsValidLight(bsLight)) {
					auto& runtimeData = niLight->GetLightRuntimeData();

					LightData light{};
					light.color = { runtimeData.diffuse.red, runtimeData.diffuse.green, runtimeData.diffuse.blue };
					light.color *= runtimeData.fade;
					light.color *= bsLight->lodDimmer;

					light.radius = runtimeData.radius.x;

					if (!IsGlobalLight(bsLight)) {
						// List of BSMultiBoundRooms affected by a light
						for (const auto& roomPtr : bsLight->unk0D8) {
							addRoom(roomPtr, light);
						}
						// List of BSPortals affected by a light
						for (const auto& portalPtr : bsLight->unk0F0) {
							struct BSPortal
							{
								uint8_t data[0x128];
								void* portalSharedNode;
							};
							addRoom(static_cast<BSPortal*>(portalPtr)->portalSharedNode, light);
						}
						light.lightFlags.set(LightFlags::PortalStrict);
					}

					if (bsLight->IsShadowLight()) {
						auto* shadowLight = static_cast<RE::BSShadowLight*>(bsLight);
						GET_INSTANCE_MEMBER(shadowLightIndex, shadowLight);
						light.shadowMaskIndex = shadowLightIndex;
						light.lightFlags.set(LightFlags::Shadow);
					}

					SetLightPosition(light, niLight->world.translate);

					if ((light.color.x + light.color.y + light.color.z) > 1e-4 && light.radius > 1e-4) {
						lightsData.push_back(light);
					}
				}
			}
		}
	};

	for (auto& e : shadowSceneNode->GetRuntimeData().activeLights) {
		addLight(e);
	}
	for (auto& e : shadowSceneNode->GetRuntimeData().activeShadowLights) {
		addLight(e);
	}

	{
		std::lock_guard<std::shared_mutex> lk{ cachedParticleLightsMutex };
		cachedParticleLights.clear();

		LightData clusteredLight{};
		uint32_t clusteredLights = 0;

		auto eyePositionOffset = eyePositionCached[0] - eyePositionCached[1];

		for (const auto& particleLight : particleLights) {
			if (const auto particleSystem = netimmerse_cast<RE::NiParticleSystem*>(particleLight.first);
				particleSystem && particleSystem->GetParticleRuntimeData().particleData.get()) {
				// Process BSGeometry
				auto particleData = particleSystem->GetParticleRuntimeData().particleData.get();

				auto numVertices = particleData->GetActiveVertexCount();
				for (std::uint32_t p = 0; p < numVertices; p++) {
					float radius = particleData->GetParticlesRuntimeData().sizes[p] * 70.0f;

					auto initialPosition = particleData->GetParticlesRuntimeData().positions[p];
					if (!particleSystem->GetParticleSystemRuntimeData().isWorldspace) {
						// Detect first-person meshes
						if ((particleLight.first->GetModelData().modelBound.radius * particleLight.first->world.scale) != particleLight.first->worldBound.radius)
							initialPosition += particleLight.first->worldBound.center;
						else
							initialPosition += particleLight.first->world.translate;
					}

					RE::NiPoint3 positionWS = initialPosition - eyePositionCached[0];

					if (clusteredLights) {
						auto averageRadius = clusteredLight.radius / (float)clusteredLights;
						float radiusDiff = abs(averageRadius - radius);

						auto averagePosition = clusteredLight.positionWS[0].data / (float)clusteredLights;
						float positionDiff = positionWS.GetDistance({ averagePosition.x, averagePosition.y, averagePosition.z });

						if ((radiusDiff + positionDiff) > settings.ParticleLightsOptimisationClusterRadius || !settings.EnableParticleLightsOptimization) {
							clusteredLight.radius /= (float)clusteredLights;
							clusteredLight.positionWS[0].data /= (float)clusteredLights;
							clusteredLight.positionWS[1].data = clusteredLight.positionWS[0].data;
							if (eyeCount == 2) {
								clusteredLight.positionWS[1].data.x += eyePositionOffset.x / (float)clusteredLights;
								clusteredLight.positionWS[1].data.y += eyePositionOffset.y / (float)clusteredLights;
								clusteredLight.positionWS[1].data.z += eyePositionOffset.z / (float)clusteredLights;
							}

							AddCachedParticleLights(lightsData, clusteredLight);

							clusteredLights = 0;
							clusteredLight.color = { 0, 0, 0 };
							clusteredLight.radius = 0;
							clusteredLight.positionWS[0].data = { 0, 0, 0 };
						}
					}

					float alpha = particleLight.second.color.alpha * particleData->GetParticlesRuntimeData().color[p].alpha;
					float3 color;
					color.x = particleLight.second.color.red * particleData->GetParticlesRuntimeData().color[p].red;
					color.y = particleLight.second.color.green * particleData->GetParticlesRuntimeData().color[p].green;
					color.z = particleLight.second.color.blue * particleData->GetParticlesRuntimeData().color[p].blue;
					clusteredLight.color += Saturation(color, settings.ParticleLightsSaturation) * alpha * settings.ParticleBrightness;

					clusteredLight.radius += radius * settings.ParticleRadius * particleLight.second.config.radiusMult;
					clusteredLight.positionWS[0].data.x += positionWS.x;
					clusteredLight.positionWS[0].data.y += positionWS.y;
					clusteredLight.positionWS[0].data.z += positionWS.z;

					clusteredLights++;
				}

			} else {
				// Process billboard
				LightData light{};

				light.color.x = particleLight.second.color.red;
				light.color.y = particleLight.second.color.green;
				light.color.z = particleLight.second.color.blue;

				light.color = Saturation(light.color, settings.ParticleLightsSaturation);

				light.color *= particleLight.second.color.alpha * settings.BillboardBrightness;
				light.radius = particleLight.first->worldBound.radius * settings.BillboardRadius * particleLight.second.config.radiusMult;

				auto position = particleLight.first->world.translate;

				SetLightPosition(light, position);  // Light is complete for both eyes by now

				AddCachedParticleLights(lightsData, light);
			}
		}

		if (clusteredLights) {
			clusteredLight.radius /= (float)clusteredLights;
			clusteredLight.positionWS[0].data /= (float)clusteredLights;
			clusteredLight.positionWS[1].data = clusteredLight.positionWS[0].data;
			if (eyeCount == 2) {
				clusteredLight.positionWS[1].data.x += eyePositionOffset.x / (float)clusteredLights;
				clusteredLight.positionWS[1].data.y += eyePositionOffset.y / (float)clusteredLights;
				clusteredLight.positionWS[1].data.z += eyePositionOffset.z / (float)clusteredLights;
			}
			AddCachedParticleLights(lightsData, clusteredLight);
		}
	}

	static auto& context = State::GetSingleton()->context;

	{
		auto projMatrixUnjittered = Util::GetCameraData(0).projMatrixUnjittered;
		float fov = atan(1.0f / static_cast<float4x4>(projMatrixUnjittered).m[0][0]) * 2.0f * (180.0f / 3.14159265359f);

		static float _lightsNear = 0.0f, _lightsFar = 0.0f, _fov = 0.0f;
		if (fabs(_fov - fov) > 1e-4 || fabs(_lightsNear - lightsNear) > 1e-4 || fabs(_lightsFar - lightsFar) > 1e-4) {
			LightBuildingCB updateData{};
			updateData.InvProjMatrix[0] = DirectX::XMMatrixInverse(nullptr, projMatrixUnjittered);
			if (eyeCount == 1)
				updateData.InvProjMatrix[1] = updateData.InvProjMatrix[0];
			else
				updateData.InvProjMatrix[1] = DirectX::XMMatrixInverse(nullptr, Util::GetCameraData(1).projMatrixUnjittered);
			updateData.LightsNear = lightsNear;
			updateData.LightsFar = lightsFar;

			lightBuildingCB->Update(updateData);

			ID3D11Buffer* buffer = lightBuildingCB->CB();
			context->CSSetConstantBuffers(0, 1, &buffer);

			ID3D11UnorderedAccessView* clusters_uav = clusters->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &clusters_uav, nullptr);

			context->CSSetShader(clusterBuildingCS, nullptr, 0);
			context->Dispatch(clusterSize[0], clusterSize[1], clusterSize[2]);

			ID3D11UnorderedAccessView* null_uav = nullptr;
			context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);

			_fov = fov;
			_lightsNear = lightsNear;
			_lightsFar = lightsFar;
		}
	}

	{
		lightCount = std::min((uint)lightsData.size(), MAX_LIGHTS);

		D3D11_MAPPED_SUBRESOURCE mapped;
		DX::ThrowIfFailed(context->Map(lights->resource.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		size_t bytes = sizeof(LightData) * lightCount;
		memcpy_s(mapped.pData, bytes, lightsData.data(), bytes);
		context->Unmap(lights->resource.get(), 0);

		LightCullingCB updateData{};
		updateData.LightCount = lightCount;
		lightCullingCB->Update(updateData);

		ID3D11Buffer* buffer = lightCullingCB->CB();
		context->CSSetConstantBuffers(0, 1, &buffer);

		ID3D11ShaderResourceView* srvs[] = { clusters->srv.get(), lights->srv.get() };
		context->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { lightCounter->uav.get(), lightList->uav.get(), lightGrid->uav.get() };
		context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);

		context->CSSetShader(clusterCullingCS, nullptr, 0);
		context->Dispatch((clusterSize[0] + 15) / 16, (clusterSize[1] + 15) / 16, (clusterSize[2] + 3) / 4);
	}

	context->CSSetShader(nullptr, nullptr, 0);

	ID3D11Buffer* null_buffer = nullptr;
	context->CSSetConstantBuffers(0, 1, &null_buffer);

	ID3D11ShaderResourceView* null_srvs[2] = { nullptr };
	context->CSSetShaderResources(0, 2, null_srvs);

	ID3D11UnorderedAccessView* null_uavs[3] = { nullptr };
	context->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
}
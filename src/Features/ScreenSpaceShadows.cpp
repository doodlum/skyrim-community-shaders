#include "ScreenSpaceShadows.h"

#include "Features/TerrainBlending.h"
#include "I18n/I18n.h"
#include "State.h"
#include "Utils/D3D.h"

#define I18N_KEY_PREFIX "feature.screen_space_shadows."

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	ScreenSpaceShadows::Settings,
	Enabled,
	BlurDepthPyramid,
	SurfaceThickness,
	ShadowContrast,
	RayLength,
	SampleCount)

void ScreenSpaceShadows::DrawSettings()
{
	if (ImGui::TreeNodeEx(T(TKEY("general"), "General"), ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox(T(TKEY("enable"), "Enable"), &settings.Enabled);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("enable_tooltip"), "Enable screen-space contact shadows from the sun/moon direction."));

		ImGui::SliderFloat(T(TKEY("surface_thickness"), "Surface Thickness"), &settings.SurfaceThickness, 0.1f, 20.0f);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("surface_thickness_world_tooltip"), "Assumed surface thickness for the occlusion test, in view-space world units."));

		ImGui::SliderFloat(T(TKEY("shadow_contrast"), "Shadow Contrast"), &settings.ShadowContrast, 0.0f, 4.0f);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("shadow_contrast_tooltip"), "Contrast boost for the shadow transition. Higher values produce harder edges."));

		ImGui::SliderFloat(T(TKEY("ray_length"), "Ray Length"), &settings.RayLength, 1.0f, 2000.0f);
		if (ImGui::IsItemActive())
			shaderCompilationDelayFrames = kShaderCompilationDebounceFrames;
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("ray_length_tooltip"), "World-space distance assigned to the longest cascade segment. The full ray reaches 1.875 times this distance."));

		ImGui::SliderInt(T(TKEY("cascade_sample_count"), "Cascade Sample Count"), &settings.SampleCount, kMinBaseSamples, kMaxBaseSamples);
		if (ImGui::IsItemActive())
			shaderCompilationDelayFrames = kShaderCompilationDebounceFrames;
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("cascade_sample_count_tooltip"),
								  "Sample count for the longest cascade segment at a Ray Length of 100. It scales with Ray Length; the three nearer segments use one-half, one-quarter, and one-eighth as many samples."));

		ImGui::Checkbox(T(TKEY("blur_depth_pyramid"), "Blur Depth Pyramid"), &settings.BlurDepthPyramid);
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("%s", T(TKEY("blur_depth_pyramid_tooltip"),
								  "Smooth each depth-pyramid level before ray marching. This reduces depth discontinuities at a small extra GPU cost."));

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(T(TKEY("buffer_viewer"), "Buffer Viewer"))) {
		static float debugRescale = 0.3f;
		ImGui::SliderFloat(T(TKEY("view_resize"), "View Resize"), &debugRescale, 0.0f, 1.0f);

		for (int i = 0; i < 4; ++i) {
			auto title = std::vformat(T(TKEY("depth_prefiltered_mip"), "Depth Prefiltered - Mip {}"), std::make_format_args(i));
			BUFFER_VIEWER_NODE_TITLE(texDepthMipPrefiltered[i], title.c_str(), debugRescale)
		}
		for (int i = 0; i < 4; ++i) {
			auto title = std::vformat(T(TKEY("depth_blurred_mip"), "Depth Blurred - Mip {}"), std::make_format_args(i));
			BUFFER_VIEWER_NODE_TITLE(texDepthMip[i], title.c_str(), debugRescale)
		}
		for (int i = 0; i < 4; ++i) {
			auto title = std::vformat(T(TKEY("shadow_marched_mip"), "Shadow Marched - Mip {}"), std::make_format_args(i));
			BUFFER_VIEWER_NODE_TITLE(texShadowMip[i], title.c_str(), debugRescale)
		}
		for (int i = 0; i < 4; ++i) {
			auto title = std::vformat(T(TKEY("shadow_work_mip"), "Shadow Work - Mip {}"), std::make_format_args(i));
			BUFFER_VIEWER_NODE_TITLE(texShadowWork[i], title.c_str(), debugRescale)
		}
		BUFFER_VIEWER_NODE_TITLE(screenSpaceShadowsTexture, T(TKEY("shadow_final_blurred"), "Shadow Final (blurred)"), debugRescale)

		ImGui::TreePop();
	}
}

void ScreenSpaceShadows::ClearShaderCache()
{
	prefilterDepthsCS = nullptr;
	blurDepthCS = nullptr;
	for (int i = 0; i < 4; ++i)
		shadowsCS[i] = nullptr;
	upscaleCS = nullptr;
	blurCS = nullptr;
	compiledBaseSampleCount = -1;
	attemptedBaseSampleCount = -1;
	CompileComputeShaders();
}

void ScreenSpaceShadows::CompilePrefilterDepthsCS()
{
	prefilterDepthsCS = nullptr;
	prefilterUsesTerrainBlending = globals::features::terrainBlending.loaded && globals::features::terrainBlending.settings.Enabled;

	std::vector<std::pair<const char*, const char*>> prefilterDefines;
	if (prefilterUsesTerrainBlending)
		prefilterDefines.push_back({ "TERRAIN_BLENDING", "" });

	if (auto* cs = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(
			L"Data\\Shaders\\ScreenSpaceShadows\\PrefilterDepthsCS.hlsl", prefilterDefines, "cs_5_0")))
		prefilterDepthsCS.attach(cs);
}

void ScreenSpaceShadows::CompileComputeShaders()
{
	CompilePrefilterDepthsCS();

	auto compile = [&](std::wstring_view path) -> ID3D11ComputeShader* {
		return reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(path.data(), {}, "cs_5_0"));
	};

	if (auto* cs = compile(L"Data\\Shaders\\ScreenSpaceShadows\\BlurDepthCS.hlsl"))
		blurDepthCS.attach(cs);

	// ShadowsCS is compiled lazily via CompileShadowsCS() so the MIP_SAMPLE_COUNT
	// define matches the current settings.

	if (auto* cs = compile(L"Data\\Shaders\\ScreenSpaceShadows\\UpscaleCS.hlsl"))
		upscaleCS.attach(cs);

	if (auto* cs = compile(L"Data\\Shaders\\ScreenSpaceShadows\\BlurCS.hlsl"))
		blurCS.attach(cs);
}

void ScreenSpaceShadows::CompileShadowsCS(int baseSampleCount)
{
	if (baseSampleCount == compiledBaseSampleCount || baseSampleCount == attemptedBaseSampleCount)
		return;
	attemptedBaseSampleCount = baseSampleCount;
	compiledBaseSampleCount = -1;

	auto compile = [&](std::vector<std::pair<const char*, const char*>> defines) -> ID3D11ComputeShader* {
		return reinterpret_cast<ID3D11ComputeShader*>(
			Util::CompileShader(L"Data\\Shaders\\ScreenSpaceShadows\\ShadowsCS.hlsl", defines, "cs_5_0"));
	};

	winrt::com_ptr<ID3D11ComputeShader> compiledShaders[4];

	// Mip 3 carries the base sample count; each step toward mip 0 halves it.
	for (int mip = 0; mip < 4; ++mip) {
		int samples = std::max(1, baseSampleCount >> (3 - mip));

		char sampleCountStr[16];
		snprintf(sampleCountStr, sizeof(sampleCountStr), "%d", samples);

		std::vector<std::pair<const char*, const char*>> defines;
		defines.push_back({ "MIP_SAMPLE_COUNT", sampleCountStr });

		if (auto* cs = compile(defines))
			compiledShaders[mip].attach(cs);
	}

	const bool compiledAllVariants = std::ranges::all_of(compiledShaders, [](const auto& shader) { return static_cast<bool>(shader); });
	if (compiledAllVariants) {
		for (int mip = 0; mip < 4; ++mip)
			shadowsCS[mip] = std::move(compiledShaders[mip]);
		compiledBaseSampleCount = baseSampleCount;
	}
}

void ScreenSpaceShadows::DrawShadows()
{
	ZoneScopedS(8);
	auto state = globals::state;
	TracyD3D11Zone(state->tracyCtx, "Screen Space Shadows");

	auto context = globals::d3d::context;

	auto accumulator = *globals::game::currentAccumulator.get();
	if (!accumulator)
		return;

	auto shadowSceneNode = accumulator->GetRuntimeData().activeShadowSceneNode;
	if (!shadowSceneNode || !shadowSceneNode->GetRuntimeData().sunLight)
		return;

	auto dirLight = skyrim_cast<RE::NiDirectionalLight*>(shadowSceneNode->GetRuntimeData().sunLight->light.get());
	if (!dirLight)
		return;

	auto& directionNi = dirLight->GetWorldDirection();
	float3 light = { directionNi.x, directionNi.y, directionNi.z };
	light.Normalize();

	const float2 screenSize = { (float)globals::game::graphicsState->screenWidth, (float)globals::game::graphicsState->screenHeight };
	const float2 renderSize = Util::ConvertToDynamic(screenSize);
	if (renderSize.x <= 0.0f || renderSize.y <= 0.0f)
		return;

	const bool usesTerrainBlendingDepth = globals::features::terrainBlending.loaded && globals::features::terrainBlending.settings.Enabled;
	if (usesTerrainBlendingDepth != prefilterUsesTerrainBlending)
		CompilePrefilterDepthsCS();

	if (!sssCB || !pointClampSampler || !prefilterDepthsCS || !upscaleCS || !blurCS || !screenSpaceShadowsTexture)
		return;
	for (int mip = 0; mip < 4; ++mip) {
		if (!texDepthMipPrefiltered[mip] || !texDepthMip[mip] || !texShadowMip[mip] || !texShadowWork[mip])
			return;
	}

	float2 texDim = { (float)texShadowMip[0]->desc.Width, (float)texShadowMip[0]->desc.Height };

	SSSCB cbData{};
	cbData.FrameDim = renderSize;
	cbData.RcpTexDim = { 1.0f / texDim.x, 1.0f / texDim.y };
	cbData.TexDim = texDim;
	cbData.DynamicRes = { renderSize.x / texDim.x, renderSize.y / texDim.y };

	const Settings defaultSettings{};
	const float surfaceThickness = std::isfinite(settings.SurfaceThickness) ? std::clamp(settings.SurfaceThickness, 0.1f, 20.0f) : defaultSettings.SurfaceThickness;
	const float shadowContrast = std::isfinite(settings.ShadowContrast) ? std::clamp(settings.ShadowContrast, 0.0f, 4.0f) : defaultSettings.ShadowContrast;
	const float rayLength = std::isfinite(settings.RayLength) ? std::clamp(settings.RayLength, 1.0f, 2000.0f) : defaultSettings.RayLength;
	const int requestedSampleCount = std::clamp(settings.SampleCount, kMinBaseSamples, kMaxBaseSamples);

	cbData.SurfaceThickness = surfaceThickness;
	// Thickness keeps growing across the whole cascade and reaches the max at the
	// very last sample — the end of mip 3.  Total reach = RayLength * (1/8 + 1/4 + 1/2 + 1).
	cbData.MaxThicknessDistance = rayLength * 15.0f / 8.0f;
	cbData.LightWorldDir = { -light.x, -light.y, -light.z };
	cbData.ShadowContrast = shadowContrast;

	// Mip 3 carries the highest sample count; mip 0 the lowest.  Scale by ray length
	// to hold sample density (samples per world unit) roughly constant.
	const float scaled = requestedSampleCount * rayLength / kReferenceRayLength;
	const int baseSampleCount = std::clamp(static_cast<int>(std::round(scaled)), kMinBaseSamples, kMaxBaseSamples);
	// MIP_SAMPLE_COUNT is a compile-time define — recompile ShadowsCS variants if it changed.
	if (shaderCompilationDelayFrames > 0)
		--shaderCompilationDelayFrames;
	if (shaderCompilationDelayFrames == 0 || compiledBaseSampleCount < 0)
		CompileShadowsCS(baseSampleCount);
	for (const auto& shader : shadowsCS)
		if (!shader)
			return;

	auto* srcDepthSRV = Util::GetCurrentSceneDepthSRV(false);
	if (!srcDepthSRV)
		return;

	const bool useBlurredDepth = settings.BlurDepthPyramid && blurDepthCS;
	globals::profiler->BeginPass("ScreenSpaceShadows::DrawShadows");

	sssCB->Update(cbData);
	auto* cbPtr = sssCB->CB();
	context->CSSetConstantBuffers(1, 1, &cbPtr);
	winrt::com_ptr<ID3D11Buffer> previousSharedDataCB;
	context->CSGetConstantBuffers(5, 1, previousSharedDataCB.put());
	auto* sharedDataCB = state->sharedDataCB->CB();
	context->CSSetConstantBuffers(5, 1, &sharedDataCB);

	winrt::com_ptr<ID3D11SamplerState> previousPointSampler;
	context->CSGetSamplers(1, 1, previousPointSampler.put());
	auto* pointSampler = pointClampSampler.get();
	context->CSSetSamplers(1, 1, &pointSampler);

	// === Depth pyramid — prefilter game depth into 4 mip levels ===
	{
		if (state->frameAnnotations)
			state->BeginPerfEvent("SSS - Prefilter Depths");

		ID3D11UnorderedAccessView* depthUAVs[4] = {
			texDepthMipPrefiltered[0]->uav.get(),
			texDepthMipPrefiltered[1]->uav.get(),
			texDepthMipPrefiltered[2]->uav.get(),
			texDepthMipPrefiltered[3]->uav.get()
		};
		context->CSSetShaderResources(0, 1, &srcDepthSRV);
		context->CSSetUnorderedAccessViews(0, 4, depthUAVs, nullptr);
		context->CSSetShader(prefilterDepthsCS.get(), nullptr, 0);
		// Each thread handles a 2x2 full-res block, so dispatch at half resolution.
		uint pfW = (static_cast<uint>(renderSize.x) + 15u) / 16u;
		uint pfH = (static_cast<uint>(renderSize.y) + 15u) / 16u;
		context->Dispatch(pfW, pfH, 1);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		ID3D11UnorderedAccessView* nullUAVs[4] = { nullptr, nullptr, nullptr, nullptr };
		context->CSSetShaderResources(0, 1, &nullSRV);
		context->CSSetUnorderedAccessViews(0, 4, nullUAVs, nullptr);

		if (state->frameAnnotations)
			state->EndPerfEvent();
	}

	// === Optional depth pyramid smoothing — blur each mip after the full chain is built. ===
	// PrefilterDepthsCS has already produced all 4 mip levels; we now blur each independently.
	if (useBlurredDepth) {
		auto runBlurDepthCS = [&](Texture2D* src, Texture2D* dst, uint mip) {
			cbData.CurrentMip = mip;
			sssCB->Update(cbData);
			ID3D11ShaderResourceView* srv = src->srv.get();
			ID3D11UnorderedAccessView* uav = dst->uav.get();
			context->CSSetShaderResources(0, 1, &srv);
			context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
			context->CSSetShader(blurDepthCS.get(), nullptr, 0);
			uint w = std::max(1u, uint(renderSize.x) >> mip);
			uint h = std::max(1u, uint(renderSize.y) >> mip);
			context->Dispatch((w + 7u) >> 3, (h + 7u) >> 3, 1);
			ID3D11ShaderResourceView* nullSRV2 = nullptr;
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			context->CSSetShaderResources(0, 1, &nullSRV2);
			context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		};

		if (state->frameAnnotations)
			state->BeginPerfEvent("SSS - Depth Pyramid Blur");

		// BlurDepthCS reads the raw prefiltered depth and writes the blurred copy
		// into texDepthMip; no ping-pong needed since the source is preserved.
		for (uint i = 0; i < 4u; ++i) {
			runBlurDepthCS(texDepthMipPrefiltered[i].get(), texDepthMip[i].get(), i);
		}

		if (state->frameAnnotations)
			state->EndPerfEvent();
	}

	// === Shadow marching — cascaded ray segments along a single ray per pixel ===
	// CPU drives the cascade.  Mip 0 dispatches first and marches the segment at
	// the start position (the receiver) with the fewest samples.  Each step toward
	// mip 3 walks farther along the ray toward the light, doubling both segment
	// length and sample count, so mip 3 covers the largest stretch near the light
	// end.  Sample density (samples / length) stays constant across mips.
	// Total ray reach = RayLength * (1/8 + 1/4 + 1/2 + 1) = RayLength * 1.875.
	{
		float segmentStart = 0.0f;
		for (int mip = 0; mip < 4; ++mip) {
			float segmentLength = rayLength / float(1 << (3 - mip));

			uint mipW = std::max(1u, uint(renderSize.x) >> mip);
			uint mipH = std::max(1u, uint(renderSize.y) >> mip);

			cbData.CurrentMip = (uint)mip;
			cbData.SegmentStart = segmentStart;
			cbData.SegmentLength = segmentLength;
			sssCB->Update(cbData);

			auto* sampleDepthSRV = useBlurredDepth ? texDepthMip[mip]->srv.get() : texDepthMipPrefiltered[mip]->srv.get();
			ID3D11ShaderResourceView* depthSRVs[2] = {
				sampleDepthSRV,
				texDepthMipPrefiltered[0]->srv.get()
			};
			context->CSSetShaderResources(0, 2, depthSRVs);

			auto* shadowUAV = texShadowMip[mip]->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &shadowUAV, nullptr);

			if (state->frameAnnotations)
				state->BeginPerfEvent(std::format("SSS - Shadows Mip{}", mip));
			context->CSSetShader(shadowsCS[mip].get(), nullptr, 0);
			context->Dispatch((mipW + 7u) >> 3, (mipH + 7u) >> 3, 1);
			if (state->frameAnnotations)
				state->EndPerfEvent();

			segmentStart += segmentLength;
		}

		ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetShaderResources(0, 2, nullSRVs);
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	// === Cascaded blur → upscale chain (mip 3 down to 1, then final blur at mip 0) ===
	// Pipeline per iteration: blur mip N → upscale+min-combine into mip N-1.
	// Blur ping-pongs between texShadowWork and texShadowMip to avoid SRV/UAV hazards:
	//   mip 3: blurIn=texShadowMip[3], blurOut=texShadowWork[3]
	//   mip 2: blurIn=texShadowWork[2] (upscale output), blurOut=texShadowMip[2] (repurposed)
	//   mip 1: blurIn=texShadowWork[1], blurOut=texShadowMip[1] (repurposed)
	for (int mip = 3; mip >= 1; --mip) {
		uint mipW = std::max(1u, uint(renderSize.x) >> mip);
		uint mipH = std::max(1u, uint(renderSize.y) >> mip);
		uint outW = std::max(1u, uint(renderSize.x) >> (mip - 1));
		uint outH = std::max(1u, uint(renderSize.y) >> (mip - 1));

		cbData.CurrentMip = (uint)mip;
		sssCB->Update(cbData);

		// For mip 3 the march output is in texShadowMip; for mip < 3 the upscale wrote texShadowWork.
		Texture2D* blurInTex = (mip == 3) ? texShadowMip[mip].get() : texShadowWork[mip].get();
		// Blur output ping-pong: mip 3 → texShadowWork[3]; mip < 3 → repurpose texShadowMip[mip].
		Texture2D* blurOutTex = (mip == 3) ? texShadowWork[mip].get() : texShadowMip[mip].get();

		if (blurCS) {
			if (state->frameAnnotations)
				state->BeginPerfEvent(std::format("SSS - Blur Mip{}", mip));
			ID3D11ShaderResourceView* blurSRVs[2] = { blurInTex->srv.get(), texDepthMipPrefiltered[mip]->srv.get() };
			auto* outUAV = blurOutTex->uav.get();
			context->CSSetShaderResources(0, 2, blurSRVs);
			context->CSSetUnorderedAccessViews(0, 1, &outUAV, nullptr);
			context->CSSetShader(blurCS.get(), nullptr, 0);
			context->Dispatch((mipW + 7u) >> 3, (mipH + 7u) >> 3, 1);
			ID3D11ShaderResourceView* nullSRVs2[2] = { nullptr, nullptr };
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			context->CSSetShaderResources(0, 2, nullSRVs2);
			context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			if (state->frameAnnotations)
				state->EndPerfEvent();
		}

		if (upscaleCS) {
			if (state->frameAnnotations)
				state->BeginPerfEvent(std::format("SSS - Upscale Mip{}→{}", mip, mip - 1));
			ID3D11ShaderResourceView* srvs[4] = {
				blurOutTex->srv.get(),                       // t0: blurred shadow at CurrentMip
				texDepthMipPrefiltered[mip]->srv.get(),      // t1: depth at CurrentMip (unused)
				texShadowMip[mip - 1]->srv.get(),            // t2: marched shadow at CurrentMip-1
				texDepthMipPrefiltered[mip - 1]->srv.get(),  // t3: depth at CurrentMip-1 (unused)
			};
			context->CSSetShaderResources(0, 4, srvs);
			auto* outUAV = texShadowWork[mip - 1]->uav.get();
			context->CSSetUnorderedAccessViews(0, 1, &outUAV, nullptr);
			context->CSSetShader(upscaleCS.get(), nullptr, 0);
			context->Dispatch((outW + 7u) >> 3, (outH + 7u) >> 3, 1);
			ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
			ID3D11UnorderedAccessView* nullUAV = nullptr;
			context->CSSetShaderResources(0, 4, nullSRVs);
			context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			if (state->frameAnnotations)
				state->EndPerfEvent();
		}
	}

	// === Final blur at mip 0 (full resolution) — writes the final shadow mask. ===
	{
		cbData.CurrentMip = 0;
		sssCB->Update(cbData);

		if (state->frameAnnotations)
			state->BeginPerfEvent("SSS - Blur Mip0");
		ID3D11ShaderResourceView* finalBlurSRVs[2] = { texShadowWork[0]->srv.get(), texDepthMipPrefiltered[0]->srv.get() };
		auto* outUAV = screenSpaceShadowsTexture->uav.get();
		context->CSSetShaderResources(0, 2, finalBlurSRVs);
		context->CSSetUnorderedAccessViews(0, 1, &outUAV, nullptr);
		context->CSSetShader(blurCS.get(), nullptr, 0);
		context->Dispatch((uint(renderSize.x) + 7u) >> 3, (uint(renderSize.y) + 7u) >> 3, 1);
		ID3D11ShaderResourceView* nullSRVs2[2] = { nullptr, nullptr };
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetShaderResources(0, 2, nullSRVs2);
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		if (state->frameAnnotations)
			state->EndPerfEvent();
	}

	// Clean up
	auto* previousPointSamplerPtr = previousPointSampler.get();
	ID3D11Buffer* nullCB = nullptr;
	context->CSSetSamplers(1, 1, &previousPointSamplerPtr);
	context->CSSetConstantBuffers(1, 1, &nullCB);
	auto* previousSharedDataCBPtr = previousSharedDataCB.get();
	context->CSSetConstantBuffers(5, 1, &previousSharedDataCBPtr);
	context->CSSetShader(nullptr, nullptr, 0);
	globals::profiler->EndPass();
}

void ScreenSpaceShadows::Prepass()
{
	auto context = globals::d3d::context;

	float white[4] = { 1, 1, 1, 1 };
	for (int i = 0; i < 4; ++i)
		context->ClearUnorderedAccessViewFloat(texShadowMip[i]->uav.get(), white);
	for (int i = 0; i < 4; ++i)
		context->ClearUnorderedAccessViewFloat(texShadowWork[i]->uav.get(), white);
	context->ClearUnorderedAccessViewFloat(screenSpaceShadowsTexture->uav.get(), white);

	if (auto sky = globals::game::sky)
		if (settings.Enabled && sky->mode.get() == RE::Sky::Mode::kFull) {
			DrawShadows();
		}

	auto* view = screenSpaceShadowsTexture->srv.get();
	context->PSSetShaderResources(45, 1, &view);
}

void ScreenSpaceShadows::LoadSettings(json& o_json)
{
	Settings loadedSettings{};

	try {
		if (!o_json.is_object()) {
			logger::warn("Screen Space Shadows settings must be a JSON object; restoring defaults");
			settings = {};
			return;
		}

		const bool isLegacySettings = !o_json.contains("Enabled") && o_json.contains("Enable");
		if (isLegacySettings) {
			const auto& enabled = o_json["Enable"];
			if (enabled.is_boolean())
				loadedSettings.Enabled = enabled.get<bool>();
			else if (enabled.is_number_integer())
				loadedSettings.Enabled = enabled.get<int>() != 0;

			if (o_json.contains("SurfaceThickness") && o_json["SurfaceThickness"].is_number())
				loadedSettings.SurfaceThickness = o_json["SurfaceThickness"].get<float>() * 100.0f;
			if (o_json.contains("ShadowContrast") && o_json["ShadowContrast"].is_number())
				loadedSettings.ShadowContrast = o_json["ShadowContrast"].get<float>();
			if (o_json.contains("SampleCount") && o_json["SampleCount"].is_number_integer()) {
				const auto legacyMultiplier = std::clamp<std::int64_t>(o_json["SampleCount"].get<std::int64_t>(), 1, 8);
				loadedSettings.SampleCount = static_cast<int>(legacyMultiplier) * 16;
			}
		} else {
			loadedSettings = o_json.get<Settings>();
		}
	} catch (const json::exception& e) {
		logger::warn("Invalid Screen Space Shadows settings ({}); restoring defaults", e.what());
		loadedSettings = {};
	}

	const Settings defaults{};
	loadedSettings.SurfaceThickness = std::isfinite(loadedSettings.SurfaceThickness) ? std::clamp(loadedSettings.SurfaceThickness, 0.1f, 20.0f) : defaults.SurfaceThickness;
	loadedSettings.ShadowContrast = std::isfinite(loadedSettings.ShadowContrast) ? std::clamp(loadedSettings.ShadowContrast, 0.0f, 4.0f) : defaults.ShadowContrast;
	loadedSettings.RayLength = std::isfinite(loadedSettings.RayLength) ? std::clamp(loadedSettings.RayLength, 1.0f, 2000.0f) : defaults.RayLength;
	loadedSettings.SampleCount = std::clamp(loadedSettings.SampleCount, kMinBaseSamples, kMaxBaseSamples);
	settings = loadedSettings;
}

void ScreenSpaceShadows::SaveSettings(json& o_json)
{
	o_json = settings;
}

void ScreenSpaceShadows::RestoreDefaultSettings()
{
	settings = {};
}

bool ScreenSpaceShadows::HasShaderDefine(RE::BSShader::Type)
{
	return true;
}

void ScreenSpaceShadows::SetupResources()
{
	auto device = globals::d3d::device;
	auto renderer = globals::game::renderer;

	sssCB = eastl::make_unique<ConstantBuffer>(ConstantBufferDesc<SSSCB>(), "SSS::CB");

	{
		D3D11_SAMPLER_DESC samplerDesc = {
			.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
			.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
			.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
			.MaxAnisotropy = 1,
			.MinLOD = 0,
			.MaxLOD = D3D11_FLOAT32_MAX
		};
		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, pointClampSampler.put()));
		Util::SetResourceName(pointClampSampler.get(), "SSS::PointClampSampler");
	}

	{
		auto shadowMask = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kSHADOW_MASK];

		D3D11_TEXTURE2D_DESC texDesc{};
		shadowMask.texture->GetDesc(&texDesc);
		texDesc.MipLevels = 1;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texDesc.MiscFlags = 0;

		UINT baseWidth = texDesc.Width;
		UINT baseHeight = texDesc.Height;

		// Depth mip pyramid — R32G32_FLOAT (linearZ, linearZ²) for VSM queries.
		// Scratch siblings are allocated alongside for in-place blur ping-pong.
		{
			texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = texDesc.Format,
				.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 }
			};
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
				.Format = texDesc.Format,
				.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MipSlice = 0 }
			};
			for (int i = 0; i < 4; ++i) {
				texDesc.Width = std::max(1u, baseWidth >> i);
				texDesc.Height = std::max(1u, baseHeight >> i);
				char name[64];
				snprintf(name, sizeof(name), "SSS::DepthMipPrefiltered%d", i);
				texDepthMipPrefiltered[i] = eastl::make_unique<Texture2D>(texDesc, name);
				texDepthMipPrefiltered[i]->CreateSRV(srvDesc);
				texDepthMipPrefiltered[i]->CreateUAV(uavDesc);

				snprintf(name, sizeof(name), "SSS::DepthMip%d", i);
				texDepthMip[i] = eastl::make_unique<Texture2D>(texDesc, name);
				texDepthMip[i]->CreateSRV(srvDesc);
				texDepthMip[i]->CreateUAV(uavDesc);
			}
		}

		// Shadow textures — R8_UNORM.  ShadowsCS does its own moments+Chebyshev
		// resolution per ray, so downstream blur/upscale and the final shadow
		// mask all carry plain visibility values.
		{
			texDesc.Format = DXGI_FORMAT_R8_UNORM;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
				.Format = texDesc.Format,
				.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 }
			};
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
				.Format = texDesc.Format,
				.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MipSlice = 0 }
			};
			for (int i = 0; i < 4; ++i) {
				texDesc.Width = std::max(1u, baseWidth >> i);
				texDesc.Height = std::max(1u, baseHeight >> i);
				char name[64];
				snprintf(name, sizeof(name), "SSS::ShadowMip%d", i);
				texShadowMip[i] = eastl::make_unique<Texture2D>(texDesc, name);
				texShadowMip[i]->CreateSRV(srvDesc);
				texShadowMip[i]->CreateUAV(uavDesc);
			}

			// Working textures — same format, one per mip (0-3).
			for (int i = 0; i < 4; ++i) {
				texDesc.Width = std::max(1u, baseWidth >> i);
				texDesc.Height = std::max(1u, baseHeight >> i);
				char name[64];
				snprintf(name, sizeof(name), "SSS::ShadowWork%d", i);
				texShadowWork[i] = eastl::make_unique<Texture2D>(texDesc, name);
				texShadowWork[i]->CreateSRV(srvDesc);
				texShadowWork[i]->CreateUAV(uavDesc);
			}

			texDesc.Width = baseWidth;
			texDesc.Height = baseHeight;

			screenSpaceShadowsTexture = eastl::make_unique<Texture2D>(texDesc, "SSS::ShadowTexture");
			screenSpaceShadowsTexture->CreateSRV(srvDesc);
			screenSpaceShadowsTexture->CreateUAV(uavDesc);
		}
	}

	CompileComputeShaders();
}
#undef I18N_KEY_PREFIX

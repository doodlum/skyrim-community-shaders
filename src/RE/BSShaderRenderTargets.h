#pragma once

enum RenderTargets : uint32_t
{
	RENDER_TARGET_NONE = 0xFFFFFFFF,
	RENDER_TARGET_FRAMEBUFFER = 0,
	RENDER_TARGET_MAIN,
	RENDER_TARGET_MAIN_COPY,
	RENDER_TARGET_MAIN_ONLY_ALPHA,
	RENDER_TARGET_NORMAL_TAAMASK_SSRMASK,
	RENDER_TARGET_NORMAL_TAAMASK_SSRMASK_SWAP,
	RENDER_TARGET_NORMAL_TAAMASK_SSRMASK_DOWNSAMPLED,
	RENDER_TARGET_MOTION_VECTOR,
	RENDER_TARGET_WATER_DISPLACEMENT,
	RENDER_TARGET_WATER_DISPLACEMENT_SWAP,
	RENDER_TARGET_WATER_REFLECTIONS,
	RENDER_TARGET_WATER_FLOW,
	RENDER_TARGET_UNDERWATER_MASK,
	RENDER_TARGET_REFRACTION_NORMALS,
	RENDER_TARGET_MENUBG,
	RENDER_TARGET_PLAYER_FACEGEN_TINT,
	RENDER_TARGET_LOCAL_MAP,
	RENDER_TARGET_LOCAL_MAP_SWAP,
	RENDER_TARGET_SHADOW_MASK,
	RENDER_TARGET_GETHIT_BUFFER,
	RENDER_TARGET_GETHIT_BLURSWAP,
	RENDER_TARGET_BLURFULL_BUFFER,
	RENDER_TARGET_HDR_BLURSWAP,
	RENDER_TARGET_LDR_BLURSWAP,
	RENDER_TARGET_HDR_BLOOM,
	RENDER_TARGET_LDR_DOWNSAMPLE0,
	RENDER_TARGET_HDR_DOWNSAMPLE0,
	RENDER_TARGET_HDR_DOWNSAMPLE1,
	RENDER_TARGET_HDR_DOWNSAMPLE2,
	RENDER_TARGET_HDR_DOWNSAMPLE3,
	RENDER_TARGET_HDR_DOWNSAMPLE4,
	RENDER_TARGET_HDR_DOWNSAMPLE5,
	RENDER_TARGET_HDR_DOWNSAMPLE6,
	RENDER_TARGET_HDR_DOWNSAMPLE7,
	RENDER_TARGET_HDR_DOWNSAMPLE8,
	RENDER_TARGET_HDR_DOWNSAMPLE9,
	RENDER_TARGET_HDR_DOWNSAMPLE10,
	RENDER_TARGET_HDR_DOWNSAMPLE11,
	RENDER_TARGET_HDR_DOWNSAMPLE12,
	RENDER_TARGET_HDR_DOWNSAMPLE13,
	RENDER_TARGET_LENSFLAREVIS,
	RENDER_TARGET_IMAGESPACE_TEMP_COPY,
	RENDER_TARGET_IMAGESPACE_TEMP_COPY2,
	RENDER_TARGET_IMAGESPACE_VOLUMETRIC_LIGHTING,
	RENDER_TARGET_IMAGESPACE_VOLUMETRIC_LIGHTING_PREVIOUS,
	RENDER_TARGET_IMAGESPACE_VOLUMETRIC_LIGHTING_COPY,
	RENDER_TARGET_SAO,
	RENDER_TARGET_SAO_DOWNSCALED,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_0_ESRAM,
	RENDER_TARGET_SAO_CAMERAZ,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_0,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_1,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_2,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_3,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_4,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_5,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_6,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_7,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_8,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_9,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_10,
	RENDER_TARGET_SAO_CAMERAZ_MIP_LEVEL_11,
	RENDER_TARGET_SAO_RAWAO,
	RENDER_TARGET_SAO_RAWAO_DOWNSCALED,
	RENDER_TARGET_SAO_RAWAO_PREVIOUS,
	RENDER_TARGET_SAO_RAWAO_PREVIOUS_DOWNSCALED,
	RENDER_TARGET_SAO_TEMP_BLUR,
	RENDER_TARGET_SAO_TEMP_BLUR_DOWNSCALED,
	RENDER_TARGET_INDIRECT,
	RENDER_TARGET_INDIRECT_DOWNSCALED,
	RENDER_TARGET_RAWINDIRECT,
	RENDER_TARGET_RAWINDIRECT_DOWNSCALED,
	RENDER_TARGET_RAWINDIRECT_PREVIOUS,
	RENDER_TARGET_RAWINDIRECT_PREVIOUS_DOWNSCALED,
	RENDER_TARGET_RAWINDIRECT_SWAP,
	RENDER_TARGET_SAVE_GAME_SCREENSHOT,
	RENDER_TARGET_SCREENSHOT,
	RENDER_TARGET_VOLUMETRIC_LIGHTING_HALF_RES,
	RENDER_TARGET_VOLUMETRIC_LIGHTING_BLUR_HALF_RES,
	RENDER_TARGET_VOLUMETRIC_LIGHTING_QUARTER_RES,
	RENDER_TARGET_VOLUMETRIC_LIGHTING_BLUR_QUARTER_RES,
	RENDER_TARGET_TEMPORAL_AA_ACCUMULATION_1,
	RENDER_TARGET_TEMPORAL_AA_ACCUMULATION_2,
	RENDER_TARGET_TEMPORAL_AA_UI_ACCUMULATION_1,
	RENDER_TARGET_TEMPORAL_AA_UI_ACCUMULATION_2,
	RENDER_TARGET_TEMPORAL_AA_MASK,
	RENDER_TARGET_TEMPORAL_AA_WATER_1,
	RENDER_TARGET_TEMPORAL_AA_WATER_2,
	RENDER_TARGET_RAW_WATER,
	RENDER_TARGET_WATER_1,
	RENDER_TARGET_WATER_2,
	RENDER_TARGET_IBLENSFLARES_LIGHTS_FILTER,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_4X_4X_PING,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_4X_4X_PONG,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_4Y_PING,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_4Y_PONG,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_4Y_BLUR,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_4Y_BLUR_SWAP,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_32X_4Y_PING,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_32X_4Y_PONG,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_32X_4Y_BLUR,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_32X_4Y_BLUR_SWAP,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_16X_PING,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_16X_PONG,
	RENDER_TARGET_IBLENSFLARES_DOWNSAMPLE_16X_16X_SWAP,
	RENDER_TARGET_BOOK_TEXT_0,
	RENDER_TARGET_BOOK_TEXT_1,
	RENDER_TARGET_BOOK_TEXT_2,
	RENDER_TARGET_BOOK_TEXT_3,
	RENDER_TARGET_SSR,
	RENDER_TARGET_SSR_RAW,
	RENDER_TARGET_SSR_BLURRED0,
	RENDER_TARGET_SNOW_SPECALPHA,
	RENDER_TARGET_SNOW_SWAP,

	RENDER_TARGET_COUNT,
	RENDER_TARGET_FRAMEBUFFER_COUNT = 1,
};

enum RenderTargetsCubemaps : uint32_t
{
	RENDER_TARGET_CUBEMAP_NONE = 0xFFFFFFFF,
	RENDER_TARGET_CUBEMAP_REFLECTIONS = 0,

	RENDER_TARGET_CUBEMAP_COUNT,
};

enum RenderTargets3D : uint32_t
{
	TEXTURE3D_NONE = 0xFFFFFFFF,
	VOLUMETRIC_LIGHTING_ACCUMULATION = 0,
	VOLUMETRIC_LIGHTING_ACCUMULATION_COPY,
	VOLUMETRIC_LIGHTING_NOISE,

	TEXTURE3D_COUNT,
};

enum RenderTargetsDepthStencil : uint32_t
{
	DEPTH_STENCIL_TARGET_NONE = 0xFFFFFFFF,
	DEPTH_STENCIL_TARGET_MAIN = 0,
	DEPTH_STENCIL_TARGET_MAIN_COPY,
	DEPTH_STENCIL_TARGET_SHADOWMAPS_ESRAM,
	DEPTH_STENCIL_TARGET_VOLUMETRIC_LIGHTING_SHADOWMAPS_ESRAM,
	DEPTH_STENCIL_TARGET_SHADOWMAPS,
	DEPTH_STENCIL_DECAL_OCCLUSION,
	DEPTH_STENCIL_CUBEMAP_REFLECTIONS,
	DEPTH_STENCIL_POST_ZPREPASS_COPY,
	DEPTH_STENCIL_POST_WATER_COPY,
	DEPTH_STENCIL_BOOK_TEXT,
	DEPTH_STENCIL_TARGET_PRECIPITATION_OCCLUSION_MAP,
	DEPTH_STENCIL_TARGET_FOCUS_NEO,

	DEPTH_STENCIL_COUNT,
};

namespace BSShaderRenderTargets
{
	const char* GetRenderTargetName(uint32_t Index);
	const char* GetCubemapName(uint32_t Index);
	const char* GetTexture3DName(uint32_t Index);
	const char* GetDepthStencilName(uint32_t Index);
}

static const char* RTNames[] = {
	"FRAMEBUFFER",
	"MAIN",
	"MAIN_COPY",
	"MAIN_ONLY_ALPHA",
	"NORMAL_TAAMASK_SSRMASK",
	"NORMAL_TAAMASK_SSRMASK_SWAP",
	"NORMAL_TAAMASK_SSRMASK_DOWNSAMPLED",
	"MOTION_VECTOR",
	"WATER_DISPLACEMENT",
	"WATER_DISPLACEMENT_SWAP",
	"WATER_REFLECTIONS",
	"WATER_FLOW",
	"UNDERWATER_MASK",
	"REFRACTION_NORMALS",
	"MENUBG",
	"PLAYER_FACEGEN_TINT",
	"LOCAL_MAP",
	"LOCAL_MAP_SWAP",
	"SHADOW_MASK",
	"GETHIT_BUFFER",
	"GETHIT_BLURSWAP",
	"BLURFULL_BUFFER",
	"HDR_BLURSWAP",
	"LDR_BLURSWAP",
	"HDR_BLOOM",
	"LDR_DOWNSAMPLE0",
	"HDR_DOWNSAMPLE0",
	"HDR_DOWNSAMPLE1",
	"HDR_DOWNSAMPLE2",
	"HDR_DOWNSAMPLE3",
	"HDR_DOWNSAMPLE4",
	"HDR_DOWNSAMPLE5",
	"HDR_DOWNSAMPLE6",
	"HDR_DOWNSAMPLE7",
	"HDR_DOWNSAMPLE8",
	"HDR_DOWNSAMPLE9",
	"HDR_DOWNSAMPLE10",
	"HDR_DOWNSAMPLE11",
	"HDR_DOWNSAMPLE12",
	"HDR_DOWNSAMPLE13",
	"LENSFLAREVIS",
	"IMAGESPACE_TEMP_COPY",
	"IMAGESPACE_TEMP_COPY2",
	"IMAGESPACE_VOLUMETRIC_LIGHTING",
	"IMAGESPACE_VOLUMETRIC_LIGHTING_PREVIOUS",
	"IMAGESPACE_VOLUMETRIC_LIGHTING_COPY",
	"SAO",
	"SAO_DOWNSCALED",
	"SAO_CAMERAZ_MIP_LEVEL_0_ESRAM",
	"SAO_CAMERAZ",
	"SAO_CAMERAZ_MIP_LEVEL_0",
	"SAO_CAMERAZ_MIP_LEVEL_1",
	"SAO_CAMERAZ_MIP_LEVEL_2",
	"SAO_CAMERAZ_MIP_LEVEL_3",
	"SAO_CAMERAZ_MIP_LEVEL_4",
	"SAO_CAMERAZ_MIP_LEVEL_5",
	"SAO_CAMERAZ_MIP_LEVEL_6",
	"SAO_CAMERAZ_MIP_LEVEL_7",
	"SAO_CAMERAZ_MIP_LEVEL_8",
	"SAO_CAMERAZ_MIP_LEVEL_9",
	"SAO_CAMERAZ_MIP_LEVEL_10",
	"SAO_CAMERAZ_MIP_LEVEL_11",
	"SAO_RAWAO",
	"SAO_RAWAO_DOWNSCALED",
	"SAO_RAWAO_PREVIOUS",
	"SAO_RAWAO_PREVIOUS_DOWNSCALED",
	"SAO_TEMP_BLUR",
	"SAO_TEMP_BLUR_DOWNSCALED",
	"INDIRECT",
	"INDIRECT_DOWNSCALED",
	"RAWINDIRECT",
	"RAWINDIRECT_DOWNSCALED",
	"RAWINDIRECT_PREVIOUS",
	"RAWINDIRECT_PREVIOUS_DOWNSCALED",
	"RAWINDIRECT_SWAP",
	"SAVE_GAME_SCREENSHOT",
	"SCREENSHOT",
	"VOLUMETRIC_LIGHTING_HALF_RES",
	"VOLUMETRIC_LIGHTING_BLUR_HALF_RES",
	"VOLUMETRIC_LIGHTING_QUARTER_RES",
	"VOLUMETRIC_LIGHTING_BLUR_QUARTER_RES",
	"TEMPORAL_AA_ACCUMULATION_1",
	"TEMPORAL_AA_ACCUMULATION_2",
	"TEMPORAL_AA_UI_ACCUMULATION_1",
	"TEMPORAL_AA_UI_ACCUMULATION_2",
	"TEMPORAL_AA_MASK",
	"TEMPORAL_AA_WATER_1",
	"TEMPORAL_AA_WATER_2",
	"RAW_WATER",
	"WATER_1",
	"WATER_2",
	"IBLENSFLARES_LIGHTS_FILTER",
	"IBLENSFLARES_DOWNSAMPLE_4X_4X_PING",
	"IBLENSFLARES_DOWNSAMPLE_4X_4X_PONG",
	"IBLENSFLARES_DOWNSAMPLE_16X_4Y_PING",
	"IBLENSFLARES_DOWNSAMPLE_16X_4Y_PONG",
	"IBLENSFLARES_DOWNSAMPLE_16X_4Y_BLUR",
	"IBLENSFLARES_DOWNSAMPLE_16X_4Y_BLUR_SWAP",
	"IBLENSFLARES_DOWNSAMPLE_32X_4Y_PING",
	"IBLENSFLARES_DOWNSAMPLE_32X_4Y_PONG",
	"IBLENSFLARES_DOWNSAMPLE_32X_4Y_BLUR",
	"IBLENSFLARES_DOWNSAMPLE_32X_4Y_BLUR_SWAP",
	"IBLENSFLARES_DOWNSAMPLE_16X_16X_PING",
	"IBLENSFLARES_DOWNSAMPLE_16X_16X_PONG",
	"IBLENSFLARES_DOWNSAMPLE_16X_16X_SWAP",
	"BOOK_TEXT_0",
	"BOOK_TEXT_1",
	"BOOK_TEXT_2",
	"BOOK_TEXT_3",
	"SSR",
	"SSR_RAW",
	"SSR_BLURRED0",
	"SNOW_SPECALPHA",
	"SNOW_SWAP"
};

static const char* RTCubemapNames[] = {
	"REFLECTIONS"
};

static const char* RT3DNames[] = {
	"VOLUMETRIC_LIGHTING_ACCUMULATION",
	"VOLUMETRIC_LIGHTING_ACCUMULATION_COPY",
	"VOLUMETRIC_LIGHTING_NOISE"
};

static const char* DepthNames[] = {
	"TARGET_MAIN_DEPTH",
	"TARGET_MAIN_COPY_DEPTH",
	"TARGET_SHADOWMAPS_ESRAM_DEPTH",
	"TARGET_VOLUMETRIC_LIGHTING_SHADOWMAPS_ESRAM_DEPTH",
	"TARGET_SHADOWMAPS_DEPTH",
	"DECAL_OCCLUSION_DEPTH",
	"CUBEMAP_REFLECTIONS_DEPTH",
	"POST_ZPREPASS_COPY_DEPTH",
	"POST_WATER_COPY_DEPTH",
	"BOOK_TEXT_DEPTH",
	"TARGET_PRECIPITATION_OCCLUSION_MAP_DEPTH",
	"TARGET_FOCUS_NEO_DEPTH"
};

static const char* StencilNames[] = {
	"TARGET_MAIN_STENCIL",
	"TARGET_MAIN_COPY_STENCIL",
	"TARGET_SHADOWMAPS_ESRAM_STENCIL",
	"TARGET_VOLUMETRIC_LIGHTING_SHADOWMAPS_ESRAM_STENCIL",
	"TARGET_SHADOWMAPS_STENCIL",
	"DECAL_OCCLUSION_STENCIL",
	"CUBEMAP_REFLECTIONS_STENCIL",
	"POST_ZPREPASS_COPY_STENCIL",
	"POST_WATER_COPY_STENCIL",
	"BOOK_TEXT_STENCIL",
	"TARGET_PRECIPITATION_OCCLUSION_MAP_STENCIL",
	"TARGET_FOCUS_NEO_STENCIL"
};

#include "Common/Color.hlsli"
#include "Common/FrameBuffer.hlsli"
#include "Common/GBuffer.hlsli"
#include "Common/LodLandscape.hlsli"
#include "Common/Math.hlsli"
#include "Common/MotionBlur.hlsli"
#include "Common/Permutation.hlsli"
#include "Common/Random.hlsli"
#include "Common/SharedData.hlsli"
#include "Common/Skinned.hlsli"

#if defined(FACEGEN) || defined(FACEGEN_RGB_TINT)
#	define SKIN
#endif

#if defined(SKINNED) || defined(ENVMAP) || defined(EYE) || defined(MULTI_LAYER_PARALLAX)
#	define DRAW_IN_WORLDSPACE
#endif

#if (defined(TREE_ANIM) || defined(LANDSCAPE)) && !defined(VC)
#	define VC
#endif  // TREE_ANIM || LANDSCAPE || !VC

#if defined(LODOBJECTS) || defined(LODOBJECTSHD) || defined(LODLANDNOISE) || defined(WORLD_MAP)
#	define LOD
#endif

struct VS_INPUT
{
	float4 Position : POSITION0;
	float2 TexCoord0 : TEXCOORD0;
#if !defined(MODELSPACENORMALS)
	float4 Normal : NORMAL0;
	float4 Bitangent : BINORMAL0;
#endif  // !MODELSPACENORMALS

#if defined(VC)
	float4 Color : COLOR0;
#	if defined(LANDSCAPE)
	float4 LandBlendWeights1 : TEXCOORD2;
	float4 LandBlendWeights2 : TEXCOORD3;
#	endif  // LANDSCAPE
#endif      // VC
#if defined(SKINNED)
	float4 BoneWeights : BLENDWEIGHT0;
	float4 BoneIndices : BLENDINDICES0;
#endif  // SKINNED
#if defined(EYE)
	float EyeParameter : TEXCOORD2;
#endif  // EYE
#if defined(VR)
	uint InstanceID : SV_INSTANCEID;
#endif  // VR
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION0;
#if (defined(PROJECTED_UV) && !defined(SKINNED)) || defined(LANDSCAPE)
	float4
#else
	float2
#endif  // (defined (PROJECTED_UV) && !defined(SKINNED)) || defined(LANDSCAPE)
		TexCoord0 : TEXCOORD0;
#if defined(ENVMAP)
	precise
#endif  // ENVMAP
		float3 InputPosition : TEXCOORD4;
#if defined(SKINNED) || !defined(MODELSPACENORMALS)
	float3 TBN0 : TEXCOORD1;
	float3 TBN1 : TEXCOORD2;
	float3 TBN2 : TEXCOORD3;
#endif  // defined(SKINNED) || !defined(MODELSPACENORMALS)
	float3 ViewVector : TEXCOORD5;
#if defined(EYE)
	float3 EyeNormal : TEXCOORD6;
#elif defined(LANDSCAPE)
	float4 LandBlendWeights1 : TEXCOORD6;
	float4 LandBlendWeights2 : TEXCOORD7;
#elif defined(PROJECTED_UV) && !defined(SKINNED)
	float3 TexProj : TEXCOORD7;
#endif  // EYE
	float3 ScreenNormalTransform0 : TEXCOORD8;
	float3 ScreenNormalTransform1 : TEXCOORD9;
	float3 ScreenNormalTransform2 : TEXCOORD10;
	// #if !defined(VR)  // Position is normally not in VR, but perhaps we can use
	// it.
	float4 WorldPosition : POSITION1;
	float4 PreviousWorldPosition : POSITION2;
	// #endif // !VR
	float4 Color : COLOR0;
	float4 FogParam : COLOR1;
#if !defined(VR)
	row_major float3x4 World[1] : POSITION3;
#else
	row_major float3x4 World[2] : POSITION3;
#endif  // VR
	bool WorldSpace : TEXCOORD11;
#if defined(VR)
	float ClipDistance : SV_ClipDistance0;  // o11
	float CullDistance : SV_CullDistance0;  // p11
#endif

	float3 ModelPosition : TEXCOORD12;
};
#ifdef VSHADER

cbuffer PerTechnique : register(b0)
{
#	if !defined(VR)
	float4 HighDetailRange[1] : packoffset(c0);  // loaded cells center in xy, size in zw
	float4 FogParam : packoffset(c1);
	float4 FogNearColor : packoffset(c2);
	float4 FogFarColor : packoffset(c3);
#	else
	float4 HighDetailRange[2] : packoffset(c0);  // loaded cells center in xy, size in zw
	float4 FogParam : packoffset(c2);
	float4 FogNearColor : packoffset(c3);
	float4 FogFarColor : packoffset(c4);
#	endif  // VR
};

cbuffer PerMaterial : register(b1)
{
	float4 LeftEyeCenter : packoffset(c0);
	float4 RightEyeCenter : packoffset(c1);
	float4 TexcoordOffset : packoffset(c2);
};

cbuffer PerGeometry : register(b2)
{
#	if !defined(VR)
	row_major float3x4 World[1] : packoffset(c0);
	row_major float3x4 PreviousWorld[1] : packoffset(c3);
	float4 EyePosition[1] : packoffset(c6);
	float4 LandBlendParams : packoffset(c7);  // offset in xy, gridPosition in yw
	float4 TreeParams : packoffset(c8);       // wind magnitude in y, amplitude in z, leaf frequency in w
	float2 WindTimers : packoffset(c9);
	row_major float3x4 TextureProj[1] : packoffset(c10);
	float IndexScale : packoffset(c13);
	float4 WorldMapOverlayParameters : packoffset(c14);
#	else   // VR has 49 vs 30 entries
	row_major float3x4 World[2] : packoffset(c0);
	row_major float3x4 PreviousWorld[2] : packoffset(c6);
	float4 EyePosition[2] : packoffset(c12);
	float4 LandBlendParams : packoffset(c14);  // offset in xy, gridPosition in yw
	float4 TreeParams : packoffset(c15);       // wind magnitude in y, amplitude in z, leaf frequency in w
	float2 WindTimers : packoffset(c16);
	row_major float3x4 TextureProj[2] : packoffset(c17);
	float IndexScale : packoffset(c23);
	float4 WorldMapOverlayParameters : packoffset(c24);
#	endif  // VR
};

cbuffer VS_PerFrame : register(b12)
{
#	if !defined(VR)
	row_major float3x3 ScreenProj[1] : packoffset(c0);
	row_major float4x4 ViewProj[1] : packoffset(c8);
#		if defined(SKINNED)
	float3 BonesPivot[1] : packoffset(c40);
	float3 PreviousBonesPivot[1] : packoffset(c41);
#		endif  // SKINNED
#	else
	row_major float3x3 ScreenProj[2] : packoffset(c0);
	row_major float4x4 ViewProj[2] : packoffset(c16);
#		if defined(SKINNED)
	float3 BonesPivot[2] : packoffset(c80);
	float3 PreviousBonesPivot[2] : packoffset(c82);
#		endif  // SKINNED
#	endif      // VR
};

#	if defined(TREE_ANIM)
float2 GetTreeShiftVector(float4 position, float4 color)
{
	precise float4 tmp1 = (TreeParams.w * TreeParams.y).xxxx * WindTimers.xxyy;
	precise float4 tmp2 = float4(0.1, 0.25, 0.1, 0.25) * tmp1 + dot(position.xyz, 1.0.xxx).xxxx;
	precise float4 tmp3 = abs(-1.0.xxxx + 2.0.xxxx * frac(0.5.xxxx + tmp2.xyzw));
	precise float4 tmp4 = (tmp3 * tmp3) * (3.0.xxxx - 2.0.xxxx * tmp3);
	return (tmp4.xz + 0.1.xx * tmp4.yw) * (TreeParams.z * color.w).xx;
}
#	endif  // TREE_ANIM

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT vsout;

	precise float4 inputPosition = float4(input.Position.xyz, 1.0);

	uint eyeIndex = Stereo::GetEyeIndexVS(
#	if defined(VR)
		input.InstanceID
#	endif
	);
#	if defined(LODLANDNOISE) || defined(LODLANDSCAPE)
	inputPosition = AdjustLodLandscapeVertexPositionMS(inputPosition, float4x4(World[eyeIndex], float4(0, 0, 0, 1)), HighDetailRange[eyeIndex]);
#	endif  // defined(LODLANDNOISE) || defined(LODLANDSCAPE)                                                                   \

	precise float4 previousInputPosition = inputPosition;

#	if defined(TREE_ANIM)
	precise float2 treeShiftVector = GetTreeShiftVector(input.Position, input.Color);
	float3 normal = -1.0.xxx + 2.0.xxx * input.Normal.xyz;

	inputPosition.xyz += normal.xyz * treeShiftVector.x;
	previousInputPosition.xyz += normal.xyz * treeShiftVector.y;
#	endif

#	if defined(SKINNED)
	precise int4 actualIndices = 765.01.xxxx * input.BoneIndices.xyzw;

	float3x4 previousWorldMatrix =
		GetBoneTransformMatrix(PreviousBones, actualIndices, PreviousBonesPivot[eyeIndex], input.BoneWeights);
	precise float4 previousWorldPosition =
		float4(mul(inputPosition, transpose(previousWorldMatrix)), 1);

	float3x4 worldMatrix = GetBoneTransformMatrix(Bones, actualIndices, BonesPivot[eyeIndex], input.BoneWeights);
	precise float4 worldPosition = float4(mul(inputPosition, transpose(worldMatrix)), 1);

	float4 viewPos = mul(ViewProj[eyeIndex], worldPosition);
#	else   // !SKINNED
	precise float4 previousWorldPosition = float4(mul(PreviousWorld[eyeIndex], inputPosition), 1);
	precise float4 worldPosition = float4(mul(World[eyeIndex], inputPosition), 1);
	precise float4x4 world4x4 = float4x4(World[eyeIndex][0], World[eyeIndex][1], World[eyeIndex][2], float4(0, 0, 0, 1));
	precise float4x4 modelView = mul(ViewProj[eyeIndex], world4x4);
	float4 viewPos = mul(modelView, inputPosition);
#	endif  // SKINNED

	vsout.Position = viewPos;

#	if defined(LODLANDNOISE) || defined(LODLANDSCAPE)
	vsout.Position.z += min(1, 1e-4 * max(0, viewPos.z - 70000)) * 0.5;
#	endif

	float2 uv = input.TexCoord0.xy * TexcoordOffset.zw + TexcoordOffset.xy;
#	if defined(LANDSCAPE)
	vsout.TexCoord0.zw = (uv * 0.010416667.xx + LandBlendParams.xy) * float2(1, -1) + float2(0, 1);
#	elif defined(PROJECTED_UV) && !defined(SKINNED)
	vsout.TexCoord0.z = mul(TextureProj[eyeIndex][0], inputPosition);
	vsout.TexCoord0.w = mul(TextureProj[eyeIndex][1], inputPosition);
#	endif
	vsout.TexCoord0.xy = uv;

#	if defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(SKINNED)
	vsout.InputPosition.xyz = worldPosition.xyz;
#	elif defined(WORLD_MAP)
	vsout.InputPosition.xyz = WorldMapOverlayParameters.xyz + worldPosition.xyz;
#	else
	vsout.InputPosition.xyz = inputPosition.xyz;
#	endif

#	if defined(SKINNED)
	float3x3 boneRSMatrix = GetBoneRSMatrix(Bones, actualIndices, input.BoneWeights);
#	endif

#	if !defined(MODELSPACENORMALS)
	float3x3 tbn = float3x3(
		float3(input.Position.w, input.Normal.w * 2 - 1, input.Bitangent.w * 2 - 1),
		input.Bitangent.xyz * 2.0.xxx + -1.0.xxx,
		input.Normal.xyz * 2.0.xxx + -1.0.xxx);
	float3x3 tbnTr = transpose(tbn);

#		if defined(SKINNED)
	float3x3 worldTbnTr = transpose(mul(transpose(tbnTr), transpose(boneRSMatrix)));
	float3x3 worldTbnTrTr = transpose(worldTbnTr);
	worldTbnTrTr[0] = normalize(worldTbnTrTr[0]);
	worldTbnTrTr[1] = normalize(worldTbnTrTr[1]);
	worldTbnTrTr[2] = normalize(worldTbnTrTr[2]);
	worldTbnTr = transpose(worldTbnTrTr);
	vsout.TBN0.xyz = worldTbnTr[0];
	vsout.TBN1.xyz = worldTbnTr[1];
	vsout.TBN2.xyz = worldTbnTr[2];
#		elif defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX)
	vsout.TBN0.xyz = mul(tbn, World[eyeIndex][0].xyz);
	vsout.TBN1.xyz = mul(tbn, World[eyeIndex][1].xyz);
	vsout.TBN2.xyz = mul(tbn, World[eyeIndex][2].xyz);
	float3x3 tempTbnTr = transpose(float3x3(vsout.TBN0.xyz, vsout.TBN1.xyz, vsout.TBN2.xyz));
	tempTbnTr[0] = normalize(tempTbnTr[0]);
	tempTbnTr[1] = normalize(tempTbnTr[1]);
	tempTbnTr[2] = normalize(tempTbnTr[2]);
	tempTbnTr = transpose(tempTbnTr);
	vsout.TBN0.xyz = tempTbnTr[0];
	vsout.TBN1.xyz = tempTbnTr[1];
	vsout.TBN2.xyz = tempTbnTr[2];
#		else
	vsout.TBN0.xyz = tbnTr[0];
	vsout.TBN1.xyz = tbnTr[1];
	vsout.TBN2.xyz = tbnTr[2];
#		endif
#	elif defined(SKINNED)
	float3x3 boneRSMatrixTr = transpose(boneRSMatrix);
	float3x3 worldTbnTr = transpose(float3x3(normalize(boneRSMatrixTr[0]),
		normalize(boneRSMatrixTr[1]), normalize(boneRSMatrixTr[2])));

	vsout.TBN0.xyz = worldTbnTr[0];
	vsout.TBN1.xyz = worldTbnTr[1];
	vsout.TBN2.xyz = worldTbnTr[2];
#	endif

#	if defined(LANDSCAPE)
	vsout.LandBlendWeights1 = input.LandBlendWeights1;

	float2 gridOffset = LandBlendParams.zw - input.Position.xy;
	vsout.LandBlendWeights2.w = 1 - saturate(0.000375600968 * (9625.59961 - length(gridOffset)));
	vsout.LandBlendWeights2.xyz = input.LandBlendWeights2.xyz;
#	elif defined(PROJECTED_UV) && !defined(SKINNED)
	vsout.TexProj = TextureProj[eyeIndex][2].xyz;
#	endif

#	if defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(SKINNED)
	vsout.ViewVector = EyePosition[eyeIndex].xyz - worldPosition.xyz;
#	else
	vsout.ViewVector = EyePosition[eyeIndex].xyz - input.Position.xyz;
#	endif

#	if defined(EYE)
	precise float4 modelEyeCenter = float4(LeftEyeCenter.xyz + input.EyeParameter.xxx * (RightEyeCenter.xyz - LeftEyeCenter.xyz), 1);
	vsout.EyeNormal.xyz = normalize(worldPosition.xyz - mul(modelEyeCenter, transpose(worldMatrix)));
#	endif  // EYE

#	if defined(SKINNED)
	float3x3 ScreenNormalTransform = mul(ScreenProj[eyeIndex], worldTbnTr);

	vsout.ScreenNormalTransform0.xyz = ScreenNormalTransform[0];
	vsout.ScreenNormalTransform1.xyz = ScreenNormalTransform[1];
	vsout.ScreenNormalTransform2.xyz = ScreenNormalTransform[2];
#	else
	float3x4 transMat = mul(ScreenProj[eyeIndex], World[eyeIndex]);

#		if defined(MODELSPACENORMALS)
	vsout.ScreenNormalTransform0.xyz = transMat[0].xyz;
	vsout.ScreenNormalTransform1.xyz = transMat[1].xyz;
	vsout.ScreenNormalTransform2.xyz = transMat[2].xyz;
#		else
	vsout.ScreenNormalTransform0.xyz = mul(transMat[0].xyz, transpose(tbn));
	vsout.ScreenNormalTransform1.xyz = mul(transMat[1].xyz, transpose(tbn));
	vsout.ScreenNormalTransform2.xyz = mul(transMat[2].xyz, transpose(tbn));
#		endif  // MODELSPACENORMALS
#	endif      // SKINNED

	vsout.WorldPosition = worldPosition;
	vsout.PreviousWorldPosition = previousWorldPosition;

#	if defined(VC)
	vsout.Color = input.Color;
#	else
	vsout.Color = 1.0.xxxx;
#	endif  // VC

	float fogColorParam = min(FogParam.w,
		exp2(FogParam.z * log2(saturate(length(viewPos.xyz) * FogParam.y - FogParam.x))));

	vsout.FogParam.xyz = lerp(FogNearColor.xyz, FogFarColor.xyz, fogColorParam);
	vsout.FogParam.w = fogColorParam;

	vsout.World[0] = World[0];
#	ifdef VR
	vsout.World[1] = World[1];
#	endif  // VR
#	if defined(SKINNED) || defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(EYE)
	vsout.WorldSpace = true;
#	else
	vsout.WorldSpace = false;
#	endif

#	if defined(VR)
	Stereo::VR_OUTPUT VRout = Stereo::GetVRVSOutput(vsout.Position, eyeIndex);
	vsout.Position = VRout.VRPosition;
	vsout.ClipDistance.x = VRout.ClipDistance;
	vsout.CullDistance.x = VRout.CullDistance;
#	endif  // VR

	vsout.ModelPosition = input.Position.xyz;

	return vsout;
}
#endif  // VSHADER

typedef VS_OUTPUT PS_INPUT;

#if !defined(LANDSCAPE)
#	undef TERRAIN_BLENDING
#endif

#if defined(DEFERRED)
struct PS_OUTPUT
{
	float4 Diffuse : SV_Target0;
	float4 MotionVectors : SV_Target1;
	float4 NormalGlossiness : SV_Target2;
	float4 Albedo : SV_Target3;
	float4 Specular : SV_Target4;
	float4 Reflectance : SV_Target5;
	float4 Masks : SV_Target6;
	float4 Parameters : SV_Target7;
#	if defined(TERRAIN_BLENDING)
	float Depth : SV_Depth;
#	endif
};
#else
struct PS_OUTPUT
{
	float4 Diffuse : SV_Target0;
	float4 MotionVectors : SV_Target1;
	float4 ScreenSpaceNormals : SV_Target2;
#	if defined(SNOW)
	float4 Parameters : SV_Target3;
#	endif
};
#endif

#ifdef PSHADER

SamplerState SampTerrainParallaxSampler : register(s1);

#	if defined(LANDSCAPE)

SamplerState SampColorSampler : register(s0);

#		define SampLandColor2Sampler SampColorSampler
#		define SampLandColor3Sampler SampColorSampler
#		define SampLandColor4Sampler SampColorSampler
#		define SampLandColor5Sampler SampColorSampler
#		define SampLandColor6Sampler SampColorSampler
#		define SampNormalSampler SampColorSampler
#		define SampLandNormal2Sampler SampColorSampler
#		define SampLandNormal3Sampler SampColorSampler
#		define SampLandNormal4Sampler SampColorSampler
#		define SampLandNormal5Sampler SampColorSampler
#		define SampLandNormal6Sampler SampColorSampler
#		define SampRMAOSSampler SampColorSampler
#		define SampLandRMAOS2Sampler SampColorSampler
#		define SampLandRMAOS3Sampler SampColorSampler
#		define SampLandRMAOS4Sampler SampColorSampler
#		define SampLandRMAOS5Sampler SampColorSampler
#		define SampLandRMAOS6Sampler SampColorSampler

#	else

SamplerState SampColorSampler : register(s0);

#		define SampNormalSampler SampColorSampler

#		if defined(MODELSPACENORMALS) && !defined(LODLANDNOISE)
SamplerState SampSpecularSampler : register(s2);
#		endif
#		if defined(FACEGEN)
SamplerState SampTintSampler : register(s3);
SamplerState SampDetailSampler : register(s4);
#		elif defined(PARALLAX)
SamplerState SampParallaxSampler : register(s3);
#		elif defined(PROJECTED_UV) && !defined(SPARKLE)
SamplerState SampProjDiffuseSampler : register(s3);
#		endif

#		if (defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(SNOW_FLAG) || defined(EYE)) && !defined(FACEGEN)
SamplerState SampEnvSampler : register(s4);
SamplerState SampEnvMaskSampler : register(s5);
#		endif

#		if defined(TRUE_PBR)
SamplerState SampParallaxSampler : register(s4);
SamplerState SampRMAOSSampler : register(s5);
#		endif

SamplerState SampGlowSampler : register(s6);

#		if defined(MULTI_LAYER_PARALLAX)
SamplerState SampLayerSampler : register(s8);
#		elif defined(PROJECTED_UV) && !defined(SPARKLE)
SamplerState SampProjNormalSampler : register(s8);
#		endif

SamplerState SampBackLightSampler : register(s9);

#		if defined(PROJECTED_UV)
SamplerState SampProjDetailSampler : register(s10);
#		endif

SamplerState SampCharacterLightProjNoiseSampler : register(s11);
SamplerState SampRimSoftLightWorldMapOverlaySampler : register(s12);

#		if defined(WORLD_MAP) && (defined(LODLANDSCAPE) || defined(LODLANDNOISE))
SamplerState SampWorldMapOverlaySnowSampler : register(s13);
#		endif

#	endif

#	if defined(LOD_LAND_BLEND)
SamplerState SampLandLodBlend1Sampler : register(s13);
SamplerState SampLandLodBlend2Sampler : register(s15);
#	elif defined(LODLANDNOISE)
SamplerState SampLandLodNoiseSampler : register(s15);
#	endif

SamplerState SampShadowMaskSampler : register(s14);

#	if defined(LANDSCAPE)

Texture2D<float4> TexColorSampler : register(t0);
Texture2D<float4> TexLandColor2Sampler : register(t1);
Texture2D<float4> TexLandColor3Sampler : register(t2);
Texture2D<float4> TexLandColor4Sampler : register(t3);
Texture2D<float4> TexLandColor5Sampler : register(t4);
Texture2D<float4> TexLandColor6Sampler : register(t5);
Texture2D<float4> TexNormalSampler : register(t7);
Texture2D<float4> TexLandNormal2Sampler : register(t8);
Texture2D<float4> TexLandNormal3Sampler : register(t9);
Texture2D<float4> TexLandNormal4Sampler : register(t10);
Texture2D<float4> TexLandNormal5Sampler : register(t11);
Texture2D<float4> TexLandNormal6Sampler : register(t12);

#		if defined(TRUE_PBR)

Texture2D<float4> TexLandDisplacement0Sampler : register(t80);
Texture2D<float4> TexLandDisplacement1Sampler : register(t81);
Texture2D<float4> TexLandDisplacement2Sampler : register(t82);
Texture2D<float4> TexLandDisplacement3Sampler : register(t83);
Texture2D<float4> TexLandDisplacement4Sampler : register(t84);
Texture2D<float4> TexLandDisplacement5Sampler : register(t85);

Texture2D<float4> TexRMAOSSampler : register(t86);
Texture2D<float4> TexLandRMAOS2Sampler : register(t87);
Texture2D<float4> TexLandRMAOS3Sampler : register(t88);
Texture2D<float4> TexLandRMAOS4Sampler : register(t89);
Texture2D<float4> TexLandRMAOS5Sampler : register(t90);
Texture2D<float4> TexLandRMAOS6Sampler : register(t91);

#		endif

#	else

Texture2D<float4> TexColorSampler : register(t0);
Texture2D<float4> TexNormalSampler : register(t1);  // normal in xyz, glossiness in w if not modelspacenormal

#		if defined(MODELSPACENORMALS) && !defined(LODLANDNOISE)
Texture2D<float4> TexSpecularSampler : register(t2);
#		endif
#		if defined(FACEGEN)
Texture2D<float4> TexTintSampler : register(t3);
Texture2D<float4> TexDetailSampler : register(t4);
#		elif defined(PARALLAX)
Texture2D<float4> TexParallaxSampler : register(t3);
#		elif defined(PROJECTED_UV) && !defined(SPARKLE)
Texture2D<float4> TexProjDiffuseSampler : register(t3);
#		endif

#		if (defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(SNOW_FLAG) || defined(EYE)) && !defined(FACEGEN)
TextureCube<float4> TexEnvSampler : register(t4);
Texture2D<float4> TexEnvMaskSampler : register(t5);
#		endif

#		if defined(TRUE_PBR)
Texture2D<float4> TexParallaxSampler : register(t4);
Texture2D<float4> TexRMAOSSampler : register(t5);
#		endif

Texture2D<float4> TexGlowSampler : register(t6);

#		if defined(MULTI_LAYER_PARALLAX)
Texture2D<float4> TexLayerSampler : register(t8);
#		elif defined(PROJECTED_UV) && !defined(SPARKLE)
Texture2D<float4> TexProjNormalSampler : register(t8);
#		endif

Texture2D<float4> TexBackLightSampler : register(t9);

#		if defined(PROJECTED_UV)
Texture2D<float4> TexProjDetail : register(t10);
#		endif

Texture2D<float4> TexCharacterLightProjNoiseSampler : register(t11);
Texture2D<float4> TexRimSoftLightWorldMapOverlaySampler : register(t12);

#		if defined(WORLD_MAP) && (defined(LODLANDSCAPE) || defined(LODLANDNOISE))
Texture2D<float4> TexWorldMapOverlaySnowSampler : register(t13);
#		endif

#	endif

#	if defined(LOD_LAND_BLEND)
Texture2D<float4> TexLandLodBlend1Sampler : register(t13);
Texture2D<float4> TexLandLodBlend2Sampler : register(t15);
#	elif defined(LODLANDNOISE)
Texture2D<float4> TexLandLodNoiseSampler : register(t15);
#	endif

Texture2D<float4> TexShadowMaskSampler : register(t14);

cbuffer PerTechnique : register(b0)
{
	float4 FogColor : packoffset(c0);           // Color in xyz, invFrameBufferRange in w
	float4 ColourOutputClamp : packoffset(c1);  // fLightingOutputColourClampPostLit in x, fLightingOutputColourClampPostEnv in y, fLightingOutputColourClampPostSpec in z
	float4 VPOSOffset : packoffset(c2);         // ???
};

cbuffer PerMaterial : register(b1)
{
	float4 LODTexParams : packoffset(c0);  // TerrainTexOffset in xy, LodBlendingEnabled in z
#	if !(defined(LANDSCAPE) && defined(TRUE_PBR))
	float4 TintColor : packoffset(c1);
	float4 EnvmapData : packoffset(c2);  // fEnvmapScale in x, 1 or 0 in y depending of if has envmask
	float4 ParallaxOccData : packoffset(c3);
	float4 SpecularColor : packoffset(c4);  // Shininess in w, color in xyz
	float4 SparkleParams : packoffset(c5);
	float4 MultiLayerParallaxData : packoffset(c6);  // Layer thickness in x, refraction scale in y, uv scale in zw
#	else
	float4 LandscapeTexture1GlintParameters : packoffset(c1);
	float4 LandscapeTexture2GlintParameters : packoffset(c2);
	float4 LandscapeTexture3GlintParameters : packoffset(c3);
	float4 LandscapeTexture4GlintParameters : packoffset(c4);
	float4 LandscapeTexture5GlintParameters : packoffset(c5);
	float4 LandscapeTexture6GlintParameters : packoffset(c6);
#	endif
	float4 LightingEffectParams : packoffset(c7);  // fSubSurfaceLightRolloff in x, fRimLightPower in y
	float4 IBLParams : packoffset(c8);

#	if !defined(TRUE_PBR)
	float4 LandscapeTexture1to4IsSnow : packoffset(c9);
	float4 LandscapeTexture5to6IsSnow : packoffset(c10);  // bEnableSnowMask in z, inverse iLandscapeMultiNormalTilingFactor in w
	float4 LandscapeTexture1to4IsSpecPower : packoffset(c11);
	float4 LandscapeTexture5to6IsSpecPower : packoffset(c12);
	float4 SnowRimLightParameters : packoffset(c13);  // fSnowRimLightIntensity in x, fSnowGeometrySpecPower in y, fSnowNormalSpecPower in z, bEnableSnowRimLighting in w
#	endif

#	if defined(TRUE_PBR) && defined(LANDSCAPE)
	float3 LandscapeTexture2PBRParams : packoffset(c9);
	float3 LandscapeTexture3PBRParams : packoffset(c10);
	float3 LandscapeTexture4PBRParams : packoffset(c11);
	float3 LandscapeTexture5PBRParams : packoffset(c12);
	float3 LandscapeTexture6PBRParams : packoffset(c13);
#	endif

	float4 CharacterLightParams : packoffset(c14);
	// VR is [9] instead of [15]

	uint PBRFlags : packoffset(c15.x);
	float3 PBRParams1 : packoffset(c15.y);  // roughness scale, displacement scale, specular level
	float4 PBRParams2 : packoffset(c16);    // subsurface color, subsurface opacity
};

cbuffer PerGeometry : register(b2)
{
#	if !defined(VR)
	float3 DirLightDirection : packoffset(c0);
	float3 DirLightColor : packoffset(c1);
	float4 ShadowLightMaskSelect : packoffset(c2);
	float4 MaterialData : packoffset(c3);  // envmapLODFade in x, specularLODFade in y, alpha in z
	float AlphaTestRef : packoffset(c4);
	float3 EmitColor : packoffset(c4.y);
	float4 ProjectedUVParams : packoffset(c6);
	float4 SSRParams : packoffset(c7);
	float4 WorldMapOverlayParametersPS : packoffset(c8);
	float4 ProjectedUVParams2 : packoffset(c9);
	float4 ProjectedUVParams3 : packoffset(c10);  // fProjectedUVDiffuseNormalTilingScale in x, fProjectedUVNormalDetailTilingScale in y, EnableProjectedNormals in w
	row_major float3x4 DirectionalAmbient : packoffset(c11);
	float4 AmbientSpecularTintAndFresnelPower : packoffset(c14);  // Fresnel power in z, color in xyz
	float4 PointLightPosition[7] : packoffset(c15);               // point light radius in w
	float4 PointLightColor[7] : packoffset(c22);
	float2 NumLightNumShadowLight : packoffset(c29);
#	else
	// VR is [49] instead of [30]
	float3 DirLightDirection : packoffset(c0);
	float4 UnknownPerGeometry[12] : packoffset(c1);
	float3 DirLightColor : packoffset(c13);
	float4 ShadowLightMaskSelect : packoffset(c14);
	float4 MaterialData : packoffset(c15);  // envmapLODFade in x, specularLODFade in y, alpha in z
	float AlphaTestRef : packoffset(c16);
	float3 EmitColor : packoffset(c16.y);
	float4 ProjectedUVParams : packoffset(c18);
	float4 SSRParams : packoffset(c19);
	float4 WorldMapOverlayParametersPS : packoffset(c20);
	float4 ProjectedUVParams2 : packoffset(c21);
	float4 ProjectedUVParams3 : packoffset(c22);  // fProjectedUVDiffuseNormalTilingScale in x,	fProjectedUVNormalDetailTilingScale in y, EnableProjectedNormals in w
	row_major float3x4 DirectionalAmbient : packoffset(c23);
	float4 AmbientSpecularTintAndFresnelPower : packoffset(c26);  // Fresnel power in z, color in xyz
	float4 PointLightPosition[14] : packoffset(c27);              // point light radius in w
	float4 PointLightColor[7] : packoffset(c41);
	float2 NumLightNumShadowLight : packoffset(c48);
#	endif  // VR
};

#	if !defined(VR)
cbuffer AlphaTestRefBuffer : register(b11)
{
	float AlphaTestRefRS : packoffset(c0);
}
#	endif

float GetSoftLightMultiplier(float angle)
{
	float softLightParam = saturate((LightingEffectParams.x + angle) / (1 + LightingEffectParams.x));
	float arg1 = (softLightParam * softLightParam) * (3 - 2 * softLightParam);
	float clampedAngle = saturate(angle);
	float arg2 = (clampedAngle * clampedAngle) * (3 - 2 * clampedAngle);
	float softLigtMul = saturate(arg1 - arg2);
	return softLigtMul;
}

float GetRimLightMultiplier(float3 L, float3 V, float3 N)
{
	float NdotV = saturate(dot(N, V));
	return exp2(LightingEffectParams.y * log2(1 - NdotV)) * saturate(dot(V, -L));
}

#	if !defined(TRUE_PBR)
float ProcessSparkleColor(float color)
{
	return exp2(SparkleParams.y * log2(min(1, abs(color))));
}
#	endif

float3 GetLightSpecularInput(PS_INPUT input, float3 L, float3 V, float3 N, float3 lightColor, float shininess, float2 uv)
{
	float3 H = normalize(V + L);
	float HdotN = 1.0;
#	if defined(ANISO_LIGHTING)
	float3 AN = normalize(N * 0.5 + float3(input.TBN0.z, input.TBN1.z, input.TBN2.z));
	float LdotAN = dot(AN, L);
	float HdotAN = dot(AN, H);
	HdotN = 1 - min(1, abs(LdotAN - HdotAN));
#	else
	HdotN = saturate(dot(H, N));
#	endif

#	if defined(SPECULAR)
	float lightColorMultiplier = exp2(shininess * log2(HdotN));

#	elif defined(SPARKLE)
	float lightColorMultiplier = 0;
#	else
	float lightColorMultiplier = HdotN;
#	endif

#	if defined(ANISO_LIGHTING)
	lightColorMultiplier *= 0.7 * max(0, L.z);
#	endif

#	if defined(SPARKLE) && !defined(SNOW)
	float3 sparkleUvScale = exp2(float3(1.3, 1.6, 1.9) * log2(abs(SparkleParams.x)).xxx);

	float sparkleColor1 = TexProjDetail.Sample(SampProjDetailSampler, uv * sparkleUvScale.xx).z;
	float sparkleColor2 = TexProjDetail.Sample(SampProjDetailSampler, uv * sparkleUvScale.yy).z;
	float sparkleColor3 = TexProjDetail.Sample(SampProjDetailSampler, uv * sparkleUvScale.zz).z;
	float sparkleColor = ProcessSparkleColor(sparkleColor1) + ProcessSparkleColor(sparkleColor2) + ProcessSparkleColor(sparkleColor3);
	float VdotN = dot(V, N);
	V += N * -(2 * VdotN);
	float sparkleMultiplier = exp2(SparkleParams.w * log2(saturate(dot(V, -L)))) * (SparkleParams.z * sparkleColor);
	sparkleMultiplier = sparkleMultiplier >= 0.5 ? 1 : 0;
	lightColorMultiplier += sparkleMultiplier * HdotN;
#	endif
	return lightColor * lightColorMultiplier.xxx;
}

float3 TransformNormal(float3 normal)
{
	return normal * 2 + -1.0.xxx;
}

float GetLodLandBlendParameter(float3 color)
{
	float result = saturate(1.6666666 * (dot(color, 0.55.xxx) - 0.4));
	result = ((result * result) * (3 - result * 2));
#	if !defined(WORLD_MAP)
	result *= 0.8;
#	endif
	return result;
}

float GetLodLandBlendMultiplier(float parameter, float mask)
{
	return 0.8333333 * (parameter * (0.37 - mask) + mask) + 0.37;
}

float GetLandSnowMaskValue(float alpha)
{
#	if !defined(TRUE_PBR)
	return alpha * LandscapeTexture5to6IsSnow.z + (1 + -LandscapeTexture5to6IsSnow.z);
#	else
	return 0;
#	endif
}

float3 GetLandNormal(float landSnowMask, float3 normal, float2 uv, SamplerState sampNormal, Texture2D<float4> texNormal)
{
	float3 landNormal = TransformNormal(normal);
#	if defined(SNOW) && !defined(TRUE_PBR)
	if (landSnowMask > 1e-5 && LandscapeTexture5to6IsSnow.w != 1.0) {
		float3 snowNormal =
			float3(-1, -1, 1) *
			TransformNormal(texNormal.Sample(sampNormal, LandscapeTexture5to6IsSnow.ww * uv).xyz);
		landNormal.z += 1;
		float normalProjection = dot(landNormal, snowNormal);
		snowNormal = landNormal * normalProjection.xxx - snowNormal * landNormal.z;
		return normalize(snowNormal);
	} else {
		return landNormal;
	}
#	else
	return landNormal;
#	endif
}

#	if defined(SNOW) && !defined(TRUE_PBR)
float3 GetSnowSpecularColor(PS_INPUT input, float3 modelNormal, float3 viewDirection)
{
	if (SnowRimLightParameters.w > 1e-5) {
#		if defined(MODELSPACENORMALS) && !defined(SKINNED)
		float3 modelGeometryNormal = float3(0, 0, 1);
#		else
		float3 modelGeometryNormal = normalize(float3(input.TBN0.z, input.TBN1.z, input.TBN2.z));
#		endif
		float normalFactor = 1 - saturate(dot(modelNormal, viewDirection));
		float geometryNormalFactor = 1 - saturate(dot(modelGeometryNormal, viewDirection));
		return (SnowRimLightParameters.x * (exp2(SnowRimLightParameters.y * log2(geometryNormalFactor)) * exp2(SnowRimLightParameters.z * log2(normalFactor)))).xxx;
	} else {
		return 0.0.xxx;
	}
}
#	endif

#	if defined(FACEGEN)
float3 GetFacegenBaseColor(float3 rawBaseColor, float2 uv)
{
	float3 detailColor = TexDetailSampler.Sample(SampDetailSampler, uv).xyz;
	detailColor = float3(3.984375, 3.984375, 3.984375) * (float3(0.00392156886, 0, 0.00392156886) + detailColor);
	float3 tintColor = TexTintSampler.Sample(SampTintSampler, uv).xyz;
	tintColor = tintColor * rawBaseColor * 2.0.xxx;
	tintColor = tintColor - tintColor * rawBaseColor;
	return (rawBaseColor * rawBaseColor + tintColor) * detailColor;
}
#	endif

#	if defined(FACEGEN_RGB_TINT)
float3 GetFacegenRGBTintBaseColor(float3 rawBaseColor, float2 uv)
{
	float3 tintColor = TintColor.xyz * rawBaseColor * 2.0.xxx;
	tintColor = tintColor - tintColor * rawBaseColor;
	return float3(1.01171875, 0.99609375, 1.01171875) * (rawBaseColor * rawBaseColor + tintColor);
}
#	endif

#	if defined(WORLD_MAP)
float3 GetWorldMapNormal(PS_INPUT input, float3 rawNormal, float3 baseColor)
{
	float3 normal = normalize(rawNormal);
#		if defined(MODELSPACENORMALS)
	float3 worldMapNormalSrc = normal.xyz;
#		else
	float3 worldMapNormalSrc = float3(input.TBN0.z, input.TBN1.z, input.TBN2.z);
#		endif
	float3 worldMapNormal = 7.0.xxx * (-0.2.xxx + abs(normalize(worldMapNormalSrc)));
	worldMapNormal = max(0.01.xxx, worldMapNormal * worldMapNormal * worldMapNormal);
	worldMapNormal /= dot(worldMapNormal, 1.0.xxx);
	float3 worldMapColor1 = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, WorldMapOverlayParametersPS.xx * input.InputPosition.yz).xyz;
	float3 worldMapColor2 = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, WorldMapOverlayParametersPS.xx * input.InputPosition.xz).xyz;
	float3 worldMapColor3 = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, WorldMapOverlayParametersPS.xx * input.InputPosition.xy).xyz;
#		if defined(LODLANDNOISE) || defined(LODLANDSCAPE)
	float3 worldMapSnowColor1 = TexWorldMapOverlaySnowSampler.Sample(SampWorldMapOverlaySnowSampler, WorldMapOverlayParametersPS.ww * input.InputPosition.yz).xyz;
	float3 worldMapSnowColor2 = TexWorldMapOverlaySnowSampler.Sample(SampWorldMapOverlaySnowSampler, WorldMapOverlayParametersPS.ww * input.InputPosition.xz).xyz;
	float3 worldMapSnowColor3 = TexWorldMapOverlaySnowSampler.Sample(SampWorldMapOverlaySnowSampler, WorldMapOverlayParametersPS.ww * input.InputPosition.xy).xyz;
#		endif
	float3 worldMapColor = worldMapNormal.xxx * worldMapColor1 + worldMapNormal.yyy * worldMapColor2 + worldMapNormal.zzz * worldMapColor3;
#		if defined(LODLANDNOISE) || defined(LODLANDSCAPE)
	float3 worldMapSnowColor = worldMapSnowColor1 * worldMapNormal.xxx + worldMapSnowColor2 * worldMapNormal.yyy + worldMapSnowColor3 * worldMapNormal.zzz;
	float snowMultiplier = GetLodLandBlendParameter(baseColor);
	worldMapColor = snowMultiplier * (worldMapSnowColor - worldMapColor) + worldMapColor;
#		endif
	worldMapColor = normalize(2.0.xxx * (-0.5.xxx + (worldMapColor)));
#		if defined(LODLANDNOISE) || defined(LODLANDSCAPE)
	float worldMapLandTmp = saturate(19.9999962 * (rawNormal.z - 0.95));
	worldMapLandTmp = saturate(-(worldMapLandTmp * worldMapLandTmp) * (worldMapLandTmp * -2 + 3) + 1.5);
	float3 worldMapLandTmp1 = normalize(normal.zxy * float3(1, 0, 0) - normal.yzx * float3(0, 0, 1));
	float3 worldMapLandTmp2 = normalize(worldMapLandTmp1.yzx * normal.zxy - worldMapLandTmp1.zxy * normal.yzx);
	float3 worldMapLandTmp3 = normalize(worldMapColor.xxx * worldMapLandTmp1 + worldMapColor.yyy * worldMapLandTmp2 + worldMapColor.zzz * normal.xyz);
	float worldMapLandTmp4 = dot(worldMapLandTmp3, worldMapLandTmp3);
	if (worldMapLandTmp4 > 0.999 && worldMapLandTmp4 < 1.001) {
		normal.xyz = worldMapLandTmp * (worldMapLandTmp3 - normal.xyz) + normal.xyz;
	}
#		else
	normal.xyz = normalize(
		WorldMapOverlayParametersPS.zzz * (rawNormal.xyz - worldMapColor.xyz) + worldMapColor.xyz);
#		endif
	return normal;
}

float3 GetWorldMapBaseColor(float3 originalBaseColor, float3 rawBaseColor, float texProjTmp)
{
#		if defined(LODOBJECTS) && !defined(PROJECTED_UV)
	return rawBaseColor;
#		endif
#		if defined(LODLANDSCAPE) || defined(LODOBJECTSHD) || defined(LODLANDNOISE)
	float lodMultiplier = GetLodLandBlendParameter(originalBaseColor.xyz);
#		elif defined(LODOBJECTS) && defined(PROJECTED_UV)
	float lodMultiplier = saturate(10 * texProjTmp);
#		else
	float lodMultiplier = 1;
#		endif
#		if defined(LODOBJECTS)
	float4 lodColorMul = lodMultiplier.xxxx * float4(0.269999981, 0.281000018, 0.441000015, 0.441000015) + float4(0.0780000091, 0.09799999, -0.0349999964, 0.465000004);
	float4 lodColor = lodColorMul.xyzw * 2.0.xxxx;
	bool useLodColorZ = lodColorMul.w > 0.5;
	lodColor.xyz = max(lodColor.xyz, rawBaseColor.xyz);
	lodColor.w = useLodColorZ ? lodColor.z : min(lodColor.w, rawBaseColor.z);
	return (0.5 * lodMultiplier).xxx * (lodColor.xyw - rawBaseColor.xyz) + rawBaseColor;
#		else
	float4 lodColorMul = lodMultiplier.xxxx * float4(0.199999988, 0.441000015, 0.269999981, 0.281000018) + float4(0.300000012, 0.465000004, 0.0780000091, 0.09799999);
	float3 lodColor = lodColorMul.zwy * 2.0.xxx;
	lodColor.xy = max(lodColor.xy, rawBaseColor.xy);
	lodColor.z = lodColorMul.y > 0.5 ? max((lodMultiplier * 0.441 + -0.0349999964) * 2, rawBaseColor.z) : min(lodColor.z, rawBaseColor.z);
	return lodColorMul.xxx * (lodColor - rawBaseColor.xyz) + rawBaseColor;
#		endif
}
#	endif

float GetSnowParameterY(float texProjTmp, float alpha)
{
#	if defined(BASE_OBJECT_IS_SNOW)
	return min(1, texProjTmp + alpha);
#	else
	return texProjTmp;
#	endif
}

#	if defined(LOD)
#		undef EXTENDED_MATERIALS
#		undef WATER_BLENDING
#		undef LIGHT_LIMIT_FIX
#		undef WETNESS_EFFECTS
#		undef DYNAMIC_CUBEMAPS
#		undef WATER_LIGHTING
#	endif

#	if defined(WORLD_MAP)
#		undef CLOUD_SHADOWS
#		undef SKYLIGHTING
#	endif

#	if defined(WATER_LIGHTING)
#		include "WaterLighting/WaterCaustics.hlsli"
#	endif

#	if defined(EYE)
#		undef WETNESS_EFFECTS
#	endif

#	if defined(EXTENDED_MATERIALS) && !defined(LOD) && (defined(PARALLAX) || defined(LANDSCAPE) || defined(ENVMAP) || defined(TRUE_PBR))
#		define EMAT
#	endif

#	if defined(DYNAMIC_CUBEMAPS)
#		include "DynamicCubemaps/DynamicCubemaps.hlsli"
#	endif

#	if defined(TRUE_PBR)
#		include "Common/PBR.hlsli"
#	endif

#	if defined(EMAT)
#		include "ExtendedMaterials/ExtendedMaterials.hlsli"
#	endif

#	if defined(SCREEN_SPACE_SHADOWS)
#		include "ScreenSpaceShadows/ScreenSpaceShadows.hlsli"
#	endif

#	if defined(LIGHT_LIMIT_FIX)
#		include "LightLimitFix/LightLimitFix.hlsli"
#	endif

#	if defined(TREE_ANIM)
#		undef WETNESS_EFFECTS
#	endif

#	if defined(WETNESS_EFFECTS)
#		include "WetnessEffects/WetnessEffects.hlsli"
#	endif

#	if defined(TERRAIN_BLENDING)
#		include "TerrainBlending/TerrainBlending.hlsli"
#	endif

#	if defined(SSS)
#		if defined(SKIN)
#			undef SOFT_LIGHTING
#		endif
#	endif

#	if defined(TERRAIN_SHADOWS)
#		include "TerrainShadows/TerrainShadows.hlsli"
#	endif

#	if defined(CLOUD_SHADOWS)
#		include "CloudShadows/CloudShadows.hlsli"
#	endif

#	if defined(SKYLIGHTING)
#		include "Skylighting/Skylighting.hlsli"
#	endif

#	if defined(EXTENDED_TRANSCLUCENCY)
#		include "ExtendedTransclucency/ExtendedTransclucency.hlsli"
#	endif

#	define LinearSampler SampColorSampler

#	include "Common/ShadowSampling.hlsli"

PS_OUTPUT main(PS_INPUT input, bool frontFace
			   : SV_IsFrontFace)
{
	PS_OUTPUT psout;
	uint eyeIndex = Stereo::GetEyeIndexPS(input.Position, VPOSOffset);

	float3 viewPosition = mul(CameraView[eyeIndex], float4(input.WorldPosition.xyz, 1)).xyz;
	float2 screenUV = FrameBuffer::ViewToUV(viewPosition, true, eyeIndex);
	float screenNoise = Random::InterleavedGradientNoise(input.Position.xy, FrameCount);

	bool inWorld = ExtraShaderDescriptor & _InWorld;

	float nearFactor = smoothstep(4096.0 * 2.5, 0.0, viewPosition.z);

#	if defined(SKINNED) || !defined(MODELSPACENORMALS)
	float3x3 tbn = float3x3(input.TBN0.xyz, input.TBN1.xyz, input.TBN2.xyz);

#		if !defined(TREE_ANIM) && !defined(LOD)
	// Fix incorrect vertex normals on double-sided meshes
	if (!frontFace)
		tbn = lerp(tbn, -tbn, nearFactor);
#		endif

	float3x3 tbnTr = transpose(tbn);

#	endif  // defined (SKINNED) || !defined (MODELSPACENORMALS)

#	if !defined(TRUE_PBR)
#		if defined(LANDSCAPE)
	float shininess = dot(input.LandBlendWeights1, LandscapeTexture1to4IsSpecPower) + input.LandBlendWeights2.x * LandscapeTexture5to6IsSpecPower.x + input.LandBlendWeights2.y * LandscapeTexture5to6IsSpecPower.y;
#		else
	float shininess = SpecularColor.w;
#		endif  // defined (LANDSCAPE)
#	endif

#	if defined(TERRAIN_BLENDING)
	float depthSampled = TerrainBlending::GetTerrainOffsetDepth(screenUV, eyeIndex);
	float depthComp = input.Position.z - depthSampled;

	float depthSampledLinear = SharedData::GetScreenDepth(depthSampled);
	float depthPixelLinear = SharedData::GetScreenDepth(input.Position.z);

	float blendFactorTerrain = saturate((depthSampledLinear - depthPixelLinear) / 5.0);

	if (input.Position.z == depthSampled)
		blendFactorTerrain = 1;

	clip(blendFactorTerrain);
	blendFactorTerrain = saturate(blendFactorTerrain);
#	endif

	float3 viewDirection = normalize(input.ViewVector.xyz);
	float3 worldSpaceViewDirection = -normalize(input.WorldPosition.xyz);

	float2 uv = input.TexCoord0.xy;
	float2 uvOriginal = uv;

#	if defined(EMAT)
	float parallaxShadowQuality = sqrt(1.0 - saturate(viewPosition.z / 2048.0));
#	endif

#	if defined(LANDSCAPE)
	float mipLevels[6];
#	else
	float mipLevel = 0;
#	endif  // LANDSCAPE
	float sh0 = 0;
	float pixelOffset = 0;

#	if defined(EMAT)
#		if defined(LANDSCAPE)
	DisplacementParams displacementParams[6];
	displacementParams[0].DisplacementScale = 1.f;
	displacementParams[0].DisplacementOffset = 0.f;
	displacementParams[0].HeightScale = 1.f;
#		else
	DisplacementParams displacementParams;
	displacementParams.DisplacementScale = 1.f;
	displacementParams.DisplacementOffset = 0.f;
	displacementParams.HeightScale = 1.f;
#		endif

#	endif

	float3 entryNormal = 0;
	float3 entryNormalTS = 0;
	float eta = 1;
	float3 refractedViewDirection = viewDirection;
	float3 refractedViewDirectionWS = worldSpaceViewDirection;

#	if defined(EMAT)
#		if defined(PARALLAX)
	if (extendedMaterialSettings.EnableParallax) {
		mipLevel = ExtendedMaterials::GetMipLevel(uv, TexParallaxSampler);
		uv = ExtendedMaterials::GetParallaxCoords(viewPosition.z, uv, mipLevel, viewDirection, tbnTr, screenNoise, TexParallaxSampler, SampParallaxSampler, 0, displacementParams, pixelOffset);
		if (extendedMaterialSettings.EnableShadows && (parallaxShadowQuality > 0.0f || extendedMaterialSettings.ExtendShadows))
			sh0 = TexParallaxSampler.SampleLevel(SampParallaxSampler, uv, mipLevel).x;
	}
#		endif  // PARALLAX

#		if defined(ENVMAP)
	bool complexMaterial = false;
	bool complexMaterialParallax = false;
	float4 complexMaterialColor = 1.0;

	if (extendedMaterialSettings.EnableComplexMaterial) {
		float envMaskTest = TexEnvMaskSampler.SampleLevel(SampEnvMaskSampler, uv, 15).w;
		complexMaterial = envMaskTest < (1.0 - (4.0 / 255.0));

		if (complexMaterial) {
			if (envMaskTest > (4.0 / 255.0)) {
				complexMaterialParallax = true;
				mipLevel = ExtendedMaterials::GetMipLevel(uv, TexEnvMaskSampler);
				uv = ExtendedMaterials::GetParallaxCoords(viewPosition.z, uv, mipLevel, viewDirection, tbnTr, screenNoise, TexEnvMaskSampler, SampTerrainParallaxSampler, 3, displacementParams, pixelOffset);
				if (extendedMaterialSettings.EnableShadows && (parallaxShadowQuality > 0.0f || extendedMaterialSettings.ExtendShadows))
					sh0 = TexEnvMaskSampler.SampleLevel(SampEnvMaskSampler, uv, mipLevel).w;
			}

			complexMaterialColor = TexEnvMaskSampler.Sample(SampEnvMaskSampler, uv);
		}
	}

#		endif  // ENVMAP

#		if defined(TRUE_PBR) && !defined(LANDSCAPE) && !defined(LODLANDSCAPE)
	bool PBRParallax = false;
	[branch] if (extendedMaterialSettings.EnableParallax && (PBRFlags & TruePBR_HasDisplacement) != 0)
	{
		PBRParallax = true;
		displacementParams.HeightScale = PBRParams1.y;
		[branch] if ((PBRFlags & TruePBR_InterlayerParallax) != 0)
		{
			displacementParams.DisplacementScale = 0.5;
			displacementParams.DisplacementOffset = -0.25;
			eta = (1 - sqrt(MultiLayerParallaxData.y)) / (1 + sqrt(MultiLayerParallaxData.y));
			[branch] if ((PBRFlags & TruePBR_CoatNormal) != 0)
			{
				entryNormalTS = normalize(TransformNormal(TexBackLightSampler.Sample(SampBackLightSampler, uvOriginal).xyz));
			}
			else
			{
				entryNormalTS = normalize(TransformNormal(TexNormalSampler.Sample(SampNormalSampler, uvOriginal).xyz));
			}
			entryNormal = normalize(mul(tbn, entryNormalTS));
			refractedViewDirection = -refract(-viewDirection, entryNormal, eta);
			refractedViewDirectionWS = normalize(mul(input.World[eyeIndex], float4(refractedViewDirection, 0)));
		}
		mipLevel = ExtendedMaterials::GetMipLevel(uv, TexParallaxSampler);
		uv = ExtendedMaterials::GetParallaxCoords(viewPosition.z, uv, mipLevel, refractedViewDirection, tbnTr, screenNoise, TexParallaxSampler, SampParallaxSampler, 0, displacementParams, pixelOffset);
		if (extendedMaterialSettings.EnableShadows && (parallaxShadowQuality > 0.0f || extendedMaterialSettings.ExtendShadows))
			sh0 = TexParallaxSampler.SampleLevel(SampParallaxSampler, uv, mipLevel).x;
	}
#		endif  // TRUE_PBR

#	endif  // EMAT

#	if defined(SNOW)
	bool useSnowSpecular = true;
#	else
	bool useSnowSpecular = false;
#	endif  // SNOW

#	if defined(SPARKLE) || !defined(PROJECTED_UV)
	bool useSnowDecalSpecular = true;
#	else
	bool useSnowDecalSpecular = false;
#	endif  // defined(SPARKLE) || !defined(PROJECTED_UV)

	float2 diffuseUv = uv;

#	if defined(SPARKLE)
	diffuseUv = ProjectedUVParams2.yy * input.TexCoord0.zw;
#	endif  // SPARKLE

#	if defined(LANDSCAPE)
	// Normalize blend weights
	float landBlendWeights = 0.0;
	landBlendWeights += input.LandBlendWeights1.x;
	landBlendWeights += input.LandBlendWeights1.y;
	landBlendWeights += input.LandBlendWeights1.z;
	landBlendWeights += input.LandBlendWeights1.w;
	landBlendWeights += input.LandBlendWeights2.x;
	landBlendWeights += input.LandBlendWeights2.y;
	input.LandBlendWeights1.x /= landBlendWeights;
	input.LandBlendWeights1.y /= landBlendWeights;
	input.LandBlendWeights1.z /= landBlendWeights;
	input.LandBlendWeights1.w /= landBlendWeights;
	input.LandBlendWeights1.x /= landBlendWeights;
#		if defined(EMAT)
	if (extendedMaterialSettings.EnableTerrainParallax) {
		mipLevels[0] = ExtendedMaterials::GetMipLevel(uv, TexColorSampler);
		mipLevels[1] = ExtendedMaterials::GetMipLevel(uv, TexLandColor2Sampler);
		mipLevels[2] = ExtendedMaterials::GetMipLevel(uv, TexLandColor3Sampler);
		mipLevels[3] = ExtendedMaterials::GetMipLevel(uv, TexLandColor4Sampler);
		mipLevels[4] = ExtendedMaterials::GetMipLevel(uv, TexLandColor5Sampler);
		mipLevels[5] = ExtendedMaterials::GetMipLevel(uv, TexLandColor6Sampler);

		displacementParams[1] = displacementParams[0];
		displacementParams[2] = displacementParams[0];
		displacementParams[3] = displacementParams[0];
		displacementParams[4] = displacementParams[0];
		displacementParams[5] = displacementParams[0];
#			if defined(TRUE_PBR)
		displacementParams[0].HeightScale = PBRParams1.y;
		displacementParams[1].HeightScale = LandscapeTexture2PBRParams.y;
		displacementParams[2].HeightScale = LandscapeTexture3PBRParams.y;
		displacementParams[3].HeightScale = LandscapeTexture4PBRParams.y;
		displacementParams[4].HeightScale = LandscapeTexture5PBRParams.y;
		displacementParams[5].HeightScale = LandscapeTexture6PBRParams.y;
#			endif

		float weights[6];
		uv = ExtendedMaterials::GetParallaxCoords(input, viewPosition.z, uv, mipLevels, viewDirection, tbnTr, screenNoise, displacementParams, pixelOffset, weights);
		if (extendedMaterialSettings.EnableHeightBlending) {
			input.LandBlendWeights1.x = weights[0];
			input.LandBlendWeights1.y = weights[1];
			input.LandBlendWeights1.z = weights[2];
			input.LandBlendWeights1.w = weights[3];
			input.LandBlendWeights2.x = weights[4];
			input.LandBlendWeights2.y = weights[5];
		}
		if (extendedMaterialSettings.EnableShadows && (parallaxShadowQuality > 0.0f || extendedMaterialSettings.ExtendShadows)) {
#			if defined(TRUE_PBR)
			sh0 = ExtendedMaterials::GetTerrainHeight(input, uv, mipLevels, displacementParams, parallaxShadowQuality, input.LandBlendWeights1, input.LandBlendWeights2.xy, weights);
#			else
			sh0 = ExtendedMaterials::GetTerrainHeight(input, uv, mipLevels, displacementParams, parallaxShadowQuality, weights);
#			endif
		}
	}
#		endif  // EMAT
#	endif      // LANDSCAPE

#	if defined(SPARKLE)
	diffuseUv = ProjectedUVParams2.yy * (input.TexCoord0.zw + (uv - uvOriginal));
#	else
	diffuseUv = uv;
#	endif  // SPARKLE

	float4 baseColor = 0;
	float4 normal = 0;
	float glossiness = 0;

	float4 rawRMAOS = 0;

	float4 glintParameters = 0;

#	if defined(LANDSCAPE)
	if (input.LandBlendWeights1.x > 0.0) {
#	endif  // LANDSCAPE

		float4 rawBaseColor = TexColorSampler.Sample(SampColorSampler, diffuseUv);
#	if defined(TRUE_PBR) && defined(LANDSCAPE)
		[branch] if ((PBRFlags & TruePBR_LandTile0PBR) == 0)
		{
			rawBaseColor.rgb = Color::GammaToLinear(rawBaseColor.rgb) / Color::AlbedoPreMult;
		}
#	endif
		baseColor = rawBaseColor;

		float landSnowMask1 = GetLandSnowMaskValue(baseColor.w);
		float4 normalColor = TexNormalSampler.Sample(SampNormalSampler, uv);

		normal = normalColor;
#	if defined(MODELSPACENORMALS)
#		if defined(LODLANDNOISE)
		normal.xyz = normal.xzy - 0.5.xxx;
		float lodLandNoiseParameter = GetLodLandBlendParameter(baseColor.xyz);
		float noise = TexLandLodNoiseSampler.Sample(SampLandLodNoiseSampler, uv * 3.0.xx).x;
		float lodLandNoiseMultiplier = GetLodLandBlendMultiplier(lodLandNoiseParameter, noise);
		baseColor.xyz *= lodLandNoiseMultiplier;
		normal.xyz *= 2;
		normal.w = 1;
		glossiness = 0;
#		elif defined(LODLANDSCAPE)
		normal.xyz = 2.0.xxx * (-0.5.xxx + normal.xzy);
		normal.w = 1;
		glossiness = 0;
#		else
		normal.xyz = normal.xzy * 2.0.xxx + -1.0.xxx;
		normal.w = 1;
		glossiness = TexSpecularSampler.Sample(SampSpecularSampler, uv).x;
#		endif  // LODLANDNOISE
#	elif (defined(SNOW) && defined(LANDSCAPE))
	normal.xyz = GetLandNormal(landSnowMask1, normal.xyz, uv, SampNormalSampler, TexNormalSampler);
	glossiness = normal.w;
#	else
	normal.xyz = TransformNormal(normal.xyz);
	glossiness = normal.w;
#	endif  // MODELSPACENORMALS

#	if defined(WORLD_MAP)
		normal.xyz = GetWorldMapNormal(input, normal.xyz, rawBaseColor.xyz);
#	endif  // WORLD_MAP

#	if defined(TRUE_PBR)
#		if defined(LODLANDNOISE)
		rawRMAOS = float4(1, 0, 1, 0.04);
#		elif defined(LANDSCAPE)
		[branch] if ((PBRFlags & TruePBR_LandTile0PBR) != 0)
		{
			rawRMAOS = input.LandBlendWeights1.x * TexRMAOSSampler.Sample(SampRMAOSSampler, diffuseUv) * float4(PBRParams1.x, 1, 1, PBRParams1.z);
			if ((PBRFlags & TruePBR_LandTile0HasGlint) != 0) {
				glintParameters += input.LandBlendWeights1.x * LandscapeTexture1GlintParameters;
			}
		}
		else
		{
			rawRMAOS = input.LandBlendWeights1.x * float4(1 - glossiness.x, 0, 1, 0.04);
		}
#		else
		rawRMAOS = TexRMAOSSampler.Sample(SampRMAOSSampler, diffuseUv) * float4(PBRParams1.x, 1, 1, PBRParams1.z);
		if ((PBRFlags & TruePBR_Glint) != 0) {
			glintParameters = MultiLayerParallaxData;
		}
#		endif
#	endif

#	if defined(LANDSCAPE)
		baseColor *= input.LandBlendWeights1.x;
		normal *= input.LandBlendWeights1.x;
		glossiness *= input.LandBlendWeights1.x;
	}
#	endif  // LANDSCAPE

#	if defined(EMAT) && defined(ENVMAP)
	complexMaterial = complexMaterial && complexMaterialColor.y > (4.0 / 255.0) && (complexMaterialColor.y < (1.0 - (4.0 / 255.0)));
	shininess = lerp(shininess, shininess * complexMaterialColor.y, complexMaterial);
	float3 complexSpecular = lerp(1.0, lerp(1.0, baseColor.xyz, complexMaterialColor.z), complexMaterial);
	baseColor.xyz = lerp(baseColor.xyz, lerp(baseColor.xyz, 0.0, complexMaterialColor.z), complexMaterial);
#	endif  // defined (EMAT) && defined(ENVMAP)

#	if defined(FACEGEN)
	baseColor.xyz = GetFacegenBaseColor(baseColor.xyz, uv);
#	elif defined(FACEGEN_RGB_TINT)
	baseColor.xyz = GetFacegenRGBTintBaseColor(baseColor.xyz, uv);
#	endif  // FACEGEN

#	if defined(LANDSCAPE)

#		if defined(SNOW) && !defined(TRUE_PBR)
	float landSnowMask = LandscapeTexture1to4IsSnow.x * input.LandBlendWeights1.x;
#		endif  // SNOW

	if (input.LandBlendWeights1.y > 0.0) {
		float4 landColor2 = TexLandColor2Sampler.Sample(SampLandColor2Sampler, uv);
		float landSnowMask2 = GetLandSnowMaskValue(landColor2.w);
		float4 landNormal2 = TexLandNormal2Sampler.Sample(SampLandNormal2Sampler, uv);
		landNormal2.xyz = GetLandNormal(landSnowMask2, landNormal2.xyz, uv, SampLandNormal2Sampler, TexLandNormal2Sampler);
		normal.xyz += input.LandBlendWeights1.yyy * landNormal2.xyz;
		glossiness += input.LandBlendWeights1.y * landNormal2.w;
#		if defined(SNOW) && !defined(TRUE_PBR)
		landSnowMask += LandscapeTexture1to4IsSnow.y * input.LandBlendWeights1.y * landSnowMask2;
#		endif  // SNOW

#		if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_LandTile1PBR) != 0)
		{
			rawRMAOS += input.LandBlendWeights1.y * TexLandRMAOS2Sampler.Sample(SampLandRMAOS2Sampler, uv) * float4(LandscapeTexture2PBRParams.x, 1, 1, LandscapeTexture2PBRParams.z);
			if ((PBRFlags & TruePBR_LandTile1HasGlint) != 0) {
				glintParameters += input.LandBlendWeights1.y * LandscapeTexture2GlintParameters;
			}
		}
		else
		{
			landColor2.rgb = Color::GammaToLinear(landColor2.rgb) / Color::AlbedoPreMult;
			rawRMAOS += input.LandBlendWeights1.y * float4(1 - landNormal2.w, 0, 1, 0.04);
		}
#		endif
		baseColor += input.LandBlendWeights1.yyyy * landColor2;
	}

	if (input.LandBlendWeights1.z > 0.0) {
		float4 landColor3 = TexLandColor3Sampler.Sample(SampLandColor3Sampler, uv);
		float landSnowMask3 = GetLandSnowMaskValue(landColor3.w);
		float4 landNormal3 = TexLandNormal3Sampler.Sample(SampLandNormal3Sampler, uv);
		landNormal3.xyz = GetLandNormal(landSnowMask3, landNormal3.xyz, uv, SampLandNormal3Sampler, TexLandNormal3Sampler);
		normal.xyz += input.LandBlendWeights1.zzz * landNormal3.xyz;
		glossiness += input.LandBlendWeights1.z * landNormal3.w;
#		if defined(SNOW) && !defined(TRUE_PBR)
		landSnowMask += LandscapeTexture1to4IsSnow.z * input.LandBlendWeights1.z * landSnowMask3;
#		endif  // SNOW

#		if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_LandTile2PBR) != 0)
		{
			rawRMAOS += input.LandBlendWeights1.z * TexLandRMAOS3Sampler.Sample(SampLandRMAOS3Sampler, uv) * float4(LandscapeTexture3PBRParams.x, 1, 1, LandscapeTexture3PBRParams.z);
			if ((PBRFlags & TruePBR_LandTile2HasGlint) != 0) {
				glintParameters += input.LandBlendWeights1.z * LandscapeTexture3GlintParameters;
			}
		}
		else
		{
			landColor3.rgb = Color::GammaToLinear(landColor3.rgb) / Color::AlbedoPreMult;
			rawRMAOS += input.LandBlendWeights1.z * float4(1 - landNormal3.w, 0, 1, 0.04);
		}
#		endif
		baseColor += input.LandBlendWeights1.zzzz * landColor3;
	}

	if (input.LandBlendWeights1.w > 0.0) {
		float4 landColor4 = TexLandColor4Sampler.Sample(SampLandColor4Sampler, uv);
		float landSnowMask4 = GetLandSnowMaskValue(landColor4.w);
		float4 landNormal4 = TexLandNormal4Sampler.Sample(SampLandNormal4Sampler, uv);
		landNormal4.xyz = GetLandNormal(landSnowMask4, landNormal4.xyz, uv, SampLandNormal4Sampler, TexLandNormal4Sampler);
		normal.xyz += input.LandBlendWeights1.www * landNormal4.xyz;
		glossiness += input.LandBlendWeights1.w * landNormal4.w;
#		if defined(SNOW) && !defined(TRUE_PBR)
		landSnowMask += LandscapeTexture1to4IsSnow.w * input.LandBlendWeights1.w * landSnowMask4;
#		endif  // SNOW

#		if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_LandTile3PBR) != 0)
		{
			rawRMAOS += input.LandBlendWeights1.w * TexLandRMAOS4Sampler.Sample(SampLandRMAOS4Sampler, uv) * float4(LandscapeTexture4PBRParams.x, 1, 1, LandscapeTexture4PBRParams.z);
			if ((PBRFlags & TruePBR_LandTile3HasGlint) != 0) {
				glintParameters += input.LandBlendWeights1.w * LandscapeTexture4GlintParameters;
			}
		}
		else
		{
			landColor4.rgb = Color::GammaToLinear(landColor4.rgb) / Color::AlbedoPreMult;
			rawRMAOS += input.LandBlendWeights1.w * float4(1 - landNormal4.w, 0, 1, 0.04);
		}
#		endif
		baseColor += input.LandBlendWeights1.wwww * landColor4;
	}

	if (input.LandBlendWeights2.x > 0.0) {
		float4 landColor5 = TexLandColor5Sampler.Sample(SampLandColor5Sampler, uv);
		float landSnowMask5 = GetLandSnowMaskValue(landColor5.w);
		float4 landNormal5 = TexLandNormal5Sampler.Sample(SampLandNormal5Sampler, uv);
		landNormal5.xyz = GetLandNormal(landSnowMask5, landNormal5.xyz, uv, SampLandNormal5Sampler, TexLandNormal5Sampler);
		normal.xyz += input.LandBlendWeights2.xxx * landNormal5.xyz;
		glossiness += input.LandBlendWeights2.x * landNormal5.w;
#		if defined(SNOW) && !defined(TRUE_PBR)
		landSnowMask += LandscapeTexture5to6IsSnow.x * input.LandBlendWeights2.x * landSnowMask5;
#		endif  // SNOW

#		if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_LandTile4PBR) != 0)
		{
			rawRMAOS += input.LandBlendWeights2.x * TexLandRMAOS5Sampler.Sample(SampLandRMAOS5Sampler, uv) * float4(LandscapeTexture5PBRParams.x, 1, 1, LandscapeTexture5PBRParams.z);
			if ((PBRFlags & TruePBR_LandTile4HasGlint) != 0) {
				glintParameters += input.LandBlendWeights2.x * LandscapeTexture5GlintParameters;
			}
		}
		else
		{
			landColor5.rgb = Color::GammaToLinear(landColor5.rgb) / Color::AlbedoPreMult;
			rawRMAOS += input.LandBlendWeights2.x * float4(1 - landNormal5.w, 0, 1, 0.04);
		}
#		endif
		baseColor += input.LandBlendWeights2.xxxx * landColor5;
	}

	if (input.LandBlendWeights2.y > 0.0) {
		float4 landColor6 = TexLandColor6Sampler.Sample(SampLandColor6Sampler, uv);
		float landSnowMask6 = GetLandSnowMaskValue(landColor6.w);
		float4 landNormal6 = TexLandNormal6Sampler.Sample(SampLandNormal6Sampler, uv);
		landNormal6.xyz = GetLandNormal(landSnowMask6, landNormal6.xyz, uv, SampLandNormal6Sampler, TexLandNormal6Sampler);
		normal.xyz += input.LandBlendWeights2.yyy * landNormal6.xyz;
		glossiness += input.LandBlendWeights2.y * landNormal6.w;
#		if defined(SNOW) && !defined(TRUE_PBR)
		landSnowMask += LandscapeTexture5to6IsSnow.y * input.LandBlendWeights2.y * landSnowMask6;
#		endif  // SNOW

#		if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_LandTile5PBR) != 0)
		{
			rawRMAOS += input.LandBlendWeights2.y * TexLandRMAOS6Sampler.Sample(SampLandRMAOS6Sampler, uv) * float4(LandscapeTexture6PBRParams.x, 1, 1, LandscapeTexture6PBRParams.z);
			if ((PBRFlags & TruePBR_LandTile5HasGlint) != 0) {
				glintParameters += input.LandBlendWeights2.y * LandscapeTexture6GlintParameters;
			}
		}
		else
		{
			landColor6.rgb = Color::GammaToLinear(landColor6.rgb) / Color::AlbedoPreMult;
			rawRMAOS += input.LandBlendWeights2.y * float4(1 - landNormal6.w, 0, 1, 0.04);
		}
#		endif
		baseColor += input.LandBlendWeights2.yyyy * landColor6;
	}

#		if defined(LOD_LAND_BLEND)
	float4 lodLandColor = TexLandLodBlend1Sampler.Sample(SampLandLodBlend1Sampler, input.TexCoord0.zw);
	float lodBlendParameter = GetLodLandBlendParameter(lodLandColor.xyz);
	float lodBlendMask = TexLandLodBlend2Sampler.Sample(SampLandLodBlend2Sampler, 3.0.xx * input.TexCoord0.zw).x;
	float lodLandFadeFactor = GetLodLandBlendMultiplier(lodBlendParameter, lodBlendMask);
	float lodLandBlendFactor = LODTexParams.z * input.LandBlendWeights2.w;

	normal.xyz = lerp(normal.xyz, float3(0, 0, 1), lodLandBlendFactor);

#			if !defined(TRUE_PBR)
	baseColor.w = 0;
	baseColor = lerp(baseColor, lodLandColor * lodLandFadeFactor, lodLandBlendFactor);
	glossiness = lerp(glossiness, 0, lodLandBlendFactor);
#			endif
#		endif  // LOD_LAND_BLEND

#		if defined(SNOW) && !defined(TRUE_PBR)
	useSnowSpecular = landSnowMask != 0.0;
#		endif  // SNOW
#	endif      // LANDSCAPE

#	if defined(BACK_LIGHTING)
	float4 backLightColor = TexBackLightSampler.Sample(SampBackLightSampler, uv);
#	endif  // BACK_LIGHTING

#	if defined(RIM_LIGHTING) || defined(SOFT_LIGHTING) || defined(LOAD_SOFT_LIGHTING)
	float4 rimSoftLightColor = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, uv);
#	endif  // RIM_LIGHTING || SOFT_LIGHTING

	float numLights = min(7, NumLightNumShadowLight.x);
	float numShadowLights = min(4, NumLightNumShadowLight.y);

#	if defined(TRUE_PBR) && !defined(MODELSPACENORMALS)
	if (!frontFace) {
		normal.xyz *= -1;
	}
#	endif

#	if defined(MODELSPACENORMALS) && !defined(SKINNED)
	float4 modelNormal = normal;
#	else
	float4 modelNormal = float4(normalize(mul(tbn, normal.xyz)), 1);

#		if defined(SPARKLE)
	float3 projectedNormal = normalize(mul(tbn, float3(ProjectedUVParams2.xx * normal.xy, normal.z)));
#		endif  // SPARKLE
#	endif      // defined (MODELSPACENORMALS) && !defined (SKINNED)

	float2 baseShadowUV = 1.0.xx;
	float4 shadowColor = 1.0;
	if ((PixelShaderDescriptor & _DefShadow) && ((PixelShaderDescriptor & _ShadowDir) || inWorld) || numShadowLights > 0) {
		baseShadowUV = input.Position.xy * DynamicResolutionParams2.xy;
		float2 adjustedShadowUV = baseShadowUV * VPOSOffset.xy + VPOSOffset.zw;
		float2 shadowUV = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(adjustedShadowUV);
		shadowColor = TexShadowMaskSampler.Sample(SampShadowMaskSampler, shadowUV);
	}

	float projectedMaterialWeight = 0;

	float projWeight = 0;

#	if defined(PROJECTED_UV)
	float2 projNoiseUv = ProjectedUVParams.zz * input.TexCoord0.zw;
	float projNoise = TexCharacterLightProjNoiseSampler.Sample(SampCharacterLightProjNoiseSampler, projNoiseUv).x;
	float3 texProj = normalize(input.TexProj);
#		if defined(TREE_ANIM) || defined(LODOBJECTSHD)
	float vertexAlpha = 1;
#		else
	float vertexAlpha = input.Color.w;
#		endif  // defined (TREE_ANIM) || defined (LODOBJECTSHD)
	projWeight = -ProjectedUVParams.x * projNoise + (dot(modelNormal.xyz, texProj) * vertexAlpha - ProjectedUVParams.w);
#		if defined(LODOBJECTSHD)
	projWeight += (-0.5 + input.Color.w) * 2.5;
#		endif  // LODOBJECTSHD
#		if defined(SPARKLE)
	if (projWeight < 0)
		discard;

	modelNormal.xyz = projectedNormal;
#			if defined(SNOW)
	psout.Parameters.y = 1;
#			endif  // SNOW
#		elif !defined(FACEGEN) && !defined(MULTI_LAYER_PARALLAX) && !defined(PARALLAX) && !defined(SPARKLE)
	if (ProjectedUVParams3.w > 0.5) {
		float2 projNormalDiffuseUv = ProjectedUVParams3.x * projNoiseUv;
		float3 projNormal = TransformNormal(TexProjNormalSampler.Sample(SampProjNormalSampler, projNormalDiffuseUv).xyz);
		float2 projDetailNormalUv = ProjectedUVParams3.y * projNoiseUv;
		float3 projDetailNormal = TexProjDetail.Sample(SampProjDetailSampler, projDetailNormalUv).xyz;
		float3 finalProjNormal = normalize(TransformNormal(projDetailNormal) * float3(1, 1, projNormal.z) + float3(projNormal.xy, 0));
		float3 projBaseColor = TexProjDiffuseSampler.Sample(SampProjDiffuseSampler, projNormalDiffuseUv).xyz * ProjectedUVParams2.xyz;
		projectedMaterialWeight = smoothstep(0, 1, 5 * (0.1 + projWeight));
#			if defined(TRUE_PBR)
		projBaseColor = saturate(EnvmapData.xyz * projBaseColor);
		rawRMAOS.xyw = lerp(rawRMAOS.xyw, float3(ParallaxOccData.x, 0, ParallaxOccData.y), projectedMaterialWeight);
		float4 projectedGlintParameters = 0;
		if ((PBRFlags & TruePBR_ProjectedGlint) != 0) {
			projectedGlintParameters = SparkleParams;
		}
		glintParameters = lerp(glintParameters, projectedGlintParameters, projectedMaterialWeight);
#			endif  // TRUE_PBR
		normal.xyz = lerp(normal.xyz, finalProjNormal, projectedMaterialWeight);
		baseColor.xyz = lerp(baseColor.xyz, projBaseColor, projectedMaterialWeight);

#			if defined(SNOW)
		useSnowDecalSpecular = true;
		psout.Parameters.y = GetSnowParameterY(projectedMaterialWeight, baseColor.w);
#			endif  // SNOW
	} else {
		if (projWeight > 0) {
			baseColor.xyz = ProjectedUVParams2.xyz;
#			if defined(SNOW)
			useSnowDecalSpecular = true;
			psout.Parameters.y = GetSnowParameterY(projWeight, baseColor.w);
#			endif  // SNOW
		} else {
#			if defined(SNOW)
			psout.Parameters.y = 0;
#			endif  // SNOW
		}
	}

#			if defined(SPECULAR)
	useSnowSpecular = useSnowDecalSpecular;
#			endif  // SPECULAR
#		endif      // SPARKLE

#	elif defined(SNOW)
#		if defined(LANDSCAPE)
	psout.Parameters.y = landSnowMask;
#		else
	psout.Parameters.y = baseColor.w;
#		endif  // LANDSCAPE
#	endif      // SNOW

#	if defined(WORLD_MAP)
	baseColor.xyz = GetWorldMapBaseColor(rawBaseColor.xyz, baseColor.xyz, projWeight);
#	endif  // WORLD_MAP

	float3 worldSpaceNormal = modelNormal.xyz;

#	if !defined(DRAW_IN_WORLDSPACE)
	[flatten] if (!input.WorldSpace)
		worldSpaceNormal = normalize(mul(input.World[eyeIndex], float4(worldSpaceNormal, 0)));
#	endif

	float3 screenSpaceNormal = normalize(FrameBuffer::WorldToView(worldSpaceNormal, false, eyeIndex));

#	if defined(TRUE_PBR)
	PBR::SurfaceProperties pbrSurfaceProperties = PBR::InitSurfaceProperties();

	pbrSurfaceProperties.Roughness = saturate(rawRMAOS.x);
	pbrSurfaceProperties.Metallic = saturate(rawRMAOS.y);
	pbrSurfaceProperties.AO = rawRMAOS.z;
	pbrSurfaceProperties.F0 = lerp(saturate(rawRMAOS.w), baseColor.xyz, pbrSurfaceProperties.Metallic);

	pbrSurfaceProperties.GlintScreenSpaceScale = max(1, glintParameters.x);
	pbrSurfaceProperties.GlintLogMicrofacetDensity = clamp(40.f - glintParameters.y, 1, 40);
	pbrSurfaceProperties.GlintMicrofacetRoughness = clamp(glintParameters.z, 0.005, 0.3);
	pbrSurfaceProperties.GlintDensityRandomization = clamp(glintParameters.w, 0, 5);

#		if defined(GLINT)
	PBR::PrecomputeGlints(uvOriginal, ddx(uvOriginal), ddy(uvOriginal), pbrSurfaceProperties.GlintScreenSpaceScale, pbrSurfaceProperties.GlintCache);
#		endif

	baseColor.xyz *= 1 - pbrSurfaceProperties.Metallic;

	pbrSurfaceProperties.BaseColor = baseColor.xyz;

	float3 coatModelNormal = modelNormal.xyz;
	float3 coatWorldNormal = worldSpaceNormal;

#		if !defined(LANDSCAPE) && !defined(LODLANDSCAPE)
	[branch] if ((PBRFlags & TruePBR_Subsurface) != 0)
	{
		pbrSurfaceProperties.SubsurfaceColor = PBRParams2.xyz;
		pbrSurfaceProperties.Thickness = PBRParams2.w;
		[branch] if ((PBRFlags & TruePBR_HasFeatureTexture0) != 0)
		{
			float4 sampledSubsurfaceProperties = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, uv);
			pbrSurfaceProperties.SubsurfaceColor *= sampledSubsurfaceProperties.xyz;
			pbrSurfaceProperties.Thickness *= sampledSubsurfaceProperties.w;
		}
		pbrSurfaceProperties.Thickness = lerp(pbrSurfaceProperties.Thickness, 1, projectedMaterialWeight);
	}
	else if ((PBRFlags & TruePBR_TwoLayer) != 0)
	{
		pbrSurfaceProperties.CoatColor = PBRParams2.xyz;
		pbrSurfaceProperties.CoatStrength = PBRParams2.w;
		pbrSurfaceProperties.CoatRoughness = MultiLayerParallaxData.x;
		pbrSurfaceProperties.CoatF0 = MultiLayerParallaxData.y;

		float2 coatUv = uv;
		[branch] if ((PBRFlags & TruePBR_InterlayerParallax) != 0)
		{
			coatUv = uvOriginal;
		}
		[branch] if ((PBRFlags & TruePBR_HasFeatureTexture0) != 0)
		{
			float4 sampledCoatProperties = TexRimSoftLightWorldMapOverlaySampler.Sample(SampRimSoftLightWorldMapOverlaySampler, coatUv);
			pbrSurfaceProperties.CoatColor *= sampledCoatProperties.xyz;
			pbrSurfaceProperties.CoatStrength *= sampledCoatProperties.w;
		}
		[branch] if ((PBRFlags & TruePBR_HasFeatureTexture1) != 0)
		{
			float4 sampledCoatProperties = TexBackLightSampler.Sample(SampBackLightSampler, coatUv);
			pbrSurfaceProperties.CoatRoughness *= sampledCoatProperties.w;
			[branch] if ((PBRFlags & TruePBR_CoatNormal) != 0)
			{
				coatModelNormal = normalize(mul(tbn, TransformNormal(sampledCoatProperties.xyz)));
			}

#			if !defined(DRAW_IN_WORLDSPACE)
			[flatten] if (!input.WorldSpace)
			{
				coatWorldNormal = normalize(mul(input.World[eyeIndex], float4(coatModelNormal, 0)));
			}
#			endif
		}
		pbrSurfaceProperties.CoatStrength = lerp(pbrSurfaceProperties.CoatStrength, 0, projectedMaterialWeight);
	}

	[branch] if ((PBRFlags & TruePBR_Fuzz) != 0)
	{
		pbrSurfaceProperties.FuzzColor = MultiLayerParallaxData.xyz;
		pbrSurfaceProperties.FuzzWeight = MultiLayerParallaxData.w;
		[branch] if ((PBRFlags & TruePBR_HasFeatureTexture1) != 0)
		{
			float4 sampledFuzzProperties = TexBackLightSampler.Sample(SampBackLightSampler, uv);
			pbrSurfaceProperties.FuzzColor *= sampledFuzzProperties.xyz;
			pbrSurfaceProperties.FuzzWeight *= sampledFuzzProperties.w;
		}
		pbrSurfaceProperties.FuzzWeight = lerp(pbrSurfaceProperties.FuzzWeight, 0, projectedMaterialWeight);
	}
#		endif

	float3 specularColorPBR = 0;
	float3 transmissionColor = 0;

	float pbrWeight = 1;
	float pbrGlossiness = 1 - pbrSurfaceProperties.Roughness;
#	endif  // TRUE_PBR

#	if !defined(MODELSPACENORMALS)
	float3 vertexNormal = tbnTr[2];
	float3 worldSpaceVertexNormal = vertexNormal;

#		if !defined(DRAW_IN_WORLDSPACE)
	[flatten] if (!input.WorldSpace)
		worldSpaceVertexNormal = normalize(mul(input.World[eyeIndex], float4(worldSpaceVertexNormal, 0)));
#		endif
#	else
	float3 worldSpaceVertexNormal = worldSpaceNormal;
#	endif

	float porosity = 1.0;

#	if defined(SKYLIGHTING)
#		if defined(VR)
	float3 positionMSSkylight = input.WorldPosition.xyz + CameraPosAdjust[eyeIndex].xyz - CameraPosAdjust[0].xyz;
#		else
	float3 positionMSSkylight = input.WorldPosition.xyz;
#		endif

#		if defined(DEFERRED)
	sh2 skylightingSH = Skylighting::sample(skylightingSettings, SkylightingProbeArray, positionMSSkylight, worldSpaceNormal);
#		else
	sh2 skylightingSH = inWorld ? Skylighting::sample(skylightingSettings, SkylightingProbeArray, positionMSSkylight, worldSpaceNormal) : float4(sqrt(4.0 * Math::PI), 0, 0, 0);
#		endif

#	endif

	float4 waterData = SharedData::GetWaterData(input.WorldPosition.xyz);
	float waterHeight = waterData.w;

	float waterRoughnessSpecular = 1;

#	if defined(WETNESS_EFFECTS)
	float wetness = 0.0;

	float wetnessDistToWater = abs(input.WorldPosition.z - waterHeight);
	float shoreFactor = saturate(1.0 - (wetnessDistToWater / (float)wetnessEffectsSettings.ShoreRange));
	float shoreFactorAlbedo = shoreFactor;

	[flatten] if (input.WorldPosition.z < waterHeight)
		shoreFactorAlbedo = 1.0;

	float minWetnessValue = wetnessEffectsSettings.MinRainWetness;

	float maxOcclusion = 1;
	float minWetnessAngle = 0;
	minWetnessAngle = saturate(max(minWetnessValue, worldSpaceNormal.z));
#		if defined(SKYLIGHTING)
	float wetnessOcclusion = inWorld ? pow(saturate(shUnproject(skylightingSH, float3(0, 0, 1))), 2) : 0;
#		else
	float wetnessOcclusion = inWorld;
#		endif

	float4 raindropInfo = float4(0, 0, 1, 0);
	if (worldSpaceNormal.z > 0 && wetnessEffectsSettings.Raining > 0.0f && wetnessEffectsSettings.EnableRaindropFx &&
		(dot(input.WorldPosition, input.WorldPosition) < wetnessEffectsSettings.RaindropFxRange * wetnessEffectsSettings.RaindropFxRange)) {
		if (wetnessOcclusion > 0.0)
#		if defined(SKINNED)
			raindropInfo = WetnessEffects::GetRainDrops(input.ModelPosition.xyz, wetnessEffectsSettings.Time, worldSpaceNormal);
#		elif defined(DEFERRED)
			raindropInfo = WetnessEffects::GetRainDrops(input.WorldPosition.xyz + CameraPosAdjust[eyeIndex].xyz, wetnessEffectsSettings.Time, worldSpaceNormal);
#		else
			raindropInfo = WetnessEffects::GetRainDrops(!FrameParams.y ? input.ModelPosition.xyz : input.WorldPosition.xyz + CameraPosAdjust[eyeIndex].xyz, wetnessEffectsSettings.Time, worldSpaceNormal);
#		endif
	}

	float rainWetness = wetnessEffectsSettings.Wetness * minWetnessAngle * wetnessEffectsSettings.MaxRainWetness;
	rainWetness = max(rainWetness, raindropInfo.w);

	float puddleWetness = wetnessEffectsSettings.PuddleWetness * minWetnessAngle;
#		if defined(SKIN)
	rainWetness = wetnessEffectsSettings.SkinWetness * wetnessEffectsSettings.Wetness;
#		endif
#		if defined(HAIR)
	rainWetness = wetnessEffectsSettings.SkinWetness * wetnessEffectsSettings.Wetness * 0.8f;
#		endif

	rainWetness *= wetnessOcclusion;
	puddleWetness *= wetnessOcclusion;

	wetness = max(shoreFactor * wetnessEffectsSettings.MaxShoreWetness, rainWetness);

	float3 wetnessNormal = worldSpaceNormal;

	float3 puddleCoords = ((input.WorldPosition.xyz + CameraPosAdjust[0].xyz) * 0.5 + 0.5) * 0.01 / wetnessEffectsSettings.PuddleRadius;
	float puddle = wetness;
	if (wetness > 0.0 || puddleWetness > 0) {
#		if !defined(SKINNED)
		puddle = Random::perlinNoise(puddleCoords) * .5 + .5;
		puddle = puddle * ((minWetnessAngle / wetnessEffectsSettings.PuddleMaxAngle) * wetnessEffectsSettings.MaxPuddleWetness * 0.25) + 0.5;
		wetness = lerp(wetness, puddleWetness, saturate(puddle - 0.25));
#		endif
		puddle *= wetness;
	}

	puddle *= nearFactor;

	float3 wetnessSpecular = 0.0;

	float wetnessGlossinessAlbedo = max(puddle, shoreFactorAlbedo * wetnessEffectsSettings.MaxShoreWetness);
	wetnessGlossinessAlbedo *= wetnessGlossinessAlbedo;

	float wetnessGlossinessSpecular = puddle;
	wetnessGlossinessSpecular = lerp(wetnessGlossinessSpecular, wetnessGlossinessSpecular * shoreFactor, input.WorldPosition.z < waterHeight);

	float flatnessAmount = smoothstep(wetnessEffectsSettings.PuddleMaxAngle, 1.0, minWetnessAngle);

	flatnessAmount *= smoothstep(wetnessEffectsSettings.PuddleMinWetness, 1.0, wetnessGlossinessSpecular);

	wetnessNormal = normalize(lerp(wetnessNormal, float3(0, 0, 1), saturate(flatnessAmount)));

	float3 rippleNormal = normalize(lerp(float3(0, 0, 1), raindropInfo.xyz, clamp(flatnessAmount, 0.0, 1.0)));
	wetnessNormal = WetnessEffects::ReorientNormal(rippleNormal, wetnessNormal);

	waterRoughnessSpecular = 1.0 - wetnessGlossinessSpecular;
#	endif

	float3 dirLightColor = DirLightColor.xyz;
	float3 dirLightColorMultiplier = 1;

#	if defined(WATER_LIGHTING)
	dirLightColorMultiplier *= WaterLighting::ComputeCaustics(waterData, input.WorldPosition.xyz, worldSpaceNormal);
#	endif

	float selfShadowFactor = 1.0f;

	float3 normalizedDirLightDirectionWS = DirLightDirection;

#	if !defined(DRAW_IN_WORLDSPACE)
	[flatten] if (!input.WorldSpace)
		normalizedDirLightDirectionWS = normalize(mul(input.World[eyeIndex], float4(DirLightDirection.xyz, 0)));
#	endif

	float dirLightAngle = dot(modelNormal.xyz, DirLightDirection.xyz);

	if ((PixelShaderDescriptor & _DefShadow) && (PixelShaderDescriptor & _ShadowDir)) {
		dirLightColorMultiplier *= shadowColor.x;
	}
#	if !defined(DEFERRED)
	else if (!InInterior && inWorld) {
		dirLightColorMultiplier *= GetLightingShadow(screenNoise, input.WorldPosition.xyz, eyeIndex);
	}
#	endif

#	if defined(SOFT_LIGHTING) || defined(BACK_LIGHTING) || defined(RIM_LIGHTING)
	bool inDirShadow = ((PixelShaderDescriptor & _DefShadow) && (PixelShaderDescriptor & _ShadowDir) && shadowColor.x == 0);
#	else
	bool inDirShadow = ((PixelShaderDescriptor & _DefShadow) && (PixelShaderDescriptor & _ShadowDir) && shadowColor.x == 0) && dirLightAngle > 0.0;
#	endif

	float3 refractedDirLightDirection = DirLightDirection;
#	if defined(TRUE_PBR)
	[branch] if ((PBRFlags & TruePBR_InterlayerParallax) != 0)
	{
		refractedDirLightDirection = -refract(-DirLightDirection, coatModelNormal, eta);
	}
#	endif

	float dirDetailShadow = 1.0;
	float dirShadow = 1.0;
	float parallaxShadow = 1;

#	if defined(DEFERRED)
#		if defined(SOFT_LIGHTING) || defined(BACK_LIGHTING) || defined(RIM_LIGHTING)
	bool extraDirShadows = !inDirShadow && inWorld;
#		else
	// If lighting cannot hit the backface of the object, do not render shadows
	bool extraDirShadows = !inDirShadow && dirLightAngle > 0.0 && inWorld;
#		endif
#	else
#		if defined(SOFT_LIGHTING) || defined(BACK_LIGHTING) || defined(RIM_LIGHTING)
	bool extraDirShadows = !inDirShadow && inWorld;
#		else
	bool extraDirShadows = !inDirShadow && dirLightAngle > 0.0 && inWorld;
#		endif
#	endif

	if (extraDirShadows) {
#	if defined(DEFERRED) && defined(SCREEN_SPACE_SHADOWS)
#		if defined(SOFT_LIGHTING) || defined(BACK_LIGHTING) || defined(RIM_LIGHTING)
		if (dirLightAngle > 0.0)
#		endif
		{
			dirDetailShadow = ScreenSpaceShadows::GetScreenSpaceShadow(input.Position.xyz, screenUV, screenNoise, viewPosition, eyeIndex);
#		if defined(TREE_ANIM)
			PerGeometry sD = SharedPerShadow[0];
			dirDetailShadow = lerp(1.0, dirDetailShadow, saturate(viewPosition.z / sqrt(sD.ShadowLightParam.z)));
#		endif
		}
#	endif

#	if defined(EMAT) && (defined(SKINNED) || !defined(MODELSPACENORMALS))
		[branch] if (!inDirShadow && extendedMaterialSettings.EnableShadows)
		{
			float3 dirLightDirectionTS = mul(refractedDirLightDirection, tbn).xyz;
#		if defined(LANDSCAPE)
			[branch] if (extendedMaterialSettings.EnableTerrainParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplierTerrain(input, uv, mipLevels, dirLightDirectionTS, sh0, parallaxShadowQuality, screenNoise, displacementParams);
#		elif defined(PARALLAX)
			[branch] if (extendedMaterialSettings.EnableParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, dirLightDirectionTS, sh0, TexParallaxSampler, SampParallaxSampler, 0, parallaxShadowQuality, screenNoise, displacementParams);
#		elif defined(ENVMAP)
			[branch] if (complexMaterialParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, dirLightDirectionTS, sh0, TexEnvMaskSampler, SampEnvMaskSampler, 3, parallaxShadowQuality, screenNoise, displacementParams);
#		elif defined(TRUE_PBR) && !defined(LODLANDSCAPE)
			[branch] if (PBRParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, dirLightDirectionTS, sh0, TexParallaxSampler, SampParallaxSampler, 0, parallaxShadowQuality, screenNoise, displacementParams);
#		endif  // LANDSCAPE
		}
#	endif  // defined(EMAT) && (defined (SKINNED) || !defined \
				// (MODELSPACENORMALS))

#	if defined(TERRAIN_SHADOWS)
		float terrainShadow = TerrainShadows::GetTerrainShadow(input.WorldPosition.xyz + CameraPosAdjust[eyeIndex].xyz, SampColorSampler);
		dirShadow *= terrainShadow;
		inDirShadow = inDirShadow || dirShadow == 0.0;
#	endif
	}

#	if defined(CLOUD_SHADOWS)
	if (!inDirShadow) {
		dirShadow *= CloudShadows::GetCloudShadowMult(input.WorldPosition.xyz, SampColorSampler);
	}
#	endif

	dirLightColorMultiplier *= dirShadow;

	float3 diffuseColor = 0.0.xxx;
	float3 specularColor = 0.0.xxx;

	float3 lightsDiffuseColor = 0.0.xxx;
	float3 coatLightsDiffuseColor = 0.0.xxx;
	float3 lightsSpecularColor = 0.0.xxx;

	float3 lodLandDiffuseColor = 0;

#	if defined(TRUE_PBR)
	{
		PBR::LightProperties lightProperties = PBR::InitLightProperties(dirLightColor, dirLightColorMultiplier * dirDetailShadow, parallaxShadow);
		float3 dirDiffuseColor, coatDirDiffuseColor, dirTransmissionColor, dirSpecularColor;
		PBR::GetDirectLightInput(dirDiffuseColor, coatDirDiffuseColor, dirTransmissionColor, dirSpecularColor, modelNormal.xyz, coatModelNormal, refractedViewDirection, viewDirection, refractedDirLightDirection, DirLightDirection, lightProperties, pbrSurfaceProperties, tbnTr, uvOriginal);
		lightsDiffuseColor += dirDiffuseColor;
		coatLightsDiffuseColor += coatDirDiffuseColor;
		transmissionColor += dirTransmissionColor;
		specularColorPBR += dirSpecularColor * !InInterior;
#		if defined(LOD_LAND_BLEND)
		lodLandDiffuseColor += dirLightColor * saturate(dirLightAngle) * dirLightColorMultiplier * dirDetailShadow * parallaxShadow;
#		endif
#		if defined(WETNESS_EFFECTS)
		if (waterRoughnessSpecular < 1.0)
			specularColorPBR += PBR::GetWetnessDirectLightSpecularInput(wetnessNormal, worldSpaceViewDirection, normalizedDirLightDirectionWS, lightProperties.LinearCoatLightColor, waterRoughnessSpecular) * wetnessGlossinessSpecular;
#		endif
	}
#	else
	dirDetailShadow *= parallaxShadow;
	dirLightColor *= dirLightColorMultiplier;
	float3 dirDiffuseColor = dirLightColor * saturate(dirLightAngle) * dirDetailShadow;

#		if defined(SOFT_LIGHTING) || defined(RIM_LIGHTING) || defined(BACK_LIGHTING)
	float backlighting = 1.0 + saturate(dot(viewDirection, -DirLightDirection.xyz));
#		endif

#		if defined(SOFT_LIGHTING)
	lightsDiffuseColor += dirLightColor * GetSoftLightMultiplier(dirLightAngle) * rimSoftLightColor.xyz * backlighting;
#		endif

#		if defined(RIM_LIGHTING)
	lightsDiffuseColor += dirLightColor * GetRimLightMultiplier(DirLightDirection, viewDirection, modelNormal.xyz) * rimSoftLightColor.xyz * backlighting;
#		endif

#		if defined(BACK_LIGHTING)
	lightsDiffuseColor += dirLightColor * saturate(-dirLightAngle) * backLightColor.xyz * backlighting;
#		endif

	if (useSnowSpecular && useSnowDecalSpecular) {
#		if defined(SNOW)
		lightsSpecularColor += GetSnowSpecularColor(input, modelNormal.xyz, viewDirection);
#		endif
	} else {
#		if defined(SPECULAR) || defined(SPARKLE)
		lightsSpecularColor = GetLightSpecularInput(input, DirLightDirection, viewDirection, modelNormal.xyz, dirLightColor.xyz * dirDetailShadow, shininess, uv);
#		endif
	}

	lightsDiffuseColor += dirDiffuseColor;

#		if defined(WETNESS_EFFECTS)
	if (waterRoughnessSpecular < 1.0)
		wetnessSpecular += WetnessEffects::GetWetnessSpecular(wetnessNormal, normalizedDirLightDirectionWS, worldSpaceViewDirection, Color::GammaToLinear(dirLightColor * dirDetailShadow) / Color::LightPreMult, waterRoughnessSpecular);
#		endif
#	endif

#	if !defined(LOD)
#		if !defined(LIGHT_LIMIT_FIX)
	[loop] for (uint lightIndex = 0; lightIndex < numLights; lightIndex++)
	{
		float3 lightDirection = PointLightPosition[eyeIndex * numLights + lightIndex].xyz - input.InputPosition.xyz;
		float lightDist = length(lightDirection);
		float intensityFactor = saturate(lightDist / PointLightPosition[lightIndex].w);
		if (intensityFactor == 1)
			continue;

		float intensityMultiplier = 1 - intensityFactor * intensityFactor;
		float3 lightColor = PointLightColor[lightIndex].xyz * intensityMultiplier;
		float lightShadow = 1.f;
		if (PixelShaderDescriptor & _DefShadow) {
			if (lightIndex < numShadowLights) {
				lightShadow *= shadowColor[ShadowLightMaskSelect[lightIndex]];
			}
		}

		float3 normalizedLightDirection = normalize(lightDirection);

#			if defined(TRUE_PBR)
		{
			float3 pointDiffuseColor, coatPointDiffuseColor, pointTransmissionColor, pointSpecularColor;
			float3 refractedLightDirection = normalizedLightDirection;
			[branch] if ((PBRFlags & TruePBR_InterlayerParallax) != 0)
			{
				refractedLightDirection = -refract(-normalizedLightDirection, coatModelNormal, eta);
			}
			PBR::LightProperties lightProperties = PBR::InitLightProperties(lightColor, lightShadow, 1);
			PBR::GetDirectLightInput(pointDiffuseColor, coatPointDiffuseColor, pointTransmissionColor, pointSpecularColor, modelNormal.xyz, coatModelNormal, refractedViewDirection, viewDirection, refractedLightDirection, normalizedLightDirection, lightProperties, pbrSurfaceProperties, tbnTr, uvOriginal);
			lightsDiffuseColor += pointDiffuseColor;
			coatLightsDiffuseColor += coatPointDiffuseColor;
			transmissionColor += pointTransmissionColor;
			specularColorPBR += pointSpecularColor;
		}
#			else
		lightColor *= lightShadow;
		float lightAngle = dot(modelNormal.xyz, normalizedLightDirection.xyz);
		float3 lightDiffuseColor = lightColor * saturate(lightAngle.xxx);

#				if defined(SOFT_LIGHTING)
		lightDiffuseColor += lightColor * GetSoftLightMultiplier(lightAngle) * rimSoftLightColor.xyz;
#				endif  // SOFT_LIGHTING

#				if defined(RIM_LIGHTING)
		lightDiffuseColor += lightColor * GetRimLightMultiplier(normalizedLightDirection, viewDirection, modelNormal.xyz) * rimSoftLightColor.xyz;
#				endif  // RIM_LIGHTING

#				if defined(BACK_LIGHTING)
		lightDiffuseColor += lightColor * saturate(-lightAngle) * backLightColor.xyz;
#				endif  // BACK_LIGHTING

#				if defined(SPECULAR) || (defined(SPARKLE) && !defined(SNOW))
		lightsSpecularColor += GetLightSpecularInput(input, normalizedLightDirection, viewDirection, modelNormal.xyz, lightColor, shininess, uv);
#				endif  // defined (SPECULAR) || (defined (SPARKLE) && !defined(SNOW))

		lightsDiffuseColor += lightDiffuseColor;
#			endif
	}

#		else

#			if defined(ANISO_LIGHTING)
	input.TBN0.z = worldSpaceVertexNormal[0];
	input.TBN1.z = worldSpaceVertexNormal[1];
	input.TBN2.z = worldSpaceVertexNormal[2];
#			endif

	uint numClusteredLights = 0;
	uint totalLightCount = strictLights[0].NumStrictLights;
	uint clusterIndex = 0;
	uint lightOffset = 0;
	if (inWorld && LightLimitFix::GetClusterIndex(screenUV, viewPosition.z, clusterIndex)) {
		numClusteredLights = lightGrid[clusterIndex].lightCount;
		totalLightCount += numClusteredLights;
		lightOffset = lightGrid[clusterIndex].offset;
	}

	uint contactShadowSteps = round(4.0 * (1.0 - saturate(viewPosition.z / 1024.0)));

	[loop] for (uint lightIndex = 0; lightIndex < totalLightCount; lightIndex++)
	{
		StructuredLight light;
		if (lightIndex < strictLights[0].NumStrictLights) {
			light = strictLights[0].StrictLights[lightIndex];
		} else {
			uint clusteredLightIndex = lightList[lightOffset + (lightIndex - strictLights[0].NumStrictLights)];
			light = lights[clusteredLightIndex];

			if (LightLimitFix::IsLightIgnored(light)) {
				continue;
			}
		}

		float3 lightDirection = light.positionWS[eyeIndex].xyz - input.WorldPosition.xyz;
		float lightDist = length(lightDirection);
		float intensityFactor = saturate(lightDist / light.radius);
		if (intensityFactor == 1)
			continue;

		float intensityMultiplier = 1 - intensityFactor * intensityFactor;
		float3 lightColor = light.color.xyz * intensityMultiplier;
		float lightShadow = 1.f;

		float shadowComponent = 1.0;
		if ((PixelShaderDescriptor & _DefShadow) && (light.lightFlags & Llf_ShadowLight)) {
			shadowComponent = shadowColor[light.shadowLightIndex];
			lightShadow *= shadowComponent;
		}

		float3 normalizedLightDirection = normalize(lightDirection);
		float lightAngle = dot(worldSpaceNormal.xyz, normalizedLightDirection.xyz);

		float contactShadow = 1;
		[branch] if (inWorld && !FrameParams.z && lightLimitFixSettings.EnableContactShadows && shadowComponent != 0.0 && lightAngle > 0.0)
		{
			float3 normalizedLightDirectionVS = normalize(light.positionVS[eyeIndex].xyz - viewPosition.xyz);
			contactShadow = LightLimitFix::ContactShadows(viewPosition, screenNoise, normalizedLightDirectionVS, contactShadowSteps, eyeIndex);
		}

		float3 refractedLightDirection = normalizedLightDirection;
#			if defined(TRUE_PBR)
		[branch] if ((PBRFlags & TruePBR_InterlayerParallax) != 0)
		{
			refractedLightDirection = -refract(-normalizedLightDirection, coatWorldNormal, eta);
		}
#			endif

		float parallaxShadow = 1;

#			if defined(EMAT)
		[branch] if (extendedMaterialSettings.EnableShadows && lightAngle > 0.0 && shadowComponent != 0.0 && contactShadow != 0.0)
		{
			float3 lightDirectionTS = normalize(mul(refractedLightDirection, tbn).xyz);
#				if defined(PARALLAX)
			[branch] if (extendedMaterialSettings.EnableParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, lightDirectionTS, sh0, TexParallaxSampler, SampParallaxSampler, 0, parallaxShadowQuality, screenNoise, displacementParams);
#				elif defined(LANDSCAPE)
			[branch] if (extendedMaterialSettings.EnableTerrainParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplierTerrain(input, uv, mipLevels, lightDirectionTS, sh0, parallaxShadowQuality, screenNoise, displacementParams);
#				elif defined(ENVMAP)
			[branch] if (complexMaterialParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, lightDirectionTS, sh0, TexEnvMaskSampler, SampEnvMaskSampler, 3, parallaxShadowQuality, screenNoise, displacementParams);
#				elif defined(TRUE_PBR) && !defined(LODLANDSCAPE)
			[branch] if (PBRParallax)
				parallaxShadow = ExtendedMaterials::GetParallaxSoftShadowMultiplier(uv, mipLevel, lightDirectionTS, sh0, TexParallaxSampler, SampParallaxSampler, 0, parallaxShadowQuality, screenNoise, displacementParams);
#				endif
		}
#			endif

#			if defined(TRUE_PBR)
		{
			PBR::LightProperties lightProperties = PBR::InitLightProperties(lightColor, lightShadow * contactShadow, parallaxShadow);
			float3 pointDiffuseColor, coatPointDiffuseColor, pointTransmissionColor, pointSpecularColor;
			PBR::GetDirectLightInput(pointDiffuseColor, coatPointDiffuseColor, pointTransmissionColor, pointSpecularColor, worldSpaceNormal.xyz, coatWorldNormal, refractedViewDirectionWS, worldSpaceViewDirection, refractedLightDirection, normalizedLightDirection, lightProperties, pbrSurfaceProperties, tbnTr, uvOriginal);
			lightsDiffuseColor += pointDiffuseColor;
			coatLightsDiffuseColor += coatPointDiffuseColor;
			transmissionColor += pointTransmissionColor;
			specularColorPBR += pointSpecularColor;
#				if defined(WETNESS_EFFECTS)
			if (waterRoughnessSpecular < 1.0)
				specularColorPBR += PBR::GetWetnessDirectLightSpecularInput(wetnessNormal, worldSpaceViewDirection, normalizedLightDirection, lightProperties.LinearCoatLightColor, waterRoughnessSpecular) * wetnessGlossinessSpecular;
#				endif
		}
#			else
		lightColor *= parallaxShadow * lightShadow;

		float3 lightDiffuseColor = lightColor * contactShadow * saturate(lightAngle.xxx);

#				if defined(SOFT_LIGHTING)
		lightDiffuseColor += lightColor * GetSoftLightMultiplier(lightAngle) * rimSoftLightColor.xyz;
#				endif

#				if defined(RIM_LIGHTING)
		lightDiffuseColor += lightColor * GetRimLightMultiplier(normalizedLightDirection, worldSpaceViewDirection, worldSpaceNormal.xyz) * rimSoftLightColor.xyz;
#				endif

#				if defined(BACK_LIGHTING)
		lightDiffuseColor += lightColor * saturate(-lightAngle) * backLightColor.xyz;
#				endif

#				if defined(SPECULAR) || (defined(SPARKLE) && !defined(SNOW))
		lightsSpecularColor += GetLightSpecularInput(input, normalizedLightDirection, worldSpaceViewDirection, worldSpaceNormal.xyz, lightColor, shininess, uv);
#				endif

		lightsDiffuseColor += lightDiffuseColor;
#			endif

#			if defined(WETNESS_EFFECTS)
		if (waterRoughnessSpecular < 1.0)
			wetnessSpecular += WetnessEffects::GetWetnessSpecular(wetnessNormal, normalizedLightDirection, worldSpaceViewDirection, Color::GammaToLinear(lightColor) / Color::LightPreMult, waterRoughnessSpecular);
#			endif
	}
#		endif
#	endif

	diffuseColor += lightsDiffuseColor;
	specularColor += lightsSpecularColor;

#	if !defined(LANDSCAPE)
	if (PixelShaderDescriptor & _CharacterLight) {
		float charLightMul =
			saturate(dot(viewDirection, modelNormal.xyz)) * CharacterLightParams.x +
			CharacterLightParams.y * saturate(dot(float2(0.164398998, -0.986393988), modelNormal.yz));
		float charLightColor = min(CharacterLightParams.w, max(0, CharacterLightParams.z * TexCharacterLightProjNoiseSampler.Sample(SampCharacterLightProjNoiseSampler, baseShadowUV).x));
#		if defined(TRUE_PBR)
		charLightColor = Color::GammaToLinear(charLightColor).x / Color::LightPreMult;
#		endif
		diffuseColor += (charLightMul * charLightColor).xxx;
	}
#	endif

#	if defined(EYE)
	modelNormal.xyz = input.EyeNormal;
#	endif  // EYE

	float3 emitColor = EmitColor;
#	if !defined(LANDSCAPE) && !defined(LODLANDSCAPE)
	bool hasEmissive = (0x3F & (PixelShaderDescriptor >> 24)) == _Glowmap;
#		if defined(TRUE_PBR)
	hasEmissive = hasEmissive || (PBRFlags & TruePBR_HasEmissive != 0);
#		endif
	[branch] if (hasEmissive)
	{
		float3 glowColor = TexGlowSampler.Sample(SampGlowSampler, uv).xyz;
		emitColor *= glowColor;
	}
#	endif

#	if !defined(TRUE_PBR)
	diffuseColor += emitColor.xyz;
#	endif

	float3 directionalAmbientColor = mul(DirectionalAmbient, modelNormal);
#	if defined(TRUE_PBR)
	directionalAmbientColor = Color::GammaToLinear(directionalAmbientColor) / Color::LightPreMult;
#	endif

	float3 reflectionDiffuseColor = diffuseColor + directionalAmbientColor;

#	if defined(SKYLIGHTING)
	float skylightingDiffuse = shFuncProductIntegral(skylightingSH, shEvaluateCosineLobe(float3(worldSpaceNormal.xy, worldSpaceNormal.z * 0.5 + 0.5))) / Math::PI;
	skylightingDiffuse = lerp(1.0, skylightingDiffuse, Skylighting::getFadeOutFactor(input.WorldPosition.xyz));
	skylightingDiffuse = Skylighting::mixDiffuse(skylightingSettings, skylightingDiffuse);
#		if !defined(TRUE_PBR)
	directionalAmbientColor = Color::GammaToLinear(directionalAmbientColor) / Color::LightPreMult;
#		endif
	directionalAmbientColor *= skylightingDiffuse;
#		if !defined(TRUE_PBR)
	directionalAmbientColor = Color::LinearToGamma(directionalAmbientColor * Color::LightPreMult);
#		endif
#	endif

#	if defined(TRUE_PBR) && defined(LOD_LAND_BLEND) && !defined(DEFERRED)
	lodLandDiffuseColor += directionalAmbientColor;
#	endif

#	if !(defined(DEFERRED) && defined(SSGI)) && !defined(TRUE_PBR)
	diffuseColor += directionalAmbientColor;
#	endif

#	if defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(EYE)
	float envMaskColor = TexEnvMaskSampler.Sample(SampEnvMaskSampler, uv).x;
	float envMask = (EnvmapData.y * (envMaskColor - glossiness) + glossiness) * (EnvmapData.x * MaterialData.x);
	float viewNormalAngle = dot(worldSpaceNormal.xyz, viewDirection);
	float3 envSamplingPoint = (viewNormalAngle * 2) * modelNormal.xyz - viewDirection;
	float4 envColorBase = TexEnvSampler.Sample(SampEnvSampler, envSamplingPoint);
	float3 envColor = envColorBase.xyz * envMask;
#		if defined(DYNAMIC_CUBEMAPS)
	float3 F0 = 0.0;
	float envRoughness = 1.0;

	uint2 envSize;
	TexEnvSampler.GetDimensions(envSize.x, envSize.y);

	bool dynamicCubemap = false;
	if (envMask != 0 && envSize.x == 1) {
		dynamicCubemap = true;
		envColorBase = TexEnvSampler.SampleLevel(SampEnvSampler, float3(1.0, 0.0, 0.0), 0);
		if (envColorBase.a < 1.0) {
			F0 = (Color::GammaToLinear(envColorBase.rgb) + Color::GammaToLinear(baseColor.rgb)) / Color::AlbedoPreMult;
			envRoughness = envColorBase.a;
		} else {
			F0 = 1.0;
			envRoughness = 1.0 / 9.0;
		}
	}

#			if defined(CREATOR)
	if (cubemapCreatorSettings.Enabled) {
		dynamicCubemap = true;
		F0 = (Color::GammaToLinear(cubemapCreatorSettings.CubemapColor.rgb) + Color::GammaToLinear(baseColor.xyz)) / Color::AlbedoPreMult;
		envRoughness = cubemapCreatorSettings.CubemapColor.a;
	}
#			endif

	if (dynamicCubemap) {
#			if defined(EMAT)
		envRoughness = lerp(envRoughness, 1.0 - complexMaterialColor.y, (float)complexMaterial);
		envRoughness *= envRoughness;
		F0 = lerp(F0, Color::GammaToLinear(complexSpecular), (float)complexMaterial);
#			endif

		envColor = DynamicCubemaps::GetDynamicCubemap(screenUV, worldSpaceNormal, worldSpaceVertexNormal, worldSpaceViewDirection, envRoughness, F0, diffuseColor, viewPosition.z) * envMask;
	}
#		endif
#	endif  // defined (ENVMAP) || defined (MULTI_LAYER_PARALLAX) || defined(EYE)

	float2 screenMotionVector = GetSSMotionVector(input.WorldPosition, input.PreviousWorldPosition, eyeIndex);

#	if defined(WETNESS_EFFECTS)
#		if !(defined(FACEGEN) || defined(FACEGEN_RGB_TINT) || defined(EYE)) || defined(TREE_ANIM)
#			if defined(TRUE_PBR)
#				if !defined(LANDSCAPE)
	[branch] if ((PBRFlags & TruePBR_TwoLayer) != 0)
	{
		porosity = 0;
	}
	else
#				endif
	{
		porosity = lerp(porosity, 0.0, saturate(sqrt(pbrSurfaceProperties.Metallic)));
	}
#			elif defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX)
	porosity = lerp(porosity, 0.0, saturate(sqrt(envMask)));
#			endif
	float wetnessDarkeningAmount = porosity * wetnessGlossinessAlbedo;
	baseColor.xyz = lerp(baseColor.xyz, pow(baseColor.xyz, 1.0 + wetnessDarkeningAmount), 0.8);
#		endif

	float3 wetnessReflectance = WetnessEffects::GetWetnessAmbientSpecular(screenUV, wetnessNormal, worldSpaceVertexNormal, worldSpaceViewDirection, 1.0 - wetnessGlossinessSpecular) * wetnessGlossinessSpecular;

#		if !defined(DEFERRED)
	wetnessSpecular += wetnessReflectance;
#		endif
#	endif

#	if defined(HAIR)
	float3 vertexColor = lerp(1, TintColor.xyz, input.Color.y);
#	else
	float3 vertexColor = input.Color.xyz;
#	endif  // defined (HAIR)

	float4 color = 0;

#	if defined(TRUE_PBR)
	{
		float3 directLightsDiffuseInput = diffuseColor * baseColor.xyz;
		[branch] if ((PBRFlags & TruePBR_ColoredCoat) != 0)
		{
			directLightsDiffuseInput = lerp(directLightsDiffuseInput, pbrSurfaceProperties.CoatColor * coatLightsDiffuseColor, pbrSurfaceProperties.CoatStrength);
		}

		color.xyz += directLightsDiffuseInput;
	}

	float3 indirectDiffuseLobeWeight, indirectSpecularLobeWeight;
	PBR::GetIndirectLobeWeights(indirectDiffuseLobeWeight, indirectSpecularLobeWeight, worldSpaceNormal.xyz, worldSpaceViewDirection, worldSpaceVertexNormal, baseColor.xyz, pbrSurfaceProperties);
#		if defined(WETNESS_EFFECTS)
	if (waterRoughnessSpecular < 1.0)
		indirectSpecularLobeWeight += PBR::GetWetnessIndirectSpecularLobeWeight(wetnessNormal, worldSpaceViewDirection, worldSpaceVertexNormal, waterRoughnessSpecular) * wetnessGlossinessSpecular;
#		endif

#		if !(defined(DEFERRED) && defined(SSGI))
	color.xyz += indirectDiffuseLobeWeight * directionalAmbientColor;
#		endif

#		if !defined(DYNAMIC_CUBEMAPS)
	specularColorPBR += indirectSpecularLobeWeight * directionalAmbientColor;
#		endif

#		if !defined(DEFERRED)
#			if defined(DYNAMIC_CUBEMAPS)
	specularColorPBR += indirectSpecularLobeWeight * DynamicCubemaps::GetDynamicCubemapSpecularIrradiance(screenUV, worldSpaceNormal, worldSpaceVertexNormal, worldSpaceViewDirection, pbrSurfaceProperties.Roughness, viewPosition.z);
#			endif
#		else
	indirectDiffuseLobeWeight *= vertexColor;
#		endif

	color.xyz += emitColor.xyz;
	color.xyz += transmissionColor;
#	else
	color.xyz += diffuseColor * baseColor.xyz;
#	endif

	color.xyz *= vertexColor;

#	if defined(MULTI_LAYER_PARALLAX)
	float layerValue = MultiLayerParallaxData.x * TexLayerSampler.Sample(SampLayerSampler, uv).w;
	float3 tangentViewDirection = mul(viewDirection, tbn);
	float3 layerNormal = MultiLayerParallaxData.yyy * (normalColor.xyz * 2.0.xxx + float3(-1, -1, -2)) + float3(0, 0, 1);
	float layerViewAngle = dot(-tangentViewDirection.xyz, layerNormal.xyz) * 2;
	float3 layerViewProjection = -layerNormal.xyz * layerViewAngle.xxx - tangentViewDirection.xyz;
	float2 layerUv = uv * MultiLayerParallaxData.zw + (0.0009765625 * (layerValue / abs(layerViewProjection.z))).xx * layerViewProjection.xy;

	float3 layerColor = TexLayerSampler.Sample(SampLayerSampler, layerUv).xyz;

	float mlpBlendFactor = saturate(viewNormalAngle) * (1.0 - baseColor.w);

#		if defined(DEFERRED) && defined(SSGI)
	color.xyz = lerp(color.xyz, (diffuseColor + directionalAmbientColor) * vertexColor * layerColor, mlpBlendFactor);
#		else
	color.xyz = lerp(color.xyz, diffuseColor * vertexColor * layerColor, mlpBlendFactor);
#		endif

#		if defined(DEFERRED)
	baseColor.xyz *= 1.0 - mlpBlendFactor;
#		endif
#	endif  // MULTI_LAYER_PARALLAX

#	if defined(SPECULAR)
#		if defined(EMAT) && defined(ENVMAP)
	specularColor = (specularColor * glossiness * MaterialData.yyy) * lerp(SpecularColor.xyz, complexSpecular, complexMaterial);
#		else
	specularColor = (specularColor * glossiness * MaterialData.yyy) * SpecularColor.xyz;
#		endif
#	elif defined(SPARKLE)
	specularColor *= glossiness;
#	endif  // SPECULAR

#	if defined(SNOW)
	if (useSnowSpecular)
		specularColor = 0;
#	endif

	diffuseColor = reflectionDiffuseColor;

#	if (defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(EYE))
#		if defined(DYNAMIC_CUBEMAPS)
	if (dynamicCubemap) {
#			if defined(DEFERRED)
		diffuseColor = 0.0;
#			else
		diffuseColor = 1.0;
#			endif
		specularColor = Color::GammaToLinear(specularColor);
	}
#		endif

#		if defined(EMAT) && defined(ENVMAP)
#			if defined(DYNAMIC_CUBEMAPS)
	envColor *= lerp(complexSpecular, 1.0, dynamicCubemap);
	specularColor += envColor * diffuseColor;
#			else
	envColor *= complexSpecular;
	specularColor += envColor * diffuseColor;
#			endif
#		else
	specularColor += envColor * diffuseColor;
#		endif
#		if defined(DYNAMIC_CUBEMAPS)
	if (dynamicCubemap)
		specularColor = Color::LinearToGamma(specularColor);
#		endif
#	endif

#	if defined(EMAT) && defined(ENVMAP)
	specularColor *= complexSpecular;
#	endif  // defined (EMAT) && defined(ENVMAP)

#	if !defined(TRUE_PBR)
#		if !defined(DEFERRED)
	color.xyz += specularColor;
#		endif
	color.xyz = Color::GammaToLinear(color.xyz);
#	endif

#	if defined(WETNESS_EFFECTS) && !defined(TRUE_PBR)
	color.xyz += wetnessSpecular * wetnessGlossinessSpecular;
#	endif

#	if defined(TRUE_PBR) && !defined(DEFERRED)
	color.xyz += specularColorPBR;
#	endif

	color.xyz = Color::LinearToGamma(color.xyz);

#	if defined(LOD_LAND_BLEND) && defined(TRUE_PBR)
	{
		pbrWeight = 1 - lodLandBlendFactor;

		float3 litLodLandColor = vertexColor * lodLandColor.xyz * lodLandFadeFactor * lodLandDiffuseColor;
		color.xyz = lerp(color.xyz, litLodLandColor, lodLandBlendFactor);

#		if defined(DEFERRED)
		specularColorPBR = lerp(specularColorPBR, 0, lodLandBlendFactor);
		indirectDiffuseLobeWeight = lerp(indirectDiffuseLobeWeight, Color::GammaToLinear(input.Color.xyz * lodLandColor * lodLandFadeFactor) / Color::AlbedoPreMult, lodLandBlendFactor);
		indirectSpecularLobeWeight = lerp(indirectSpecularLobeWeight, 0, lodLandBlendFactor);
		pbrGlossiness = lerp(pbrGlossiness, 0, lodLandBlendFactor);
#		endif
	}
#	endif  // defined(LOD_LAND_BLEND) && defined(TRUE_PBR)

#	if !defined(DEFERRED)
	if (FrameParams.y && FrameParams.z)
		color.xyz = lerp(color.xyz, input.FogParam.xyz, input.FogParam.w);
#	endif

#	if defined(TESTCUBEMAP)
#		if (defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(EYE))
#			if defined(DYNAMIC_CUBEMAPS)
	baseColor.xyz = 0.0;
	specularColor = 0.0;
	diffuseColor = 0.0;
	dynamicCubemap = true;
	envColor = 1.0;
	envRoughness = 0.0;
#			endif
#		endif
#	endif

#	if defined(LANDSCAPE) && !defined(LOD_LAND_BLEND)
	psout.Diffuse.w = 0;
#	else
	float alpha = baseColor.w;
#		if defined(EMAT) && !defined(LANDSCAPE)
#			if defined(PARALLAX)
	alpha = TexColorSampler.Sample(SampColorSampler, uvOriginal).w;
#			elif defined(TRUE_PBR)
	[branch] if (PBRParallax)
	{
		alpha = TexColorSampler.Sample(SampColorSampler, uvOriginal).w;
	}
#			endif
#		endif
#		if !defined(ADDITIONAL_ALPHA_MASK)
	alpha *= MaterialData.z;
#		else
	uint2 alphaMask = input.Position.xy;
	alphaMask.x = ((alphaMask.x << 2) & 12);
	alphaMask.x = (alphaMask.y & 3) | (alphaMask.x & ~3);
	const float maskValues[16] = {
		0.003922,
		0.533333,
		0.133333,
		0.666667,
		0.800000,
		0.266667,
		0.933333,
		0.400000,
		0.200000,
		0.733333,
		0.066667,
		0.600000,
		0.996078,
		0.466667,
		0.866667,
		0.333333,
	};

	float testTmp = 0;
	if (MaterialData.z - maskValues[alphaMask.x] < 0) {
		discard;
	}
#		endif  // !defined(ADDITIONAL_ALPHA_MASK)
#		if !(defined(TREE_ANIM) || defined(LODOBJECTSHD) || defined(LODOBJECTS))
	alpha *= input.Color.w;
#		endif  // !(defined(TREE_ANIM) || defined(LODOBJECTSHD) || defined(LODOBJECTS))
#		if defined(DO_ALPHA_TEST)
#			if defined(DEPTH_WRITE_DECALS)
	if (alpha - 0.0156862754 < 0) {
		discard;
	}
	alpha = saturate(1.05 * alpha);
#			endif  // DEPTH_WRITE_DECALS
	if (alpha - AlphaTestRefRS < 0) {
		discard;
	}
#		endif      // DO_ALPHA_TEST
#		if defined(EXTENDED_TRANSCLUCENCY) && (defined(SKINNED) || !defined(MODELSPACENORMALS)) && !(defined(SKIN) || defined(HAIR) || defined(EYE) || defined(TREE_ANIM) || defined(LODOBJECTSHD) || defined(LODOBJECTS))
	if (transclucencySettings.AlphaMode < 3) {
		if (alpha >= 0.0156862754 && alpha < 1.0) {
			alpha = alpha * (1.0 - transclucencySettings.AlphaReduction);
			float limit = 2.0 - transclucencySettings.AlphaSoftness;
			if (transclucencySettings.AlphaMode == 0) {
				alpha = ExtendedTransclucency::GetViewDependentAlphaFabric1D(alpha, viewDirection, modelNormal.xyz);
			} else if (transclucencySettings.AlphaMode == 1) {
				alpha = ExtendedTransclucency::GetViewDependentAlphaFabric2D(alpha, viewDirection, tbnTr);
			} else {
				alpha = ExtendedTransclucency::GetViewDependentAlphaNaive(alpha, viewDirection, modelNormal.xyz);
			}
			alpha = saturate(ExtendedTransclucency::SoftClamp(alpha, limit));
		}
	}
#		endif
	psout.Diffuse.w = alpha;

#	endif
#	if defined(LIGHT_LIMIT_FIX) && defined(LLFDEBUG)
	if (lightLimitFixSettings.EnableLightsVisualisation) {
		if (lightLimitFixSettings.LightsVisualisationMode == 0) {
			psout.Diffuse.xyz = LightLimitFix::TurboColormap(strictLights[0].NumStrictLights >= 7.0);
		} else if (lightLimitFixSettings.LightsVisualisationMode == 1) {
			psout.Diffuse.xyz = LightLimitFix::TurboColormap((float)strictLights[0].NumStrictLights / 15.0);
		} else if (lightLimitFixSettings.LightsVisualisationMode == 2) {
			psout.Diffuse.xyz = LightLimitFix::TurboColormap((float)numClusteredLights / 128.0);
		} else {
			psout.Diffuse.xyz = shadowColor.xyz;
		}
		baseColor.xyz = 0.0;
	} else {
		psout.Diffuse.xyz = color.xyz;
	}
#	else
	psout.Diffuse.xyz = color.xyz;
#	endif  // defined(LIGHT_LIMIT_FIX)

#	if defined(SNOW)
#		if defined(TRUE_PBR)
	psout.Parameters.x = Color::RGBToLuminanceAlternative(specularColorPBR);
	psout.Parameters.y = 0;
#		else
	psout.Parameters.x = Color::RGBToLuminanceAlternative(lightsSpecularColor);
#		endif
#	endif  // SNOW && !PBR

	psout.MotionVectors.xy = SSRParams.z > 1e-5 ? float2(1, 0) : screenMotionVector.xy;
	psout.MotionVectors.zw = float2(0, 1);

#	if !defined(DEFERRED)
	float ssrMask = glossiness;
#		if defined(TRUE_PBR)
	ssrMask = Color::RGBToLuminanceAlternative(pbrSurfaceProperties.F0);
#		endif
	psout.ScreenSpaceNormals.w = smoothstep(-1e-5 + SSRParams.x, SSRParams.y, ssrMask) * SSRParams.w;

	// Green reflections fix
	if (FrameParams.z)
		psout.ScreenSpaceNormals.w = 0.0;

	screenSpaceNormal.z = max(0.001, sqrt(8 + -8 * screenSpaceNormal.z));
	screenSpaceNormal.xy /= screenSpaceNormal.zz;
	psout.ScreenSpaceNormals.xy = screenSpaceNormal.xy + 0.5.xx;
	psout.ScreenSpaceNormals.z = 0;

#	else

#		if defined(TERRAIN_BLENDING)
	psout.Diffuse.w = blendFactorTerrain;
	psout.Depth = lerp(max(depthSampled, input.Position.z), input.Position.z, blendFactorTerrain > screenNoise);
#		endif

	psout.MotionVectors.zw = float2(0.0, psout.Diffuse.w);

	float3 outputSpecular = specularColor.xyz;
#		if defined(TRUE_PBR)
	outputSpecular = Color::LinearToGamma(specularColorPBR.xyz);
#		endif
	psout.Specular = float4(outputSpecular, psout.Diffuse.w);

	float3 outputAlbedo = baseColor.xyz * vertexColor;
#		if defined(TRUE_PBR)
	outputAlbedo = Color::LinearToGamma(indirectDiffuseLobeWeight * Color::AlbedoPreMult);
#		endif
	psout.Albedo = float4(outputAlbedo, psout.Diffuse.w);

	float outGlossiness = saturate(glossiness * SSRParams.w);

#		if defined(TRUE_PBR)
	psout.Parameters.z = pbrWeight;
#		else
	psout.Parameters.z = 0;
#		endif

#		if defined(TRUE_PBR)
	psout.Reflectance = float4(indirectSpecularLobeWeight, psout.Diffuse.w);
	psout.NormalGlossiness = float4(GBuffer::EncodeNormal(screenSpaceNormal), pbrGlossiness, psout.Diffuse.w);
#		elif defined(WETNESS_EFFECTS)
	psout.Reflectance = float4(wetnessReflectance, psout.Diffuse.w);
	psout.NormalGlossiness = float4(GBuffer::EncodeNormal(screenSpaceNormal), lerp(outGlossiness, 1.0, wetnessGlossinessSpecular), psout.Diffuse.w);
#		else
	psout.Reflectance = float4(0.0.xxx, psout.Diffuse.w);
	psout.NormalGlossiness = float4(GBuffer::EncodeNormal(screenSpaceNormal), outGlossiness, psout.Diffuse.w);
#		endif

	psout.Parameters.w = psout.Diffuse.w;

#		if (defined(ENVMAP) || defined(MULTI_LAYER_PARALLAX) || defined(EYE))
#			if defined(DYNAMIC_CUBEMAPS)
	if (dynamicCubemap) {
#				if defined(WETNESS_EFFECTS)
		psout.Reflectance.xyz = max(envColor, wetnessReflectance);
		psout.NormalGlossiness.z = lerp(1.0 - envRoughness, 1.0, wetnessGlossinessSpecular);
#				else
		psout.Reflectance.xyz = envColor;
		psout.NormalGlossiness.z = 1.0 - envRoughness;
#				endif
	}
#			endif
#		endif

#		if defined(SSS) && defined(SKIN)
	psout.Masks = float4(saturate(baseColor.a), !(ExtraShaderDescriptor & _IsBeastRace), 0, psout.Diffuse.w);
#		elif defined(WETNESS_EFFECTS)
	float wetnessNormalAmount = saturate(dot(float3(0, 0, 1), wetnessNormal) * saturate(flatnessAmount));
	psout.Masks = float4(0, 0, wetnessNormalAmount, psout.Diffuse.w);
#		else
	psout.Masks = float4(0, 0, 0, psout.Diffuse.w);
#		endif
#	endif

	return psout;
}
#endif  // PSHADER

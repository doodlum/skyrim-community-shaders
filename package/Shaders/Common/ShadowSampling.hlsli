
#if defined(WATER) || defined(EFFECT)

Texture2DArray<float4> TexShadowMapSampler : register(t25);

struct PerGeometry
{
	float4 VPOSOffset;
	float4 ShadowSampleParam;    // fPoissonRadiusScale / iShadowMapResolution in z and w
	float4 EndSplitDistances;    // cascade end distances int xyz, cascade count int z
	float4 StartSplitDistances;  // cascade start ditances int xyz, 4 int z
	float4 FocusShadowFadeParam;
	float4 DebugColor;
	float4 PropertyColor;
	float4 AlphaTestRef;
	float4 ShadowLightParam;  // Falloff in x, ShadowDistance squared in z
	float4x3 FocusShadowMapProj[4];
	// Since PerGeometry is passed between c++ and hlsl, can't have different defines due to strong typing
	float4x3 ShadowMapProj[2][3];
	float4x4 CameraViewProjInverse[2];
};

StructuredBuffer<PerGeometry> perShadow : register(t26);

float3 GetShadow(float3 positionWS, float2 offset, uint a_eyeIndex = 0)
{
	PerGeometry sD = perShadow[0];
	sD.EndSplitDistances.x = GetScreenDepth(sD.EndSplitDistances.x);
	sD.EndSplitDistances.y = GetScreenDepth(sD.EndSplitDistances.y);
	sD.EndSplitDistances.z = GetScreenDepth(sD.EndSplitDistances.z);
	sD.EndSplitDistances.w = GetScreenDepth(sD.EndSplitDistances.w);

	float shadowMapDepth = length(positionWS.xyz);

	half cascadeIndex = 0;
	half4x3 lightProjectionMatrix = sD.ShadowMapProj[a_eyeIndex][0];
	half shadowMapThreshold = sD.AlphaTestRef.y;
	half shadowRange = sD.EndSplitDistances.x;

	[flatten] if (2.5 < sD.EndSplitDistances.w && sD.EndSplitDistances.y < shadowMapDepth)
	{
		lightProjectionMatrix = sD.ShadowMapProj[a_eyeIndex][2];
		shadowMapThreshold = sD.AlphaTestRef.z;
		cascadeIndex = 2;
		shadowRange = sD.EndSplitDistances.z - sD.EndSplitDistances.y;
	}
	else if (sD.EndSplitDistances.x < shadowMapDepth)
	{
		lightProjectionMatrix = sD.ShadowMapProj[a_eyeIndex][1];
		shadowMapThreshold = sD.AlphaTestRef.z;
		cascadeIndex = 1;
		shadowRange = sD.EndSplitDistances.y - sD.EndSplitDistances.x;
	}

	half3 positionLS = mul(transpose(lightProjectionMatrix), half4(positionWS.xyz, 1)).xyz;
	float deltaZ = positionLS.z - shadowMapThreshold;

	float4 depths = TexShadowMapSampler.GatherRed(LinearSampler, half3(positionLS.xy + offset * rcp(shadowRange), cascadeIndex), 0);

	float shadow = dot(depths > deltaZ, 0.25);

	return shadow;
}

float GetFullShadow(float3 startPosWS, float4 screenPosition, uint eyeIndex = 0)
{
	float worldShadow = 1.0;

	float startDepth = length(startPosWS);

#	if defined(TERRA_OCC)
	float terrainShadow = 1.0;
	float terrainAo = 1.0;
	GetTerrainOcclusion(startPosWS + CameraPosAdjust[eyeIndex], startDepth, LinearSampler, terrainShadow, terrainAo);
	worldShadow *= terrainShadow;
	if (worldShadow == 0.0)
		return 0.0;
#	endif

#	if defined(CLOUD_SHADOWS)
	worldShadow *= GetCloudShadowMult(startPosWS, LinearSampler);
	if (worldShadow == 0.0)
		return 0.0;
#	endif

	float nearFactor = 1.0 - saturate(startDepth / 10000.0);
	uint nSteps = round(8 * nearFactor);

	if (nSteps == 0)
		return worldShadow;

	float step = 1.0 / float(nSteps);

	float noise = InterleavedGradientNoise(screenPosition, FrameCount);
	noise = noise * 2.0 * M_PI;

	half2x2 rotationMatrix = half2x2(cos(noise), sin(noise), -sin(noise), cos(noise));

	PerGeometry sD = perShadow[0];

	float vlShadow = 0;

	float2 PoissonDisk[8] = {
		float2(-0.4706069, -0.4427112),
		float2(-0.9057375, +0.3003471),
		float2(-0.3487388, +0.4037880),
		float2(+0.1023042, +0.6439373),
		float2(+0.5699277, +0.3513750),
		float2(+0.2939128, -0.1131226),
		float2(+0.7836658, -0.4208784),
		float2(+0.1564120, -0.8198990)
	};

	sD.EndSplitDistances.x = GetScreenDepth(sD.EndSplitDistances.x);
	sD.EndSplitDistances.y = GetScreenDepth(sD.EndSplitDistances.y);

	sD.StartSplitDistances.x = GetScreenDepth(sD.StartSplitDistances.x);
	sD.StartSplitDistances.y = GetScreenDepth(sD.StartSplitDistances.y);

	for (uint i = 0; i < nSteps; ++i) {
		half2 offset = mul(PoissonDisk[(float(i) + noise) % 8].xy, rotationMatrix);

		half cascadeIndex = 0;
		half4x3 lightProjectionMatrix = sD.ShadowMapProj[eyeIndex][0];
		half shadowRange = sD.EndSplitDistances.x;

		if (sD.EndSplitDistances.x < startDepth + nearFactor * 8.0 * dot(offset, float2(0, 1))) {
			lightProjectionMatrix = sD.ShadowMapProj[eyeIndex][1];
			cascadeIndex = 1;
			shadowRange = sD.EndSplitDistances.y - sD.StartSplitDistances.y;
		}

		half3 samplePositionLS = mul(transpose(lightProjectionMatrix), half4(startPosWS.xyz, 1)).xyz;

		samplePositionLS.xy += nearFactor * 8.0 * offset * rcp(shadowRange);

		float4 depths = TexShadowMapSampler.GatherRed(LinearSampler, half3(samplePositionLS.xy, cascadeIndex), 0);

		vlShadow += dot(depths > samplePositionLS.z, 0.25);
	}

	return lerp(worldShadow, min(worldShadow, vlShadow * step), nearFactor);
}

float GetVL(float3 startPosWS, float3 endPosWS, float3 normal, float2 screenPosition, inout float shadow, uint eyeIndex = 0)
{
	float worldShadow = 1.0;
	float3 worldDir = endPosWS - startPosWS;

	float startDepth = length(startPosWS);
	float depthDifference = length(worldDir);

	normal *= depthDifference * 32;

#	if defined(TERRA_OCC)
	float terrainShadow = 1.0;
	float terrainAo = 1.0;
	GetTerrainOcclusion(startPosWS + normal + CameraPosAdjust[eyeIndex], startDepth, LinearSampler, terrainShadow, terrainAo);
	worldShadow *= terrainShadow;
	if (worldShadow == 0.0)
		return 0.0;
#	endif

#	if defined(CLOUD_SHADOWS)
	worldShadow *= GetCloudShadowMult(startPosWS + normal, LinearSampler);
	if (worldShadow == 0.0)
		return 0.0;
#	endif

	shadow = worldShadow;

	float phase = saturate(dot(normalize(startPosWS.xyz), DirLightDirectionShared.xyz));

	if (phase <= 0.0)
		return 0.0;

	worldShadow *= phase;

	float nearFactor = 1.0 - saturate(startDepth / 5000.0);
	uint nSteps = round(8 * nearFactor);

	if (nSteps == 0)
		return worldShadow;

	float step = 1.0 / float(nSteps);

	float noise = InterleavedGradientNoise(screenPosition, FrameCount);

	startPosWS += worldDir * step * noise;

	noise = noise * 2.0 * M_PI;
	half2x2 rotationMatrix = half2x2(cos(noise), sin(noise), -sin(noise), cos(noise));

	PerGeometry sD = perShadow[0];
	sD.EndSplitDistances.x = GetScreenDepth(sD.EndSplitDistances.x);
	sD.EndSplitDistances.y = GetScreenDepth(sD.EndSplitDistances.y);

	sD.StartSplitDistances.x = GetScreenDepth(sD.StartSplitDistances.x);
	sD.StartSplitDistances.y = GetScreenDepth(sD.StartSplitDistances.y);

	float vlShadow = 0;

	float2 PoissonDisk[8] = {
		float2(-0.4706069, -0.4427112),
		float2(-0.9057375, +0.3003471),
		float2(-0.3487388, +0.4037880),
		float2(+0.1023042, +0.6439373),
		float2(+0.5699277, +0.3513750),
		float2(+0.2939128, -0.1131226),
		float2(+0.7836658, -0.4208784),
		float2(+0.1564120, -0.8198990)
	};

	for (uint i = 0; i < nSteps; ++i) {
		float t = saturate(i * step);
		float3 samplePositionWS = startPosWS + worldDir * t;

		half2 offset = mul(PoissonDisk[(float(i) + noise) % 8].xy, rotationMatrix);

		half cascadeIndex = 0;
		half4x3 lightProjectionMatrix = sD.ShadowMapProj[eyeIndex][0];
		half shadowRange = sD.EndSplitDistances.x;

		if (sD.EndSplitDistances.x < length(samplePositionWS) + 8.0 * dot(offset, float2(0, 1))) {
			lightProjectionMatrix = sD.ShadowMapProj[eyeIndex][1];
			cascadeIndex = 1;
			shadowRange = sD.EndSplitDistances.y - sD.StartSplitDistances.y;
		}

		half3 samplePositionLS = mul(transpose(lightProjectionMatrix), half4(samplePositionWS.xyz, 1)).xyz;

		samplePositionLS.xy += nearFactor * 8.0 * offset * rcp(shadowRange);

		float4 depths = TexShadowMapSampler.GatherRed(LinearSampler, half3(samplePositionLS.xy, cascadeIndex), 0);

		vlShadow += dot(depths > samplePositionLS.z, 0.25);
	}
	return lerp(worldShadow, min(worldShadow, vlShadow * step * phase), nearFactor);
}
#endif

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
	float4x3 ShadowMapProj[4];
	float4x4 CameraViewProjInverse;
};

Texture2DArray<float4> TexShadowMapSampler : register(t80);
StructuredBuffer<PerGeometry> perShadow : register(t81);
Texture2D<float> SkylightingTexture : register(t82);

#if defined(WATER) || defined(EFFECT)

float3 GetShadow(float3 positionWS)
{
	PerGeometry sD = perShadow[0];
	sD.EndSplitDistances.x = GetScreenDepth(sD.EndSplitDistances.x);
	sD.EndSplitDistances.y = GetScreenDepth(sD.EndSplitDistances.y);
	sD.EndSplitDistances.z = GetScreenDepth(sD.EndSplitDistances.z);
	sD.EndSplitDistances.w = GetScreenDepth(sD.EndSplitDistances.w);

	float shadowMapDepth = length(positionWS.xyz);

	half cascadeIndex = 0;
	half4x3 lightProjectionMatrix = sD.ShadowMapProj[0];
	half shadowMapThreshold = sD.AlphaTestRef.y;

	[flatten] if (2.5 < sD.EndSplitDistances.w && sD.EndSplitDistances.y < shadowMapDepth)
	{
		lightProjectionMatrix = sD.ShadowMapProj[2];
		shadowMapThreshold = sD.AlphaTestRef.z;
		cascadeIndex = 2;
	}
	else if (sD.EndSplitDistances.x < shadowMapDepth)
	{
		lightProjectionMatrix = sD.ShadowMapProj[1];
		shadowMapThreshold = sD.AlphaTestRef.z;
		cascadeIndex = 1;
	}

	half3 positionLS = mul(transpose(lightProjectionMatrix), half4(positionWS.xyz, 1)).xyz;
	float deltaZ = positionLS.z - shadowMapThreshold;

	float4 depths = TexShadowMapSampler.GatherRed(LinearSampler, half3(positionLS.xy, cascadeIndex), 0);

	float shadow = (depths.x > deltaZ) +
	               (depths.y > deltaZ) +
	               (depths.z > deltaZ) +
	               (depths.w > deltaZ);

	return shadow *= 0.25;
}

void GetVL(
	float3 worldPosition, float3 viewPosition, float rawDepth, float depth, float2 screenPosition,
	out float3 scatter, out float3 transmittance)
{
	const static uint nSteps = 16;
	const static float step = rcp(nSteps);

	// https://s.campbellsci.com/documents/es/technical-papers/obs_light_absorption.pdf
	// clear water: 0.002 cm^-1 * 1.428 cm/game unit
	float3 hue = normalize(rcp(ShallowColor));
	const float3 scatterCoeff = hue * 0.002 * 4 * 1.428;
	const float3 absorpCoeff = hue * 0.0002 * 4 * 1.428;
	const float3 extinction = scatterCoeff + absorpCoeff;

	float3 worldDir = normalize(worldPosition);
	float phase = .25 / 3.1415926535;
	float distRatio = abs(SunDir.z / worldDir.z);

	uint noiseIdx = dot(screenPosition, float2(1, lightingData[0].BufferDim.x)) + lightingData[0].Timer * 1000;
	float noise = frac(0.5 + noiseIdx * 0.38196601125);

	const float cutoffTransmittance = 1e-2;  // don't go deeper than this
#	if defined(UNDERWATER)
	const float cutoffDist = -log(cutoffTransmittance) / max(extinction.x, max(extinction.y, extinction.z));
#	else
	const float cutoffDist = -log(cutoffTransmittance) / ((1 + distRatio) * max(extinction.x, max(extinction.y, extinction.z)));
#	endif

	float deltaDepth = abs(depth - viewPosition.z);
	float marchDist = min(deltaDepth, cutoffDist);
	float3 endPointWS = worldPosition * (viewPosition.z + marchDist) / viewPosition.z;
	float sunMarchDist = marchDist * distRatio;

	PerGeometry sD = perShadow[0];
	sD.EndSplitDistances.x = GetScreenDepth(sD.EndSplitDistances.x);
	sD.EndSplitDistances.y = GetScreenDepth(sD.EndSplitDistances.y);
	sD.EndSplitDistances.z = GetScreenDepth(sD.EndSplitDistances.z);
	sD.EndSplitDistances.w = GetScreenDepth(sD.EndSplitDistances.w);

	scatter = 0;
	transmittance = 1;
	for (uint i = 0; i < nSteps; ++i) {
		float t = saturate((i + noise) * step);

		float sampleTransmittance = exp(-step * marchDist * extinction);
		transmittance *= sampleTransmittance;

		// scattering
		float shadow = 0;
		{
			float3 samplePositionWS = lerp(worldPosition, endPointWS, t);
			float shadowMapDepth = length(samplePositionWS.xyz);

			half cascadeIndex = 0;
			half4x3 lightProjectionMatrix = sD.ShadowMapProj[0];
			half shadowMapThreshold = sD.AlphaTestRef.y;

			[flatten] if (2.5 < sD.EndSplitDistances.w && sD.EndSplitDistances.y < shadowMapDepth)
			{
				lightProjectionMatrix = sD.ShadowMapProj[2];
				shadowMapThreshold = sD.AlphaTestRef.z;
				cascadeIndex = 2;
			}
			else if (sD.EndSplitDistances.x < shadowMapDepth)
			{
				lightProjectionMatrix = sD.ShadowMapProj[1];
				shadowMapThreshold = sD.AlphaTestRef.z;
				cascadeIndex = 1;
			}

			half3 samplePositionLS = mul(transpose(lightProjectionMatrix), half4(samplePositionWS.xyz, 1)).xyz;
			float deltaZ = samplePositionLS.z - shadowMapThreshold;

			float4 depths = TexShadowMapSampler.GatherRed(LinearSampler, half3(samplePositionLS.xy, cascadeIndex), 0);

			shadow = (depths.x > deltaZ) +
			         (depths.y > deltaZ) +
			         (depths.z > deltaZ) +
			         (depths.w > deltaZ);
			shadow *= 0.25;
		}

		float sunTransmittance = exp(-sunMarchDist * t * extinction) * shadow;  // assuming water surface is always level
		float inScatter = scatterCoeff * phase * sunTransmittance;
		scatter += inScatter * (1 - sampleTransmittance) / extinction * transmittance;
	}

	scatter *= SunColor;
	transmittance = exp(-deltaDepth * (1 + distRatio) * extinction);
}
#endif
struct PerPassTerraOcc
{
	uint EnableTerrainShadow;
	uint EnableTerrainAO;

	float ShadowBias;
	float ShadowSofteningRadiusAngle;
	float ShadowMinStep;
	float ShadowAnglePower;
	uint ShadowSamples;

	float AOAmbientMix;
	float AODiffuseMix;
	float AOPower;
	float AOFadeOutHeightRcp;

	float3 scale;
	float3 invScale;
	float3 offset;
	float2 zRange;

	float ShadowSofteningDiameterRcp;
	float AoDistance;
};

StructuredBuffer<PerPassTerraOcc> perPassTerraOcc : register(t25);
Texture2D<float> TexTerraOcc : register(t26);
Texture2D<float> TexNormalisedHeight : register(t27);
Texture2D<float> TexShadowHeight : register(t28);

float2 GetTerrainOcclusionUV(float2 xy)
{
	return xy * perPassTerraOcc[0].scale.xy + perPassTerraOcc[0].offset.xy;
}

float2 GetTerrainOcclusionXY(float2 uv)
{
	return (uv - perPassTerraOcc[0].offset.xy) * perPassTerraOcc[0].invScale.xy;
}

float GetTerrainZ(float norm_z)
{
	return lerp(perPassTerraOcc[0].zRange.x, perPassTerraOcc[0].zRange.y, norm_z);
}

float2 GetTerrainZ(float2 norm_z)
{
	return float2(GetTerrainZ(norm_z.x), GetTerrainZ(norm_z.y));
}

float GetTerrainSoftShadow(float2 uv, float3 dirLightDirectionWS, float startZ, SamplerState samp)
{
	float2 shadowHeight = GetTerrainZ(TexShadowHeight.SampleLevel(samp, uv, 0));
	float shadowFraction = (startZ - shadowHeight.y - perPassTerraOcc[0].ShadowBias) / (shadowHeight.x - shadowHeight.y);
	return saturate(shadowFraction);
}


TextureCube<float4> unfilteredEnvTexture : register(t64);
TextureCube<float4> irradianceTexture    : register(t65);
TextureCube<float4> specularTexture	     : register(t66);
Texture2D<float4>   specularBRDF_LUT     : register(t67);

#define LinearSampler SampShadowMaskSampler

float3 GetPBRAmbientSpecular(float3 N, float3 V, float Roughness, float3 F0)
{   
    float3 R = reflect(-V, N);
    float NoV = saturate(dot(N, V));

    float level = Roughness * 6.0;

    float3 specularIrradiance = specularTexture.SampleLevel(SampColorSampler, R, level).rgb;
    
	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.Sample(LinearSampler, float2(NoV, Roughness));

    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}

float3 GetDynamicCubemap(float3 N, float3 V, float Roughness, float3 F0)
{   
    float3 R = reflect(-V, N);
    float NoV = saturate(dot(N, V));
    
    Roughness *= Roughness;
    Roughness *= Roughness;

    float level = Roughness * 6.0;

    float3 specularIrradiance = specularTexture.SampleLevel(SampColorSampler, R, level).rgb;

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.SampleLevel(LinearSampler, float2(NoV, Roughness), 0);

    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}
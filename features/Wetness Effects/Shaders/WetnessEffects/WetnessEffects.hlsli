struct PerPassWetnessEffects
{
    float Wetness;
	float WaterHeight;
    row_major float3x4 DirectionalAmbientWS;
    
	bool EnableWetnessEffects;
    float AlbedoColorPow;
    float MinimumRoughness;
    uint WaterEdgeRange;
};

StructuredBuffer<PerPassWetnessEffects> perPassWetnessEffects : register(t22);

#define LinearSampler SampShadowMaskSampler

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
float2 EnvBRDFApprox( float3 F0, float Roughness, float NoV )
{
	const float4 c0 = { -1, -0.0275, -0.572, 0.022 };
	const float4 c1 = { 1, 0.0425, 1.04, -0.04 };
	float4 r = Roughness * c0 + c1;
	float a004 = min( r.x * r.x, exp2( -9.28 * NoV ) ) * r.x + r.y;
	float2 AB = float2( -1.04, 1.04 ) * a004 + r.zw;
	return AB;
}

#if !defined(DYNAMIC_CUBEMAPS)
float3 GetPBRAmbientSpecular(float3 N, float3 V, float Roughness, float3 F0)
{   
    float3 R = reflect(-V, N);
    float NoV = saturate(dot(N, V));

    float3 specularIrradiance = mul(perPassWetnessEffects[0].DirectionalAmbientWS, float4(-R, 1.0)) * (1.0 / PI);

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = EnvBRDFApprox(F0, Roughness, NoV);

    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}
#endif

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1 - F0) * pow(saturate(1 - cosTheta), 5);
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1) + 1);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float cosTheta, float roughness)
{
    float r = (roughness + 1);
    float k = (r * r) / 8;

    float num = cosTheta;
    float denom = cosTheta * (1 - k) + k;
	
    return num / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
	
    return ggxV * ggxL;
}

float3 GetWetnessSpecular(float3 N, float3 L, float3 V, float3 lightColor, float waterGlossiness)
{	
    float roughness = lerp(1.0, perPassWetnessEffects[0].MinimumRoughness, waterGlossiness);

    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float HdotV = saturate(dot(H, V));
    float NdotH = saturate(dot(N, H));
        
    float NDF = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, 0.02);
                
    float3 numerator = NDF * G * F;
    float denominator = 4 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;
            
    return specular * NdotL * lightColor;
}

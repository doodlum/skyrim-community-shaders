struct PerPassRainWetnessEffects
{
    float Wetness;
	float WaterHeight;
    row_major float3x4 DirectionalAmbientWS;
    
	bool EnableRainWetnessEffects;
    float AlbedoColorPow;
    float WetnessWaterEdgeRange;
};

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);

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

    float3 specularIrradiance = mul(perPassRainWetnessEffects[0].DirectionalAmbientWS, float4(-R, 1.0)) * (1.0 / PI);

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = EnvBRDFApprox(F0, Roughness, NoV);

    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}
#endif

float Square(float x)
{
    return x * x;
}

// Eta is relative IOR : N2/N1 with to / From
// CosTheta is dotVH
float FullFresnel(float Eta, float CosTheta)
{
   float c = CosTheta;
   float temp = Eta* Eta + c * c - 1;

   if (temp < 0)
      return 1;

   float g = sqrt(temp);
   return 0.5 * Square((g - c) / (g + c)) *
                       (1 + Square(( (g + c)  * c - 1) / ((g - c) * c + 1)));
}

// FresnelConductor Exact
float3 FresnelConductor(float3 I, float3 N, float N1, float3 N2, float3 K)
{
   float3 Eta = N2 / N1;
   float3 Etak = K / N1;
   float3 CosTheta = dot(N, I);

   float CosTheta2 = CosTheta * CosTheta;
   float SinTheta2 = 1 - CosTheta2;
   float3 Eta2 = Eta * Eta;
   float3 Etak2 = Etak * Etak;

   float3 t0 = Eta2 - Etak2 - SinTheta2;
   float3 a2plusb2 = sqrt(t0 * t0 + 4 * Eta2 * Etak2);
   float3 t1 = a2plusb2 + CosTheta2;
   float3 a = sqrt(0.5f * (a2plusb2 + t0));
   float3 t2 = 2 * a * CosTheta;
   float3 Rs = (t1 - t2) / (t1 + t2);

   float3 t3 = CosTheta2 * a2plusb2 + SinTheta2 * SinTheta2;
   float3 t4 = t2 * SinTheta2;   
   float3 Rp = Rs * (t3 - t4) / (t3 + t4);

   return 0.5f * (Rp + Rs);
}



float3 fresnelSchlick(float cosTheta, float3 F0)
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
    float roughness = lerp(1.0, 0.1, waterGlossiness);

    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float HdotV = saturate(dot(H, V));
    float NdotH = saturate(dot(N, H));
        
    float NDF = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = fresnelSchlick(HdotV, 0.02);
                
    float3 numerator = NDF * G * F;
    float denominator = 4 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;
            
    return specular * NdotL * lightColor;
}



struct PerPassRainWetnessEffects
{
	bool EnableRainWetnessEffects;
    float AlbedoColorPow;
    float WetnessWaterEdgeRange;
	float wetness;
	float waterHeight;
};

StructuredBuffer<PerPassRainWetnessEffects> perPassRainWetnessEffects : register(t22);

TextureCube<float4> unfilteredEnvTexture : register(t64);
TextureCube<float4> irradianceTexture    : register(t65);
TextureCube<float4> specularTexture	     : register(t66);
Texture2D<float4>   specularBRDF_LUT     : register(t67);

#define LinearSampler SampShadowMaskSampler

float3 GetPBRAmbientSpecular(float3 N, float3 V, float Roughness, float3 F0)
{   
    float3 R = reflect(-V, N);
    float NoV = saturate(dot(N, V));

    Roughness *= Roughness;

    float3 specularIrradiance = specularTexture.Sample(LinearSampler, R).rgb;

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.Sample(LinearSampler, float2(NoV, Roughness));

    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}

float3 GetDynamicCubemap(float3 N, float3 V, float Roughness, float3 F0)
{   
    float3 R = reflect(-V, N);
    float NoV = saturate(dot(N, V));
    
    Roughness *= Roughness;

    float level = Roughness;

#if defined(ENVMAP)
    float3 specularIrradiance = specularTexture.SampleLevel(SampEnvMaskSampler, R, level).rgb;
#else
    float3 specularIrradiance = specularTexture.SampleLevel(LinearSampler, R, level).rgb;
#endif

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.SampleLevel(LinearSampler, float2(NoV, Roughness), 0);
    //return specularIrradiance;
    return specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);
}

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

float3 GetWetnessSpecular(float3 N, float3 L, float3 V, float3 lightColor, float waterGlossiness)
{
    // Precalc values for lighting equation
    float3 V1 = normalize(V);      
    float3 L1 = normalize(L);  
    float3 H1 = normalize(L1 + V1);

    float  dotVH1 = saturate(dot(V1, H1));
    float  dotNH1 = saturate(dot(N, H1));
    float  dotNL1 = saturate(dot(N, L1));
    float  dotNV1 = saturate(dot(N, V1));

    // IOR  of water is 1.33 or 0.02 at normal incident  
    float F12 = FullFresnel(1.33, dotVH1);
    float T12 = 1 - F12;
    // Simulate a mirror-like reflection for water
    float Fr1 = F12 * ((200 + 2.0) / 8.0) * pow(dotNH1, 200);

    float3 color = lightColor * Fr1 * dotNL1;
    return color * waterGlossiness;
}

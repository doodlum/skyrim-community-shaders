//--------------------------------------------------------------------------------
//SETTINGS
//--------------------------------------------------------------------------------

//Select the diffusion model
#define DIFFUSE_MODEL 3     //[1-3]
// 1 = Lambert
// 2 = Oren-Nayar
// 3 = Burley

#define SPECULAR_MODEL 1
// 1 = GGX

static const float PI = 3.14159265;

float GeometrySchlickGGX(float cosTheta, float roughness)
{
    float r = (roughness + 1);
    float k = (r * r) / 8;

    float num = cosTheta;
    float denom = cosTheta * (1 - k) + k;
	
    return num / denom;
}

//几何遮蔽
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

//UE4菲尼尔
float3 FastfresnelSchlick(float LoH, float3 F0)
{
    return F0 + (1 - F0) * exp2((-5.55473 * LoH - 6.98316) * LoH);
}
//GGX微表面分布
float DistributionGGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

//Oren-Nayar漫射模型
float OrenNayarDiffuseCoefficient(float roughness, float3 N, float3 L, float3 V, float NdotL, float NdotV)
{
    float gamma = dot(V - N * NdotV, L - N * NdotL);
    float a = roughness * roughness;
    float A = 1 - 0.5 * (a / (a + 0.57));
    float B = 0.45 * (a / (a + 0.09));
    float C = sqrt((1 - NdotV * NdotV) * (1 - NdotL * NdotL)) / max(NdotV, NdotL);
    return (A + B * max(0.0f, gamma) * C) / PI;

}

float F_Schlick(float u, float f0, float f90) 
{
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

//Burley漫射模型
float Fd_Burley(float NoV, float NoL, float LoH, float roughness) 
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter;
}
//完整光照模型，第一列为漫射光照，第二列为反射光照
float2x3 GetLighting(float3 N, float3 L, float3 V, float NoV, float3 F0, float3 originalRadiance, float roughness)
{	
    float3 H = normalize(V + L);
    float NoL = saturate(dot(N, L));
    float LoH = saturate(dot(H, V));
    float NoH = saturate(dot(N, H));
        
    float NDF = DistributionGGX(NoH, roughness);
    float G = GeometrySmith(NoV, NoL, roughness);
    float3 F = FastfresnelSchlick(LoH, F0);
        
    float3 kD = 1 - F;
        
    float3 numerator = NDF * G * F;
    float denominator = 4 * NoV * NoL + 0.0001;
    float3 specular = numerator / denominator * PI;

	float diffuse_Scatter;
    #if DIFFUSE_MODEL == 1
        diffuse_Scatter = 1;
    #elif DIFFUSE_MODEL == 2
        diffuse_Scatter = OrenNayarDiffuseCoefficient(roughness, N, L, V, NoL, NoV);
    #elif DIFFUSE_MODEL == 3
        diffuse_Scatter = Fd_Burley(NoV, NoL, LoH, roughness);
    #endif

	float3 irradiance = NoL * originalRadiance;

	float2x3 lighting;
	lighting[0] = kD * irradiance * diffuse_Scatter;
	lighting[1] = specular * irradiance;
            
	return lighting;
}
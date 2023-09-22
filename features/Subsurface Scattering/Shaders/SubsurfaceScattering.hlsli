
struct PerPassSSS
{
	uint SkinMode;
    uint pad[3];
};

StructuredBuffer<PerPassSSS> perPassSSS : register(t35);

#if (defined(FACEGEN) || defined(FACEGEN_RGB_TINT)) && defined(SOFT_LIGHTING) && defined(MODELSPACENORMALS)
#define SKIN_SHADING
//#define SKIN
#endif



float3 sRGB2Lin(float3 Color)
{ 
    return Color > 0.04045 ? pow(Color / 1.055 + 0.055 / 1.055, 2.4) : Color / 12.92; }

float3 Lin2sRGB(float3 Color)
{ 

    return Color > 0.0031308 ? 1.055 * pow(Color, 1.0/2.4) - 0.055 : 12.92 * Color; }

// #if (defined(FACEGEN) || defined(FACEGEN_RGB_TINT)) && defined(SOFT_LIGHTING) && defined(MODELSPACENORMALS) && false
// #define SKIN

// https://github.com/zesterer/openmw-volumetric-clouds
// https://github.com/codewings/PreIntegrated-Skin

float3 RGB2HSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 HSV2RGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

#define SATURATION 1.0
#define HUE_SHIFT -0.01

void RemoveTonemap(inout float3 color) {
    const float k = 1.5;
    color = -log2(1.0 - color);
    color = float3(1.0, 1.0, 1.0) - exp2(color * -k);
}

void Material2PBR(float3 color, out float3 albedo, out float ao) {
   // RemoveTonemap(color);
    float3 hsv = RGB2HSV(color);
    ao = hsv.z;
    albedo = HSV2RGB(float3(hsv.x + HUE_SHIFT, hsv.y * SATURATION, 1.0));
}


struct SphericalGaussian
{
    float3	Axis;		// u
    float	Sharpness;	// L
    float	Amplitude;	// a
};

struct SubsurfaceNormal
{
    float3 NormalR;
    float3 NormalG;
    float3 NormalB;
};

#define EPS 1e-4f

SphericalGaussian MakeNormalizedSG(float3 lightDir, float sharpness)
{
    SphericalGaussian sg;
    sg.Axis = lightDir;
    sg.Sharpness = sharpness;
    sg.Amplitude = sg.Sharpness / ((2 * PI) * (1 - exp(-2 * sg.Sharpness)));
    return sg;
}

float SGIrradianceFitted(SphericalGaussian G, float3 N)
{
    const float muDotN = dot( G.Axis, N );

    const float c0 = 0.36;
    const float c1 = 0.25 / c0;

    float eml  = exp( -G.Sharpness );
    float em2l = eml * eml;
    float rl   = rcp( G.Sharpness );

    float scale = 1.0f + 2.0f * em2l - rl;
    float bias  = (eml - em2l) * rl - em2l;

    float x = sqrt( 1.0 - scale );
    float x0 = c0 * muDotN;
    float x1 = c1 * x;

    float n = x0 + x1;
    float y = ( abs( x0 ) <= x1 ) ? n*n / x : saturate( muDotN );

    return scale * y + bias;
}

SubsurfaceNormal CalculateSubsurfaceNormal(float3 normalFactor, float3 shadeNormal, float3 blurredNormal)
{
    SubsurfaceNormal subsurfaceNormal;
    subsurfaceNormal.NormalR = normalize(lerp(shadeNormal, blurredNormal, normalFactor.x));
    subsurfaceNormal.NormalG = normalize(lerp(shadeNormal, blurredNormal, normalFactor.y));
    subsurfaceNormal.NormalB = normalize(lerp(shadeNormal, blurredNormal, normalFactor.z));
    return subsurfaceNormal;
}

float3 SGGetSSS(float3 scatterColor, SubsurfaceNormal subsurfaceNormal, float3 blurredNormal, float3 L)
{
    SphericalGaussian kernelR = MakeNormalizedSG(L, rcp(max(scatterColor.r, EPS)));
    SphericalGaussian kernelG = MakeNormalizedSG(L, rcp(max(scatterColor.g, EPS)));
    SphericalGaussian kernelB = MakeNormalizedSG(L, rcp(max(scatterColor.b, EPS)));
    float3 diffuse = float3(SGIrradianceFitted(kernelR, subsurfaceNormal.NormalR),
                          SGIrradianceFitted(kernelG, subsurfaceNormal.NormalG),
                          SGIrradianceFitted(kernelB, subsurfaceNormal.NormalB));

    float NdotL = dot(blurredNormal, L);
    return diffuse * max(0, (NdotL + 0.5) / (1 + 0.5));;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1 - F0) * pow(saturate(1 - cosTheta), 5);
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

float3 Fd_Lambert()
{
    return 1.0 / PI;
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


float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1 - F0) * pow(saturate(1 - cosTheta), 5);
}

#define PBR_SKIN
float3 GetPBRSpecular(PS_INPUT input, float2 uv, float3 N, float3 L, float3 V, float3 lightColor, float glossiness, float shininess)
{
    float3 F0 = 0.028;

    float3 radiance = PI * lightColor;
	
    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float HdotV = saturate(dot(H, V));
    float NdotH = saturate(dot(N, H));
        
    float roughness = 1 - glossiness;
    roughness /= 1.8;

    float3 NDF1 = GetLightSpecularInput(input, L, V, N, lightColor, shininess, uv) * glossiness * MaterialData.yyy * SpecularColor.xyz;
   
    float NDF = DistributionGGX(saturate(NdotH), roughness);
    float G = GeometrySmith(saturate(NdotV), saturate(NdotL), roughness);
    float3 F = fresnelSchlick(saturate(HdotV), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4 * saturate(NdotV) * saturate(NdotL) + 0.0001;
    float3 specular = numerator / denominator;;
    
    // Total contribution for this light.
    return max(specular * NdotL * radiance, NDF1);
}

void GetLightRadianceSubsurface(PS_INPUT input, float2 uv, float3 N, float3 L, float3 V, float3 lightColor, float3 albedo, float ao, float roughness, float shininess, float3 specularMultiplier, float3 subsurface, SubsurfaceNormal ssN, float3 bN, inout float3 diffuseColor, inout float3 specularColor)
{
    float3 f0 = 0.04;

    float3 radiance = PI * lightColor;
	
    float3 H = normalize(V + L);
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    float HdotV = dot(H, V);
    float NdotH = dot(N, H);
        
    roughness /= 1.8;

   // float NDF = GetLightSpecularInput(input, L, V, N, lightColor, shininess, uv);
    float NDF = DistributionGGX(saturate(NdotH), roughness);
    float G = GeometrySmith(saturate(NdotV), saturate(NdotL), roughness);
    float3 F = FresnelSchlick(saturate(HdotV), f0);

    float metallic = 0;

    float3 kD = (1 - F) * (1 - metallic);

    float3 numerator = NDF * G * F;
    float denominator = 4 * saturate(NdotV) * saturate(NdotL) + 0.0001;
    float3 specular = numerator / denominator;

    diffuseColor +=kD * albedo * SGGetSSS(subsurface, ssN, bN, L) * Fd_Lambert() * radiance * ao;
    specularColor += GetPBRSpecular(input, uv, N, L, V, lightColor, 1 - roughness, shininess) * ao;
} 

//#endif
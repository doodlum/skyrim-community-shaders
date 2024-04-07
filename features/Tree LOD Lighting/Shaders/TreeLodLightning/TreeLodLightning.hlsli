cbuffer TreePerFrame : register(b3)
{
	row_major float3x4 DirectionalAmbient;
	float4 DirLightColor;
	float4 DirLightDirection;
	float DirLightScale;
	bool ComplexAtlasTexture;
	bool EnableComplexTreeLOD;
	bool EnableDirLightFix;
	float SubsurfaceScatteringAmount;
	float pad[3];
}

float GetSoftLightMultiplier(float angle, float strength)
{
	float softLightParam = saturate((strength + angle) / (1 + strength));
	float arg1 = (softLightParam * softLightParam) * (3 - 2 * softLightParam);
	float clampedAngle = saturate(angle);
	float arg2 = (clampedAngle * clampedAngle) * (3 - 2 * clampedAngle);
	float softLightMul = saturate(arg1 - arg2);
	return softLightMul;
}

float3 TransformNormal(float3 normal)
{
	return normal * 2 + -1.0.xxx;
}

// http://www.thetenthplanet.de/archives/1180
float3x3 CalculateTBN(float3 N, float3 p, float2 uv)
{
	// get edge vectors of the pixel triangle
	float3 dp1 = ddx_coarse(p);
	float3 dp2 = ddy_coarse(p);
	float2 duv1 = ddx_coarse(uv);
	float2 duv2 = ddy_coarse(uv);

	// solve the linear system
	float3 dp2perp = cross(dp2, N);
	float3 dp1perp = cross(N, dp1);
	float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = rsqrt(max(dot(T, T), dot(B, B)));
	return float3x3(T * invmax, B * invmax, N);
}

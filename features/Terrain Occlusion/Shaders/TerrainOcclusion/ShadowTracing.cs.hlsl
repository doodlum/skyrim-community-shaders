Texture2D<float2> TexHeight : register(t0);
RWTexture2D<float2> RWTexShadowHeights : register(u0);

cbuffer ShadowTracingCB : register(b0)
{
	float2 LightUVDir;   // direction on which light descends, from one pixel to next via dda
	float2 LightDeltaZ;  // per LightUVDir, upper penumbra and lower, should be negative
	float HeightBias;
	uint StartPxCoord;
	float2 PxSize;
}

float GetInterpolatedHeight(Texture2D tex, float2 pxCoord)
{
	int2 lerpPxCoordA = int2(threadPxCoord - .5);
	int2 lerpPxCoordB = int2(threadPxCoord + .5);
	float heightA = tex[lerpPxCoordA];
	float heightB = tex[lerpPxCoordB];

	bool inBoundA = all(lerpPxCoordA > 0);
	bool inBoundB = all(lerpPxCoordB < dims);
	if (inBoundA && inBoundB)
		return lerp(heightA, heightB, frac(threadPxCoord - .5));
	else if (!inBoundA)
		return heightB;
	else
		return heightA;
}

float GetInterpolatedHeight(RWTexture2D tex, float2 pxCoord)
{
	int2 lerpPxCoordA = int2(threadPxCoord - .5);
	int2 lerpPxCoordB = int2(threadPxCoord + .5);
	float heightA = tex[lerpPxCoordA];
	float heightB = tex[lerpPxCoordB];

	bool inBoundA = all(lerpPxCoordA > 0);
	bool inBoundB = all(lerpPxCoordB < dims);
	if (inBoundA && inBoundB)
		return lerp(heightA, heightB, frac(threadPxCoord - .5));
	else if (!inBoundA)
		return heightB;
	else
		return heightA;
}

groupshared float2 g_shadowHeight[1024];

[numthreads(1024, 1, 1)] void main(const uint gtid : SV_GroupThreadID, const uint gid : SV_GroupID) {
	uint2 dims;
	TexHeight.GetDimensions(dims.x, dims.y);

	bool isVertical = abs(LightUVDir.y) > abs(LightUVDir.x);

	uint2 rayStartPxCoord = isVertical ? uint2(StartPxCoord, gid) : uint2(gid, StartPxCoord);
	float2 rayStartUV = (rayStartPxCoord + .5) * PxSize;
	float2 threadUV = rayStartUV + gtid * LightUVDir;
	float2 threadPxCoord = threadUV * dims;

	// bifilter
	float2 heights = GetInterpolatedHeight(TexHeight, threadPxCoord).xx + HeightBias;

	// fetch last dispatch
	if (gtid == 0) {
		float2 lastDispatchUV = threadUV - LightUVDir;
		if (all(lastDispatchUV > 0) && all(lastDispatchUV < 1))
			heights = max(heights, GetInterpolatedHeight(RWTexShadowHeights, lastDispatchUV * dims) + HeightBias + LightDeltaZ);
	}

	g_shadowHeight[gtid] = heights;

	GroupMemoryBarrierWithGroupSync();

	// simple parallel scan
	[unroll] for (uint offset = 1; offset < 1024; offset <<= 1)
	{
		if (gtid >= offset)
			g_shadowHeight[gtid] = max(g_shadowHeight[gtid], g_shadowHeight[gtid - offset] + LightDeltaZ * offset);
		GroupMemoryBarrierWithGroupSync();
	}

	// save
	RWTexShadowHeights[uint2(threadPxCoord)] = g_shadowHeight[gtid];
}
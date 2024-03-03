Texture2D<float> TexHeight : register(t0);
RWTexture2D<float2> RWTexShadowHeights : register(u0);

cbuffer ShadowUpdateCB : register(b0)
{
	float2 LightPxDir : packoffset(c0.x);   // direction on which light descends, from one pixel to next via dda
	float2 LightDeltaZ : packoffset(c0.z);  // per lightUVDir, normalised, [upper, lower] penumbra, should be negative
	uint StartPxCoord : packoffset(c1.x);
	float2 PxSize : packoffset(c1.y);
}

float GetInterpolatedHeight(float2 pxCoord, bool isVertical)
{
	uint2 dims;
	TexHeight.GetDimensions(dims.x, dims.y);

	int2 lerpPxCoordA = int2(pxCoord - .5 * float2(isVertical, !isVertical));
	int2 lerpPxCoordB = int2(pxCoord + .5 * float2(isVertical, !isVertical));
	float heightA = TexHeight[lerpPxCoordA];
	float heightB = TexHeight[lerpPxCoordB];

	bool inBoundA = all(lerpPxCoordA > 0);
	bool inBoundB = all(lerpPxCoordB < dims);
	if (inBoundA && inBoundB)
		return lerp(heightA, heightB, frac(pxCoord - .5));
	else if (!inBoundA)
		return heightB;
	else
		return heightA;
}

float2 GetInterpolatedHeightRW(float2 pxCoord, bool isVertical)
{
	uint2 dims;
	RWTexShadowHeights.GetDimensions(dims.x, dims.y);

	int2 lerpPxCoordA = int2(pxCoord - .5 * float2(isVertical, !isVertical));
	int2 lerpPxCoordB = int2(pxCoord + .5 * float2(isVertical, !isVertical));
	float2 heightA = RWTexShadowHeights[lerpPxCoordA];
	float2 heightB = RWTexShadowHeights[lerpPxCoordB];

	bool inBoundA = all(lerpPxCoordA > 0);
	bool inBoundB = all(lerpPxCoordB < dims);
	if (inBoundA && inBoundB)
		return lerp(heightA, heightB, frac(pxCoord - .5));
	else if (!inBoundA)
		return heightB;
	else
		return heightA;
}

groupshared float2 g_shadowHeight[1024];

[numthreads(1024, 1, 1)] void main(const uint gtid : SV_GroupThreadID, const uint gid : SV_GroupID) {
	uint2 dims;
	TexHeight.GetDimensions(dims.x, dims.y);

	bool isVertical = abs(LightPxDir.y) > abs(LightPxDir.x);
	float2 lightUVDir = LightPxDir * PxSize;

	uint2 rayStartPxCoord = isVertical ? uint2(gid, StartPxCoord) : uint2(StartPxCoord, gid);
	float2 rayStartUV = (rayStartPxCoord + .5) * PxSize;
	float2 rawThreadUV = rayStartUV + gtid * lightUVDir;

	bool2 isUVinRange = (rawThreadUV > 0) && (rawThreadUV < 1);
	bool isValid = isVertical ? isUVinRange.y : isUVinRange.x;

	float2 threadUV = rawThreadUV - floor(rawThreadUV);  // wraparound
	float2 threadPxCoord = threadUV * dims;

	float2 pastHeights;
	if (isValid) {
		pastHeights = RWTexShadowHeights[uint2(threadPxCoord)];

		// bifilter
		float2 heights = GetInterpolatedHeight(threadPxCoord, isVertical).xx;

		// fetch last dispatch
		if (gtid == 0 && all(floor(rawThreadUV - lightUVDir) == floor(rawThreadUV))) {
			heights = max(heights, GetInterpolatedHeightRW(threadPxCoord - LightPxDir, isVertical) + LightDeltaZ);
		}

		g_shadowHeight[gtid] = heights;
	}

	GroupMemoryBarrierWithGroupSync();

	// simple parallel scan
	[unroll] for (uint offset = 1; offset < 1024; offset <<= 1)
	{
		if (isValid && gtid >= offset) {
			if (all(floor(rawThreadUV - lightUVDir * offset) == floor(rawThreadUV)))  // no wraparound happend
				g_shadowHeight[gtid] = max(g_shadowHeight[gtid], g_shadowHeight[gtid - offset] + LightDeltaZ * offset);
		}
		GroupMemoryBarrierWithGroupSync();
	}

	// save
	if (isValid) {
		RWTexShadowHeights[uint2(threadPxCoord)] = lerp(pastHeights, g_shadowHeight[gtid], .2f);
		// RWTexShadowHeights[uint2(threadPxCoord)] = gtid / 1024.f;
		// RWTexShadowHeights[uint2(gtid, gid)] = threadUV;
	}
}
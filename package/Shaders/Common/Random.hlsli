#ifndef CS_RANDOM
#define CS_RANDOM

///////////////////////////////////////////////////////////
// WHITE-LIKE HASHES
///////////////////////////////////////////////////////////

// https://www.shadertoy.com/view/XlGcRh

uint pcg(uint v)
{
	uint state = v * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

uint2 pcg2d(uint2 v)
{
	v = v * 1664525u + 1013904223u;

	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;

	v = v ^ (v >> 16u);

	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;

	v = v ^ (v >> 16u);

	return v;
}

uint3 pcg3d(uint3 v)
{
	v = v * 1664525u + 1013904223u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	v ^= v >> 16u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	return v;
}

uint iqint3(uint2 x)
{
	uint2 q = 1103515245U * ((x >> 1U) ^ (x.yx));
	uint n = 1103515245U * ((q.x) ^ (q.y >> 3U));

	return n;
}

///////////////////////////////////////////////////////////
// BLUE-LIKE HASHES / LOW DISCREPANCY SEQUENCES
///////////////////////////////////////////////////////////

// Derived from the interleaved gradient function from Jimenez 2014 http://goo.gl/eomGso
float InterleavedGradientNoise(float2 pxCoord, uint frameCount)
{
	// Temporal factor
	float frameStep = float(frameCount % 16) * 0.0625f;
	pxCoord.x += frameStep * 4.7526;
	pxCoord.y += frameStep * 3.1914;

	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(pxCoord, magic.xy)));
}

// https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
// https://www.shadertoy.com/view/mts3zN
float R1Sequence(float idx, float seed = 0)
{
	return frac(seed + idx * 0.61803398875);
}

float R1Modified(float idx, float seed = 0)
{
	return frac(seed + idx * 0.38196601125);
}

float2 R2Sequence(float idx, float2 seed = 0)
{
	return frac(seed + idx * float2(0.7548776662467, 0.569840290998));
}

float2 R2Modified(float idx, float2 seed = 0)
{
	return frac(seed + idx * float2(0.245122333753, 0.430159709002));
}

float3 R3Sequence(float idx, float3 seed = 0)
{
	return frac(seed + idx * float3(0.8191725133961, 0.6710436067038, 0.5497004779019));
}

float3 R3Modified(float idx, float3 seed = 0)
{
	return frac(seed + idx * float3(0.180827486604, 0.328956393296, 0.450299522098));
}

///////////////////////////////////////////////////////////
// NOISES
///////////////////////////////////////////////////////////

#endif
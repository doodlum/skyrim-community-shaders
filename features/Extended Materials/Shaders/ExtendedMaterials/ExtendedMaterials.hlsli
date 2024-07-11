// https://github.com/tgjones/slimshader-cpp/blob/master/src/Shaders/Sdk/Direct3D11/DetailTessellation11/POM.hlsl
// https://github.com/alandtse/SSEShaderTools/blob/main/shaders_vr/ParallaxEffect.h

// https://github.com/marselas/Zombie-Direct3D-Samples/blob/5f53dc2d6f7deb32eb2e5e438d6b6644430fe9ee/Direct3D/ParallaxOcclusionMapping/ParallaxOcclusionMapping.fx
// http://www.diva-portal.org/smash/get/diva2:831762/FULLTEXT01.pdf
// https://bartwronski.files.wordpress.com/2014/03/ac4_gdc.pdf

float GetMipLevel(float2 coords, Texture2D<float4> tex)
{
	// Compute the current gradients:
	float2 textureDims;
	tex.GetDimensions(textureDims.x, textureDims.y);

	float2 texCoordsPerSize = coords * textureDims;

	float2 dxSize = ddx(texCoordsPerSize);
	float2 dySize = ddy(texCoordsPerSize);

	// Find min of change in u and v across quad: compute du and dv magnitude across quad
	float2 dTexCoords = dxSize * dxSize + dySize * dySize;

	// Standard mipmapping uses max here
	float minTexCoordDelta = max(dTexCoords.x, dTexCoords.y);

	// Compute the current mip level  (* 0.5 is effectively computing a square root before )
	float mipLevel = max(0.5 * log2(minTexCoordDelta), 0);

#if !defined(PARALLAX)
	mipLevel++;
#endif

	return mipLevel;
}

#if defined(LANDSCAPE)

float GetTerrainHeight(PS_INPUT input, float2 coords, float mipLevels[6], out float pixelOffset[6])
{
	if (input.LandBlendWeights1.x > 0.0)
		pixelOffset[0] = TexColorSampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[0]).w;
	else pixelOffset[0] = 0;
	if (input.LandBlendWeights1.y > 0.0)
		pixelOffset[1] = TexLandColor2Sampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[1]).w;
	else pixelOffset[1] = 0;
	if (input.LandBlendWeights1.z > 0.0)
		pixelOffset[2] = TexLandColor3Sampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[2]).w;
	else pixelOffset[2] = 0;
	if (input.LandBlendWeights1.w > 0.0)
		pixelOffset[3] = TexLandColor4Sampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[3]).w;
	else pixelOffset[3] = 0;
	if (input.LandBlendWeights2.x > 0.0)
		pixelOffset[4] = TexLandColor5Sampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[4]).w;
	else pixelOffset[4] = 0;
	if (input.LandBlendWeights2.y > 0.0)
		pixelOffset[5] = TexLandColor6Sampler.SampleLevel(SampTerrainParallaxSampler, coords, mipLevels[5]).w;
	else pixelOffset[5] = 0;
	//float weights[6] = { input.LandBlendWeights1.x, input.LandBlendWeights1.y, input.LandBlendWeights1.z, input.LandBlendWeights1.w, input.LandBlendWeights2.x, input.LandBlendWeights2.y };
	//float maxWeight = max(weights[0], max(weights[1], max(weights[2], max(weights[3], max(weights[4], weights[5])))));
	//float maxOffset = max(pixelOffset[0], max(pixelOffset[1], max(pixelOffset[2], max(pixelOffset[3], max(pixelOffset[4], pixelOffset[5])))));
	//float blendFactor = saturate(1 - 0);
	//float total = 0;
	//for (int i = 0; i < 6; i++) {
	//	weights[i] = pow(weights[i] / maxWeight, 1 + 2 * blendFactor) * (pow(pixelOffset[i] + 1 - maxOffset, blendFactor * 100) + 0.1);
	//	total += weights[i];
	//}
	//
	//float height = (weights[0]+weights[1]+weights[2]+weights[3]+weights[4]+weights[5])/total;
	//return height;
	return max(max(pixelOffset[0], max(pixelOffset[1], max(pixelOffset[2], max(pixelOffset[3], max(pixelOffset[4], pixelOffset[5]))))), (pixelOffset[0] * input.LandBlendWeights1.x + pixelOffset[1] * input.LandBlendWeights1.y + pixelOffset[2] * input.LandBlendWeights1.z + pixelOffset[3] * input.LandBlendWeights1.w + pixelOffset[4] * input.LandBlendWeights2.x + pixelOffset[5] * input.LandBlendWeights2.y));
	return  max(pixelOffset[0] * input.LandBlendWeights1.x, max(pixelOffset[1] * input.LandBlendWeights1.y, max(pixelOffset[2] * input.LandBlendWeights1.z, max(pixelOffset[3] * input.LandBlendWeights1.w, max(pixelOffset[4] * input.LandBlendWeights2.x,pixelOffset[5] * input.LandBlendWeights2.y)))));
	return (pixelOffset[0] * input.LandBlendWeights1.x + pixelOffset[1] * input.LandBlendWeights1.y + pixelOffset[2] * input.LandBlendWeights1.z + pixelOffset[3] * input.LandBlendWeights1.w + pixelOffset[4] * input.LandBlendWeights2.x + pixelOffset[5] * input.LandBlendWeights2.y);
}
#endif

#if defined(LANDSCAPE)
float2 GetParallaxCoords(PS_INPUT input, float distance, float2 coords, float mipLevels[6], float3 viewDir, float3x3 tbn, float noise, out float pixelOffset, out float heights[6])
#else
float2 GetParallaxCoords(float distance, float2 coords, float mipLevel, float3 viewDir, float3x3 tbn, float noise, Texture2D<float4> tex, SamplerState texSampler, uint channel, out float pixelOffset)
#endif
{
	float3 viewDirTS = normalize(mul(tbn, viewDir));
	viewDirTS.xy /= viewDirTS.z * 0.7 + 0.3;  // Fix for objects at extreme viewing angles

	float nearBlendToFar = saturate(distance / 2048.0);

	float maxHeight = 0.1;
	float minHeight = maxHeight * 0.5;

	if (nearBlendToFar < 1.0) {
		uint numSteps = uint((32 * (1.0 - nearBlendToFar)) + 0.5);
		numSteps = clamp((numSteps + 3) & ~0x03, 4, 32);

		float stepSize = rcp(numSteps);

		float2 offsetPerStep = viewDirTS.xy * float2(maxHeight, maxHeight) * stepSize.xx;
		float2 prevOffset = viewDirTS.xy * float2(minHeight, minHeight) + coords.xy;

		float prevBound = 1.0;
		float prevHeight = 1.0;

		float2 pt1 = 0;
		float2 pt2 = 0;

		[loop] while (numSteps > 0)
		{
			float4 currentOffset[2];
			currentOffset[0] = prevOffset.xyxy - float4(1, 1, 2, 2) * offsetPerStep.xyxy;
			currentOffset[1] = prevOffset.xyxy - float4(3, 3, 4, 4) * offsetPerStep.xyxy;
			float4 currentBound = prevBound.xxxx - float4(1, 2, 3, 4) * stepSize;

#if defined(LANDSCAPE)
			float4 currHeight;
			currHeight.x = GetTerrainHeight(input, currentOffset[0].xy, mipLevels, heights);
			currHeight.y = GetTerrainHeight(input, currentOffset[0].zw, mipLevels, heights);
			currHeight.z = GetTerrainHeight(input, currentOffset[1].xy, mipLevels, heights);
			currHeight.w = GetTerrainHeight(input, currentOffset[1].zw, mipLevels, heights);
#else
			float4 currHeight;
			currHeight.x = tex.SampleLevel(texSampler, currentOffset[0].xy, mipLevel)[channel];
			currHeight.y = tex.SampleLevel(texSampler, currentOffset[0].zw, mipLevel)[channel];
			currHeight.z = tex.SampleLevel(texSampler, currentOffset[1].xy, mipLevel)[channel];
			currHeight.w = tex.SampleLevel(texSampler, currentOffset[1].zw, mipLevel)[channel];
#endif

			bool4 testResult = currHeight >= currentBound;
			[branch] if (any(testResult))
			{
				[flatten] if (testResult.w)
				{
					pt1 = float2(currentBound.w, currHeight.w);
					pt2 = float2(currentBound.z, currHeight.z);
				}
				[flatten] if (testResult.z)
				{
					pt1 = float2(currentBound.z, currHeight.z);
					pt2 = float2(currentBound.y, currHeight.y);
				}
				[flatten] if (testResult.y)
				{
					pt1 = float2(currentBound.y, currHeight.y);
					pt2 = float2(currentBound.x, currHeight.x);
				}
				[flatten] if (testResult.x)
				{
					pt1 = float2(currentBound.x, currHeight.x);
					pt2 = float2(prevBound, prevHeight);
				}
				break;
			}

			prevOffset = currentOffset[1].zw;
			prevBound = currentBound.w;
			prevHeight = currHeight.w;
			numSteps -= 4;
		}

		float delta2 = pt2.x - pt2.y;
		float delta1 = pt1.x - pt1.y;
		float denominator = delta2 - delta1;

		float parallaxAmount = 0.0;
		[flatten] if (denominator == 0.0)
		{
			parallaxAmount = 0.0;
		}
		else
		{
			parallaxAmount = (pt1.x * delta2 - pt2.x * delta1) / denominator;
		}


#if defined(LANDSCAPE)
		float weights[6] = { input.LandBlendWeights1.x, input.LandBlendWeights1.y, input.LandBlendWeights1.z, input.LandBlendWeights1.w, input.LandBlendWeights2.x, input.LandBlendWeights2.y };
		float maxOffset = max(heights[0], max(heights[1], max(heights[2], max(heights[3], max(heights[4], heights[5])))));
		float maxWeight = max(weights[0], max(weights[1], max(weights[2], max(weights[3], max(weights[4], weights[5])))));
		float blendFactor = saturate(1 - nearBlendToFar);
		float total = 0;
		for (int i = 0; i < 6; i++) {
			heights[i] = pow(weights[i], 1 + 2 * blendFactor) * (pow(heights[i] + 0.5, blendFactor * 100) + 0.1);
			total += heights[i];
		}
		for (int i = 0; i < 6; i++) {
			heights[i] /= total;
		}
#endif

		nearBlendToFar *= nearBlendToFar;

		float offset = (1.0 - parallaxAmount) * -maxHeight + minHeight;
		pixelOffset = lerp(parallaxAmount, 0.5, nearBlendToFar);
		return lerp(viewDirTS.xy * offset + coords.xy, coords, nearBlendToFar);
	}

#if defined(LANDSCAPE)
	heights[0] = input.LandBlendWeights1.x;
	heights[1] = input.LandBlendWeights1.y;
	heights[2] = input.LandBlendWeights1.z;
	heights[3] = input.LandBlendWeights1.w;
	heights[4] = input.LandBlendWeights2.x;
	heights[5] = input.LandBlendWeights2.y;
	float maxOffset = max(heights[0], max(heights[1], max(heights[2], max(heights[3], max(heights[4], heights[5])))));
	float total = 0;
	float blendFactor = 0;
	for (int i = 0; i < 6; i++) {
		heights[i] = pow(heights[i] / maxOffset, 1) * (1 + 0.1);
		total += heights[i];
	}
	for (int i = 0; i < 6; i++) {
		heights[i] /= total;
	}
#endif

	pixelOffset = 0.5;
	return coords;
}

// https://advances.realtimerendering.com/s2006/Tatarchuk-POM.pdf

// Cheap method of creating shadows using height for a given light source
float GetParallaxSoftShadowMultiplier(float2 coords, float mipLevel, float3 L, float sh0, Texture2D<float4> tex, SamplerState texSampler, uint channel, float quality, float noise)
{
	[branch] if (quality > 0.0)
	{
		float2 rayDir = L.xy * 0.1;
		float4 multipliers = rcp((float4(4, 3, 2, 1) + noise));
		float4 sh;
		sh.x = tex.SampleLevel(texSampler, coords + rayDir * multipliers.x, mipLevel)[channel];
		sh.y = tex.SampleLevel(texSampler, coords + rayDir * multipliers.y, mipLevel)[channel];
		sh.z = tex.SampleLevel(texSampler, coords + rayDir * multipliers.z, mipLevel)[channel];
		sh.w = tex.SampleLevel(texSampler, coords + rayDir * multipliers.w, mipLevel)[channel];
		return 1.0 - saturate(dot(max(0, sh - sh0), 1.0) * 2.0) * quality;
	}
	return 1.0;
}

#if defined(LANDSCAPE)
float GetParallaxSoftShadowMultiplierTerrain(PS_INPUT input, float2 coords, float mipLevel[6], float3 L, float sh0, float quality, float noise)
{
	if (quality > 0.0) {
		float2 rayDir = L.xy * 0.1;
		float4 multipliers = rcp((float4(4, 3, 2, 1) + noise));
		float4 sh;
		float heights[6] = { 0, 0, 0, 0, 0, 0 };
		sh.x = GetTerrainHeight(input, coords + rayDir * multipliers.x, mipLevel, heights);
		sh.y = GetTerrainHeight(input, coords + rayDir * multipliers.y, mipLevel, heights);
		sh.z = GetTerrainHeight(input, coords + rayDir * multipliers.z, mipLevel, heights);
		sh.w = GetTerrainHeight(input, coords + rayDir * multipliers.w, mipLevel, heights);
		return 1.0 - saturate(dot(max(0, sh - sh0), 1.0) * 2.0) * quality;
	}
	return 1.0;
}
#endif

RWTexture2DArray<uint> AccumulationDataRed : register(u0);
RWTexture2DArray<uint> AccumulationDataGreen : register(u1);
RWTexture2DArray<uint> AccumulationDataBlue : register(u2);
RWTexture2DArray<uint> AccumulationDataCounter : register(u3);

Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> ColorTexture : register(t1);

cbuffer PerFrame : register(b0)
{
	float2 RcpBufferDim;
	uint CubeMapSize;
	float4x4 InvViewProjMatrix;
};

// Inverse project UV + raw depth into the world space.
float3 InverseProjectUVZW(float2 uv, float z)
{
	uv.y = 1 - uv.y;
	float4 cp = float4(float3(uv, z) * 2 - 1, 1);
	float4 vp = mul(InvViewProjMatrix, cp);
	return float3(vp.xy, vp.z) / vp.w;
}

// Helper function to calculate cubemap face and UV coordinates from a view direction
uint3 GetThreadID(float3 samplingVector)
{
    // Calculate the vector's normalized version.
    float3 normalizedVector = normalize(samplingVector);

    // Determine the cubemap face index based on the vector direction.
    uint faceIndex;
    if (normalizedVector.x >= abs(normalizedVector.y) && normalizedVector.x >= abs(normalizedVector.z))
    {
        // Face along the positive x-axis.
        faceIndex = 0;
    }
    else if (normalizedVector.y >= abs(normalizedVector.x) && normalizedVector.y >= abs(normalizedVector.z))
    {
        // Face along the positive y-axis.
        faceIndex = 2;
    }
    else
    {
        // Face along the positive z-axis.
        faceIndex = 4;
    }

    // Calculate the st coordinates from the normalized vector.
    float st_x, st_y;
    if (faceIndex == 0)
    {
        st_x = normalizedVector.z / normalizedVector.x;
        st_y = 1.0 - normalizedVector.y / normalizedVector.x;
    }
    else if (faceIndex == 2)
    {
        st_x = normalizedVector.x / normalizedVector.y;
        st_y = 1.0 - normalizedVector.z / normalizedVector.y;
    }
    else
    {
        st_x = normalizedVector.x / normalizedVector.z;
        st_y = 1.0 - normalizedVector.y / normalizedVector.z;
    }

    // Convert st coordinates to ThreadID.xy
    uint2 ThreadID = uint2(st_x * 512, (1.0 - st_y) * 512);

    // Set the z component to the determined face index.
    ThreadID.z = faceIndex;

    return ThreadID;
}

// Helper function to calculate cubemap face and UV coordinates from a view direction
uint3 ToCubemapID(float3 viewDirection)
{
    float3 absViewDir = abs(viewDirection);
    float ma;
    uint face;
    
    if (absViewDir.x >= absViewDir.y && absViewDir.x >= absViewDir.z)
    {
        ma = 0.5 / absViewDir.x;
        if (viewDirection.x < 0.0)
            face = 0; // Right
        else
            face = 1; // Left
    }
    else if (absViewDir.y >= absViewDir.z)
    {
        ma = 0.5 / absViewDir.y;
        if (viewDirection.y < 0.0)
            face = 2; // Up
        else
            face = 3; // Down
    }
    else
    {
        ma = 0.5 / absViewDir.z;
        if (viewDirection.z < 0.0)
            face = 4; // Front
        else
            face = 5; // Back
    }
    
    float2 uv = (viewDirection.xy * ma + 0.5);
    
    return uint3(uv.x * CubeMapSize, uv.y * CubeMapSize, face);
}

// Function to pack a float into a uint while preserving decimal data
uint PackFloatWithDecimalToUInt(float value, float scaleFactor)
{
    return (value * scaleFactor);
}

[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{	
	float3 color = ColorTexture[DTid.xy];

	float depth = DepthTexture[DTid.xy];
	float2 uv = (DTid.xy + 0.5) * RcpBufferDim;

	float3 worldSpacePosition = InverseProjectUVZW(uv, depth);
	float3 viewDirection = -normalize(worldSpacePosition);
	uint3 texID = ToCubemapID(viewDirection);
	uint3 packedColor = uint3(
        PackFloatWithDecimalToUInt(color.x, 10000),
        PackFloatWithDecimalToUInt(color.y, 10000),
        PackFloatWithDecimalToUInt(color.z, 10000)
    );

	InterlockedAdd(AccumulationDataRed[texID], packedColor.x);
    InterlockedAdd(AccumulationDataGreen[texID], packedColor.y);
	InterlockedAdd(AccumulationDataBlue[texID], packedColor.z);
	InterlockedAdd(AccumulationDataCounter[texID], 1u);
}

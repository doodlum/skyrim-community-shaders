RWTexture2DArray<uint> AccumulationDataRed : register(u0);
RWTexture2DArray<uint> AccumulationDataGreen : register(u1);
RWTexture2DArray<uint> AccumulationDataBlue : register(u2);
RWTexture2DArray<uint> AccumulationDataCounter : register(u3);

RWTexture2DArray<float4> DynamicCubemap : register(u4);

Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> ColorTexture : register(t1);

// Function to unpack a uint back into a float
float UnpackUIntWithDecimalToFloat(uint packedValue, float scaleFactor)
{
    return float(packedValue) / scaleFactor;
}

[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{	
    uint counter = AccumulationDataCounter[DTid.xyz];
    if (counter)
    {
        float invSamples = 1.0 / counter;
        float3 color = float3(
            UnpackUIntWithDecimalToFloat(AccumulationDataRed[DTid.xyz], 10000) * invSamples,
            UnpackUIntWithDecimalToFloat(AccumulationDataGreen[DTid.xyz], 10000) * invSamples,
            UnpackUIntWithDecimalToFloat(AccumulationDataBlue[DTid.xyz], 10000) * invSamples
        );
        color.rgb = float3(1.0, 0, 0);
        DynamicCubemap[DTid.xyz] = float4(color, 1.0);
        
        AccumulationDataRed[DTid.xyz] = 0;
        AccumulationDataGreen[DTid.xyz] = 0;
        AccumulationDataBlue[DTid.xyz] = 0;
        AccumulationDataCounter[DTid.xyz] = 0;
    }
}

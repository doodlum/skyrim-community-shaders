RWTexture2DArray<float4> OutputTexture : register(u0);

[numthreads(32, 32, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{	
    OutputTexture[ThreadID] = float4(0, 0, 0, 0);   
}

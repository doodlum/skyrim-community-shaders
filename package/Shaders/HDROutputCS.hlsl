
#include "Common/Color.hlsli"
#include "Common/DICETonemapper.hlsli"

Texture2D<float4> UI : register(t0);

RWTexture2D<float4> FrameBuffer : register(u0);

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	float3 framebuffer = FrameBuffer[dispatchID.xy];

	framebuffer = sign(framebuffer) * Color::GammaToLinear(abs(framebuffer));
	framebuffer *= 203.0 / 80.0;

	FrameBuffer[dispatchID.xy] = float4(framebuffer, 1.0);
};
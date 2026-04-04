// Convert R11G11B10_FLOAT scene color to R8G8B8A8_UNORM shared buffer
// The GPU handles format conversion automatically via typed SRV/UAV reads/writes
Texture2D<float3> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(8, 8, 1)] void main(uint3 DTid
								: SV_DispatchThreadID) {
	float3 color = InputTexture[DTid.xy];
	OutputTexture[DTid.xy] = float4(color, 1.0);
}

// Bilinear upscale pixel shader — samples render-res source with hardware filtering.
// The sampler stretches the render-res texture to fill the display-res render target.

Texture2D<float4> ColorInput : register(t0);
SamplerState LinearSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_Target
{
    return ColorInput.SampleLevel(LinearSampler, texcoord, 0);
}

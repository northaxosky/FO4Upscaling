// Fullscreen triangle vertex shader — generates a screen-covering triangle from SV_VertexID.
// Draw(3, 0) with no vertex buffer. Vertex 0=(0,0), Vertex 1=(0,2), Vertex 2=(2,0).

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 uv;
    uv.x = (vertexID == 2) ? 2.0 : 0.0;
    uv.y = (vertexID == 1) ? 2.0 : 0.0;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.5, 1.0);
    output.texcoord = uv;
    return output;
}


struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VS_OUTPUT D3D11BlurVertexShader_Main(uint id : SV_VertexID)
{
    VS_OUTPUT output;
    output.texcoord = float2((id << 1) & 2, id & 2);
    output.position = float4(output.texcoord * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

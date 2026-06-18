
cbuffer BlurConstants : register(b0)
{
    float2 dir;
    float2 resolution;
    float radius;
    float3 padding;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

float4 D3D11BlurPixelShader_Main(PS_INPUT input) : SV_Target
{
    float4 sum = float4(0, 0, 0, 0);
    float step = radius / 7.0;
    float sigma = max(radius * 0.5, 1.0);
    float twoSigma2 = 2.0 * sigma * sigma;
    float totalWeight = 0.0;

    [unroll]
    for (int i = -7; i <= 7; i++)
    {
        float off = float(i) * step;
        float w = exp(-(off * off) / twoSigma2);
        float2 tc = input.texcoord + dir * off / resolution;
        sum += tex.Sample(samp, tc) * w;
        totalWeight += w;
    }

    return sum / totalWeight;
}

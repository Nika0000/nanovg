Texture2D    g_texture                    : register(t0);
SamplerState g_sampler                 : register(s0);

struct PS_INPUT
{
    float4 position   : SV_Position;    // vertex position
    float2 ftcoord    : TEXCOORD0;      // float 2 tex coord
    float2 fpos       : TEXCOORD1;      // float 2 position
    float2 uv         : TEXCOORD2;      // normalized line width / path length
};

cbuffer PS_CONSTANTS
{
    float4x4 scissorMat;
    float4 scissorExt;
    float4 scissorScale;
    float4x4 paintMat;
    float4 extent;
    float4 radius;
    float4 feather;
    float4 innerCol;
    float4 outerCol;
    float4 strokeMult;
    int texType;
    int type;
    int lineStyle;
};


float sdroundrect(float2 pt, float2 ext, float rad)
{
    float2 ext2 = ext - float2(rad,rad);
    float2 d = abs(pt) - ext2;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;
}

// Scissoring
float scissorMask(float2 p)
{
    float2 sc = (abs((mul((float3x3)scissorMat, float3(p.x, p.y, 1.0))).xy) - scissorExt.xy);
    sc = float2(0.5,0.5) - sc * scissorScale.xy;
    return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);
}

float glow(float2 uv)
{
    return smoothstep(0.0, 1.0, 1.0 - 2.0 * abs(uv.x));
}

float dashed(float2 uv)
{
    float fy = frac(uv.y / 4.0);
    float w = step(fy, 0.5);
    fy *= 4.0;
    if (fy >= 1.5)
    {
        fy -= 1.5;
    }
    else if (fy <= 0.5)
    {
        fy = 0.5 - fy;
    }
    else
    {
        fy = 0.0;
    }
    w *= smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
    return w;
}

float dotted(float2 uv)
{
    float fy = 4.0 * frac(uv.y / 4.0) - 0.5;
    return smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
}

#ifdef EDGE_AA
// Stroke - from [0..1] to clipped pyramid, where the slope is 1px.
float strokeMask(float2 ftcoord)
{
    return min(1.0, (1.0 - abs(ftcoord.x*2.0 - 1.0))*strokeMult.x) * min(1.0f, ftcoord.y);
}
#endif

#ifdef EDGE_AA
float4 D3D11PixelShaderAA_Main(PS_INPUT input) : SV_TARGET
#else
float4 D3D11PixelShader_Main(PS_INPUT input) : SV_TARGET
#endif
{
    float4 result;
    float scissor = scissorMask(input.fpos);
#ifdef EDGE_AA
    float strokeAlpha = strokeMask(input.ftcoord);
    if (lineStyle == 2) strokeAlpha *= dashed(input.uv);
    if (lineStyle == 3) strokeAlpha *= dotted(input.uv);
    if (lineStyle == 4) strokeAlpha *= glow(input.uv);
    if (strokeAlpha < strokeMult.y)
        discard;
#else
    float strokeAlpha = 1.0f;
    if (lineStyle == 2) strokeAlpha *= dashed(input.uv);
    if (lineStyle == 3) strokeAlpha *= dotted(input.uv);
    if (lineStyle == 4) strokeAlpha *= glow(input.uv);
    if (lineStyle > 1 && strokeAlpha < strokeMult.y)
        discard;
#endif
    if (type == 0)
    {
        // Calculate gradient color using box gradient
        float2 pt = (mul((float3x3)paintMat, float3(input.fpos,1.0))).xy;
        float d = clamp((sdroundrect(pt, extent.xy, radius.x) + feather.x*0.5) / feather.x, 0.0, 1.0);
        float4 color = lerp(innerCol, outerCol, d);

        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    }
    else if (type == 1)
    {
        // Calculate color fron texture
        float2 pt = (mul((float3x3)paintMat, float3(input.fpos,1.0))).xy / extent.xy;
        float4 color = g_texture.Sample(g_sampler, pt);
        if (texType == 1) color = float4(color.xyz*color.w,color.w);
		if (texType == 2) color = float4(color.x, color.x, color.x, color.x);
        if (texType == 3 && color.w == 1.0) discard;
		
        // Apply color tint and alpha.
        color *= innerCol;
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    }
    else if (type == 2)
    {
        // Stencil fill
        result = float4(1,1,1,1);
    }
    else
    {
        // Textured tris
        float4 color = g_texture.Sample(g_sampler, input.ftcoord);
        if (texType == 1) color = float4(color.xyz*color.w,color.w);
		if (texType == 2) color = float4(color.x, color.x, color.x, color.x);
		color *= scissor;
        result = (color * innerCol);
    }
    return result;
}


// Copyright (c) 2017 Ollix
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// ---
// Author: olliwang@ollix.com (Olli Wang)

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct {
  float2 pos [[attribute(0)]];
  float2 tcoord [[attribute(1)]];
  float2 lcoord [[attribute(2)]];
} Vertex;

typedef struct {
  float4 pos  [[position]];
  float2 fpos;
  float2 ftcoord;
  float2 uv;
} RasterizerData;

typedef struct  {
  float3x3 scissorMat;
  float3x3 paintMat;
  float4 innerCol;
  float4 outerCol;
  float2 scissorExt;
  float2 scissorScale;
  float2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int lineStyle;
  int texType;
  int type;
} Uniforms;

float scissorMask(constant Uniforms& uniforms, float2 p);
float sdroundrect(constant Uniforms& uniforms, float2 pt);
float strokeMask(constant Uniforms& uniforms, float2 ftcoord);
float glow(float2 uv);
float dashed(float2 uv);
float dotted(float2 uv);
float lineStyleMask(constant Uniforms& uniforms, float2 uv);

float scissorMask(constant Uniforms& uniforms, float2 p) {
  float2 sc = (abs((uniforms.scissorMat * float3(p, 1.0f)).xy)
                  - uniforms.scissorExt) \
              * uniforms.scissorScale;
  sc = saturate(float2(0.5f) - sc);
  return sc.x * sc.y;
}

float sdroundrect(constant Uniforms& uniforms, float2 pt) {
  float2 ext2 = uniforms.extent - float2(uniforms.radius);
  float2 d = abs(pt) - ext2;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - uniforms.radius;
}

float strokeMask(constant Uniforms& uniforms, float2 ftcoord) {
  return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * uniforms.strokeMult) \
         * min(1.0, ftcoord.y);
}

float glow(float2 uv) {
  return smoothstep(0.0, 1.0, 1.0 - 2.0 * abs(uv.x));
}

float dashed(float2 uv) {
  float fy = fract(uv.y / 4.0);
  float w = step(fy, 0.5);
  fy *= 4.0;
  if (fy >= 1.5) {
    fy -= 1.5;
  } else if (fy <= 0.5) {
    fy = 0.5 - fy;
  } else {
    fy = 0.0;
  }
  w *= smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
  return w;
}

float dotted(float2 uv) {
  float fy = 4.0 * fract(uv.y / 4.0) - 0.5;
  return smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
}

float lineStyleMask(constant Uniforms& uniforms, float2 uv) {
  if (uniforms.lineStyle == 2)
    return dashed(uv);
  if (uniforms.lineStyle == 3)
    return dotted(uv);
  if (uniforms.lineStyle == 4)
    return glow(uv);
  return 1.0;
}

// Vertex Function
vertex RasterizerData vertexShader(Vertex vert [[stage_in]],
                                   constant float2& viewSize [[buffer(1)]]) {
  RasterizerData out;
  out.ftcoord = vert.tcoord;
  out.uv = 0.5 * vert.lcoord;
  out.fpos = vert.pos;
  out.pos = float4(2.0 * vert.pos.x / viewSize.x - 1.0,
                   1.0 - 2.0 * vert.pos.y / viewSize.y,
                   0, 1);
  return out;
}

// Fragment function (No AA)
fragment float4 fragmentShader(RasterizerData in [[stage_in]],
                               constant Uniforms& uniforms [[buffer(0)]],
                               texture2d<float> texture [[texture(0)]],
                               sampler sampler [[sampler(0)]]) {
  float scissor = scissorMask(uniforms, in.fpos);
  if (scissor == 0) {
    discard_fragment();
    return float4(0);
  }

  float strokeAlpha = lineStyleMask(uniforms, in.uv);
  if (uniforms.lineStyle > 1 && strokeAlpha < uniforms.strokeThr) {
    discard_fragment();
    return float4(0);
  }

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy;
    float d = saturate((uniforms.feather * 0.5 + sdroundrect(uniforms, pt))
                       / uniforms.feather);
    float4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    return color * strokeAlpha * scissor;
  } else if (uniforms.type == 1) {  // MNVG_SHADER_FILLIMG
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy / uniforms.extent;
    float4 color = texture.sample(sampler, pt);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= strokeAlpha * scissor;
    return color * uniforms.innerCol;
  } else {  // MNVG_SHADER_IMG
    float4 color = texture.sample(sampler, in.ftcoord);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }
}

// Fragment function (AA)
fragment float4 fragmentShaderAA(RasterizerData in [[stage_in]],
                                 constant Uniforms& uniforms [[buffer(0)]],
                                 texture2d<float> texture [[texture(0)]],
                                 sampler sampler [[sampler(0)]]) {
  float scissor = scissorMask(uniforms, in.fpos);
  if (scissor == 0) {
    discard_fragment();
    return float4(0);
  }

  if (uniforms.type == 2) {  // MNVG_SHADER_IMG
    float4 color = texture.sample(sampler, in.ftcoord);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }

  float strokeAlpha = strokeMask(uniforms, in.ftcoord)
                      * lineStyleMask(uniforms, in.uv);
  if (strokeAlpha < uniforms.strokeThr) {
    discard_fragment();
    return float4(0);
  }

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy;
    float d = saturate((uniforms.feather * 0.5 + sdroundrect(uniforms, pt))
                        / uniforms.feather);
    float4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    color *= scissor;
    color *= strokeAlpha;
    return color;
  } else {  // MNVG_SHADER_FILLIMG
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy / uniforms.extent;
    float4 color = texture.sample(sampler, pt);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    color *= strokeAlpha;
    return color * uniforms.innerCol;
  }
}

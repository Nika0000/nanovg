#version 460

layout(binding = 0) uniform sampler2D tex;

layout(std140, binding = 0) uniform frag {
    mat3 scissorMat;
    mat3 paintMat;
    vec4 innerCol;
    vec4 outerCol;
    vec2 scissorExt;
    vec2 scissorScale;
    vec2 extent;
    float radius;
    float feather;
    float strokeMult;
    float strokeThr;
    int lineStyle;
    int texType;
    int type;
};

layout(location = 0) in vec2 ftcoord;
layout(location = 1) in vec2 fpos;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec4 outColor;

float sdroundrect(vec2 pt, vec2 ext, float rad) {
    vec2 ext2 = ext - vec2(rad,rad);
    vec2 d = abs(pt) - ext2;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;
}

// Scissoring
float scissorMask(vec2 p) {
    vec2 sc = (abs((scissorMat * vec3(p,1.0)).xy) - scissorExt);
    sc = vec2(0.5,0.5) - sc * scissorScale;
    return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);
}

float glow(vec2 uv) {
    return smoothstep(0.0, 1.0, 1.0 - 2.0 * abs(uv.x));
}

float dashed(vec2 uv) {
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

float dotted(vec2 uv) {
    float fy = 4.0 * fract(uv.y / 4.0) - 0.5;
    return smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
}

// Line style (dashed/dotted/glow) mask.
float lineStyleMask(vec2 uv) {
    if (lineStyle == 2) return dashed(uv);
    if (lineStyle == 3) return dotted(uv);
    if (lineStyle == 4) return glow(uv);
    return 1.0;
}

void main(void) {
    vec4 result;
    float scissor = scissorMask(fpos);
    float strokeAlpha = lineStyleMask(uv);

    if (lineStyle > 1 && strokeAlpha < strokeThr) discard;

    if (type == 0) {			// Gradient
        // Calculate gradient color using box gradient
        vec2 pt = (paintMat * vec3(fpos,1.0)).xy;
        float d = clamp((sdroundrect(pt, extent, radius) + feather*0.5) / feather, 0.0, 1.0);
        vec4 color = mix(innerCol,outerCol,d);
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    } else if (type == 1) {		// Image
        // Calculate color fron texture
        vec2 pt = (paintMat * vec3(fpos,1.0)).xy / extent;
        vec4 color = texture(tex, pt);

        if (texType == 1) color = vec4(color.xyz*color.w,color.w);
        if (texType == 2) color = vec4(color.x);
        // Apply color tint and alpha.
        color *= innerCol;
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    } else if (type == 2) {		// Stencil fill
        result = vec4(1,1,1,1);
    } else if (type == 3) {		// Textured tris

        vec4 color = texture(tex, ftcoord);

        if (texType == 1) color = vec4(color.xyz*color.w,color.w);
        if (texType == 2) color = vec4(color.x);
        color *= scissor;
        result = color * innerCol;
    }

    outColor = result;
};
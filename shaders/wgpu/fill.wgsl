// EDGE_AA is prepended as a `const EDGE_AA: u32 = 0u;` or `1u;` line before this
// source at shader-module-creation time (see wgnvg_renderCreate).

struct VertexUniform {
    viewSize: vec2<f32>,
};

struct FragmentData {
    scissorMat: mat3x4<f32>,
    paintMat: mat3x4<f32>,
    innerCol: vec4<f32>,
    outerCol: vec4<f32>,
    scissorExt: vec2<f32>,
    scissorScale: vec2<f32>,
    extent: vec2<f32>,
    radius: f32,
    feather: f32,
    strokeMult: f32,
    strokeThr: f32,
    lineStyle: i32,
    texType: i32,
    type_: i32,
};

@group(0) @binding(0) var<storage, read> frag: FragmentData;
@group(0) @binding(1) var<uniform> vertUniform: VertexUniform;
@group(1) @binding(0) var tex: texture_2d<f32>;
@group(1) @binding(1) var texSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) ftcoord: vec2<f32>,
    @location(1) fpos: vec2<f32>,
    @location(2) uv: vec2<f32>,
};

@vertex
fn vs_main(
    @location(0) vertex: vec2<f32>,
    @location(1) tcoord: vec2<f32>,
    @location(2) lcoord: vec2<f32>,
) -> VertexOutput {
    var out: VertexOutput;
    out.ftcoord = tcoord;
    out.fpos = vertex;
    out.uv = 0.5 * lcoord;
    // WebGPU NDC has +Y up, unlike Vulkan's +Y down - the y term is flipped
    // relative to shaders/vulkan/fill.vert.
    out.position = vec4<f32>(
        2.0 * vertex.x / vertUniform.viewSize.x - 1.0,
        1.0 - 2.0 * vertex.y / vertUniform.viewSize.y,
        0.0,
        1.0,
    );
    return out;
}

fn xformPt(m: mat3x4<f32>, p: vec2<f32>) -> vec2<f32> {
    let r = m[0].xyz * p.x + m[1].xyz * p.y + m[2].xyz;
    return r.xy;
}

fn sdroundrect(pt: vec2<f32>, ext: vec2<f32>, rad: f32) -> f32 {
    let ext2 = ext - vec2<f32>(rad, rad);
    let d = abs(pt) - ext2;
    return min(max(d.x, d.y), 0.0) + length(max(d, vec2<f32>(0.0, 0.0))) - rad;
}

fn scissorMask(p: vec2<f32>) -> f32 {
    var sc = abs(xformPt(frag.scissorMat, p)) - frag.scissorExt;
    sc = vec2<f32>(0.5, 0.5) - sc * frag.scissorScale;
    return clamp(sc.x, 0.0, 1.0) * clamp(sc.y, 0.0, 1.0);
}

fn strokeMask(ftcoord: vec2<f32>) -> f32 {
    return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * frag.strokeMult) * min(1.0, ftcoord.y);
}

fn glow(uv: vec2<f32>) -> f32 {
    return smoothstep(0.0, 1.0, 1.0 - 2.0 * abs(uv.x));
}

fn dashed(uv: vec2<f32>) -> f32 {
    var fy = fract(uv.y / 4.0);
    let w0 = step(fy, 0.5);
    fy *= 4.0;
    if (fy >= 1.5) {
        fy -= 1.5;
    } else if (fy <= 0.5) {
        fy = 0.5 - fy;
    } else {
        fy = 0.0;
    }
    let w = w0 * smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
    return w;
}

fn dotted(uv: vec2<f32>) -> f32 {
    let fy = 4.0 * fract(uv.y / 4.0) - 0.5;
    return smoothstep(0.0, 1.0, 6.0 * (0.25 - (uv.x * uv.x + fy * fy)));
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var result = vec4<f32>(0.0, 0.0, 0.0, 0.0);
    let scissor = scissorMask(in.fpos);
    var strokeAlpha = 1.0;
    if (EDGE_AA == 1u) {
        strokeAlpha = strokeMask(in.ftcoord);
        if (frag.lineStyle == 2) { strokeAlpha *= dashed(in.uv); }
        if (frag.lineStyle == 3) { strokeAlpha *= dotted(in.uv); }
        if (frag.lineStyle == 4) { strokeAlpha *= glow(in.uv); }
        if (strokeAlpha < frag.strokeThr) { discard; }
    } else {
        if (frag.lineStyle == 2) { strokeAlpha *= dashed(in.uv); }
        if (frag.lineStyle == 3) { strokeAlpha *= dotted(in.uv); }
        if (frag.lineStyle == 4) { strokeAlpha *= glow(in.uv); }
        if (frag.lineStyle > 1 && strokeAlpha < frag.strokeThr) { discard; }
    }

    if (frag.type_ == 0) { // Gradient
        let pt = xformPt(frag.paintMat, in.fpos);
        let d = clamp((sdroundrect(pt, frag.extent, frag.radius) + frag.feather * 0.5) / frag.feather, 0.0, 1.0);
        var color = mix(frag.innerCol, frag.outerCol, d);
        color *= strokeAlpha * scissor;
        result = color;
    } else if (frag.type_ == 1) { // Image
        let pt = xformPt(frag.paintMat, in.fpos) / frag.extent;
        var color = textureSample(tex, texSampler, pt);
        if (frag.texType == 1) { color = vec4<f32>(color.xyz * color.w, color.w); }
        if (frag.texType == 2) { color = vec4<f32>(color.x, color.x, color.x, color.x); }
        if (frag.texType == 3 && color.a == 1.0) { discard; }
        color *= frag.innerCol;
        color *= strokeAlpha * scissor;
        result = color;
    } else if (frag.type_ == 2) { // Stencil fill
        result = vec4<f32>(1.0, 1.0, 1.0, 1.0);
    } else if (frag.type_ == 3) { // Textured tris
        var color = textureSample(tex, texSampler, in.ftcoord);
        if (frag.texType == 1) { color = vec4<f32>(color.xyz * color.w, color.w); }
        if (frag.texType == 2) { color = vec4<f32>(color.x, color.x, color.x, color.x); }
        color *= scissor;
        result = color * frag.innerCol;
    }

    return result;
}

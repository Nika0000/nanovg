#ifndef NANOVG_WGPU_H
#define NANOVG_WGPU_H

// This backend targets the standard C webgpu.h header (the interface shared by
// Dawn, wgpu-native, and Emscripten's emdawnwebgpu port), not any single
// implementation. Include webgpu.h (or an implementation's variant of it)
// before including this file.
//
// Feature parity note: unlike nanovg_gl.h, this backend does not implement
// nvgStencil()/nvgStencilClear() (NVGstencilFlags) 
enum NVGcreateFlags {
	// Flag indicating if geometry based anti-aliasing is used (may not be needed
	// when using MSAA).
	NVG_ANTIALIAS = 1 << 0,
	// Flag indicating if strokes should be drawn using stencil buffer. The
	// rendering will be a little
	// slower, but path overlaps (i.e. self-intersecting or sharp turns) will be
	// drawn just once.
	NVG_STENCIL_STROKES = 1 << 1,
	// Flag indicating that additional debug checks are done.
	NVG_DEBUG = 1 << 2,
};

typedef struct WGPUNVGCreateInfo {
	WGPUDevice device;                // borrowed, app-owned
	WGPUQueue queue;                  // borrowed
	WGPUTextureFormat colorFormat;    // format of the color target the pipelines render into
	WGPUTextureFormat depthStencilFormat; // e.g. WGPUTextureFormat_Depth24PlusStencil8
} WGPUNVGCreateInfo;

enum NVGimageFlagsWgpu {
	NVG_IMAGE_NODELETE = 1 << 16,
};

#ifdef __cplusplus
extern "C" {
#endif

struct NVGcontext* nvgCreateWgpu(WGPUNVGCreateInfo createInfo, int flags);
void nvgDeleteWgpu(struct NVGcontext* ctx);

// Call once per frame, after beginning the app's render pass (color attachment +
// depth-stencil attachment matching createInfo.depthStencilFormat, stencil cleared
// to 0) and before issuing any nanovg draw calls this frame. nanovg may flush more
// than once per frame (e.g. when the font atlas grows), so the pass must already be
// bound before the first nvgBeginPath()/nvgText()/etc. call, not just before
// nvgEndFrame().
void nvgWgpuBindRenderPass(struct NVGcontext* ctx, WGPURenderPassEncoder pass);

#ifdef __cplusplus
}
#endif

#endif // NANOVG_WGPU_H

#ifdef NANOVG_WGPU_IMPLEMENTATION
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "nanovg.h"

#if !defined(__cplusplus) || defined(NANOVG_WGPU_NO_nullptrPTR)
#define nullptr NULL
#endif

enum WGNVGshaderType {
	NSVG_SHADER_FILLGRAD,
	NSVG_SHADER_FILLIMG,
	NSVG_SHADER_SIMPLE,
	NSVG_SHADER_IMG,
};

enum WGNVGcallType {
	WGNVG_NONE = 0,
	WGNVG_FILL,
	WGNVG_CONVEXFILL,
	WGNVG_STROKE,
	WGNVG_TRIANGLES,
};

enum WGNVGstencilSetting {
	WGNVG_STENCIL_STROKE_UNDEFINED = 0,
	WGNVG_STENCIL_STROKE_FILL      = 1,
	WGNVG_STENCIL_STROKE_DRAW_AA,
	WGNVG_STENCIL_STROKE_CLEAR,
};

typedef struct WGNVGtexture {
	WGPUTexture texture;
	WGPUTextureView view;
	WGPUSampler sampler;
	WGPUBindGroup bindGroup; // group 1: texture_2d + sampler, cached per texture
	int32_t width, height;
	int type;  // enum NVGtexture
	int flags; // enum NVGimageFlags
} WGNVGtexture;

typedef struct WGNVGcall {
	int type;
	int image;
	int pathOffset;
	int pathCount;
	int triangleOffset;
	int triangleCount;
	int uniformOffset;
	NVGcompositeOperationState compositOperation;
} WGNVGcall;

typedef struct WGNVGpath {
	int fillOffset;
	int fillCount;
	int strokeOffset;
	int strokeCount;
} WGNVGpath;

// Layout must match the WGSL FragmentData struct in fill.wgsl exactly (first
// 192 bytes of this struct are shader-visible). The trailing pad256 exists only
// so each record is a legal dynamic storage-buffer offset - it is never read by
// the shader, which sees only one record at a time via the bound offset.
typedef struct WGNVGfragUniforms {
	float scissorMat[12]; // matrices are actually 3 vec4s (mat3x4 in WGSL)
	float paintMat[12];
	struct NVGcolor innerCol;
	struct NVGcolor outerCol;
	float scissorExt[2];
	float scissorScale[2];
	float extent[2];
	float radius;
	float feather;
	float strokeMult;
	float strokeThr;
	int lineStyle;
	int texType;
	int type;
	int padding[3]; // rounds the shader-visible part up to 192 bytes (mat3x4 alignment)
	unsigned char pad256[64]; // 192 -> 256, minStorageBufferOffsetAlignment default
} WGNVGfragUniforms;

typedef struct WGNVGpipelineKey {
	int stencilStroke; // enum WGNVGstencilSetting
	bool stencilFill;
	bool stencilTest;
	bool edgeAA;
	WGPUPrimitiveTopology topology;
	NVGcompositeOperationState compositOperation;
	WGPUColorWriteMask colorWriteMask; // set and compared independently
} WGNVGpipelineKey;

typedef struct WGNVGpipeline {
	WGNVGpipelineKey createKey;
	WGPURenderPipeline pipeline;
} WGNVGpipeline;

typedef struct WGNVGbuffer {
	WGPUBuffer buffer;
	uint64_t size; // allocated capacity in bytes
} WGNVGbuffer;

typedef struct WGNVGcontext {
	WGPUNVGCreateInfo createInfo;
	int flags;

	WGPUDevice device;
	WGPUQueue queue;

	uint32_t uniformAlign; // stride of one frag-uniform record (>= 256, aligned to minStorageBufferOffsetAlignment)

	// own resources
	WGNVGtexture* textures;
	int ntextures;
	int ctextures;

	WGPUBindGroupLayout bindGroupLayout0; // storage (dynamic offset) + viewSize uniform
	WGPUBindGroupLayout bindGroupLayout1; // texture_2d + sampler
	WGPUPipelineLayout pipelineLayout;
	WGPUShaderModule shaderModule;

	WGNVGpipeline* pipelines;
	int cpipelines;
	int npipelines;

	// Per frame buffers (CPU side)
	WGNVGcall* calls;
	int ccalls;
	uint32_t ncalls;
	WGNVGpath* paths;
	int cpaths;
	int npaths;
	struct NVGvertex* verts;
	int cverts;
	int nverts;
	unsigned char* uniforms;
	int cuniforms;
	int nuniforms;

	float viewSize[2];
	WGPUBuffer viewSizeBuffer; // small uniform buffer, rewritten each flush

	// One vertex buffer + one frag-uniform storage buffer + one bind group 0 per
	// flush index. wgpuQueueWriteBuffer is queue-ordered ahead of any later
	// submit, so no need to duplicate these per swapchain image - only per flush index 
	// within a frame (mid-frame flushes, e.g. from font atlas growth, must not alias buffers an
	// earlier flush's already-recorded draws still reference).
	WGNVGbuffer* vertexBuffers;
	WGNVGbuffer* uniformBuffers;
	WGPUBindGroup* bindGroups0;
	uint32_t maxFlushesPerFrame;
	uint32_t pendingMaxFlushes;
	uint32_t flushIndex;

	WGNVGpipeline* currentPipeline;
	WGPURenderPassEncoder pass; // bound once per frame via nvgWgpuBindRenderPass

	WGPUShaderModule vertShaderUnused; // reserved, keeps struct stable if split later
} WGNVGcontext;

static int wgnvg_maxi(int a, int b) { return a > b ? a : b; }
static uint32_t wgnvg_alignUp(uint32_t v, uint32_t align) { return (v + align - 1) / align * align; }

static void wgnvg_xformToMat3x4(float* m3, float* t) {
	m3[0]  = t[0];
	m3[1]  = t[1];
	m3[2]  = 0.0f;
	m3[3]  = 0.0f;
	m3[4]  = t[2];
	m3[5]  = t[3];
	m3[6]  = 0.0f;
	m3[7]  = 0.0f;
	m3[8]  = t[4];
	m3[9]  = t[5];
	m3[10] = 1.0f;
	m3[11] = 0.0f;
}

static NVGcolor wgnvg_premulColor(NVGcolor c) {
	c.r *= c.a;
	c.g *= c.a;
	c.b *= c.a;
	return c;
}

static WGNVGtexture* wgnvg_findTexture(WGNVGcontext* wg, int id) {
	if (id > wg->ntextures || id <= 0) {
		return nullptr;
	}
	return wg->textures + id - 1;
}

static WGNVGtexture* wgnvg_allocTexture(WGNVGcontext* wg) {
	WGNVGtexture* tex = nullptr;
	int i;
	for (i = 0; i < wg->ntextures; i++) {
		if (wg->textures[i].texture == nullptr) {
			tex = &wg->textures[i];
			break;
		}
	}
	if (tex == nullptr) {
		if (wg->ntextures + 1 > wg->ctextures) {
			WGNVGtexture* textures;
			int ctextures = wgnvg_maxi(wg->ntextures + 1, 4) + wg->ctextures / 2; // 1.5x Overallocate
			textures       = (WGNVGtexture*) realloc(wg->textures, sizeof(WGNVGtexture) * ctextures);
			if (textures == nullptr)
				return nullptr;
			wg->textures  = textures;
			wg->ctextures = ctextures;
		}
		tex = &wg->textures[wg->ntextures++];
	}
	memset(tex, 0, sizeof(*tex));
	return tex;
}

static int wgnvg_textureId(WGNVGcontext* wg, WGNVGtexture* tex) {
	ptrdiff_t id = tex - wg->textures;
	if (id < 0 || id > wg->ntextures) {
		return 0;
	}
	return (int) id + 1;
}

static int wgnvg_deleteTexture(WGNVGtexture* tex) {
	if (!tex->texture) {
		return 0;
	}
	if (tex->bindGroup) {
		wgpuBindGroupRelease(tex->bindGroup);
	}
	if (tex->sampler) {
		wgpuSamplerRelease(tex->sampler);
	}
	if (tex->view) {
		wgpuTextureViewRelease(tex->view);
	}
	wgpuTextureDestroy(tex->texture);
	wgpuTextureRelease(tex->texture);
	memset(tex, 0, sizeof(*tex));
	return 1;
}

static WGNVGpipeline* wgnvg_allocPipeline(WGNVGcontext* wg) {
	if (wg->npipelines + 1 > wg->cpipelines) {
		WGNVGpipeline* pipelines;
		int cpipelines = wgnvg_maxi(wg->npipelines + 1, 128) + wg->cpipelines / 2; // 1.5x Overallocate
		pipelines       = (WGNVGpipeline*) realloc(wg->pipelines, sizeof(WGNVGpipeline) * cpipelines);
		if (pipelines == nullptr)
			return nullptr;
		wg->pipelines  = pipelines;
		wg->cpipelines = cpipelines;
	}
	WGNVGpipeline* ret = &wg->pipelines[wg->npipelines++];
	memset(ret, 0, sizeof(*ret));
	return ret;
}

// WebGPU has no extended-dynamic-state escape hatch, so every field is 
// always compared - every distinct state combination is a real WGPURenderPipeline.
static int wgnvg_compareCreatePipelineKey(const WGNVGpipelineKey* a, const WGNVGpipelineKey* b) {
	if (a->topology != b->topology) return (int) a->topology - (int) b->topology;
	if (a->stencilTest != b->stencilTest) return a->stencilTest - b->stencilTest;
	if (a->stencilFill != b->stencilFill) return a->stencilFill - b->stencilFill;
	if (a->stencilStroke != b->stencilStroke) return a->stencilStroke - b->stencilStroke;
	if (a->colorWriteMask != b->colorWriteMask) return (int) a->colorWriteMask - (int) b->colorWriteMask;
	if (a->edgeAA != b->edgeAA) return a->edgeAA - b->edgeAA;
	if (a->compositOperation.srcRGB != b->compositOperation.srcRGB) return a->compositOperation.srcRGB - b->compositOperation.srcRGB;
	if (a->compositOperation.srcAlpha != b->compositOperation.srcAlpha) return a->compositOperation.srcAlpha - b->compositOperation.srcAlpha;
	if (a->compositOperation.dstRGB != b->compositOperation.dstRGB) return a->compositOperation.dstRGB - b->compositOperation.dstRGB;
	if (a->compositOperation.dstAlpha != b->compositOperation.dstAlpha) return a->compositOperation.dstAlpha - b->compositOperation.dstAlpha;
	return 0;
}

static WGNVGpipeline* wgnvg_findPipeline(WGNVGcontext* wg, const WGNVGpipelineKey* key) {
	for (int i = 0; i < wg->npipelines; i++) {
		if (wgnvg_compareCreatePipelineKey(&wg->pipelines[i].createKey, key) == 0) {
			return &wg->pipelines[i];
		}
	}
	return nullptr;
}

static int wgnvg_convertPaint(WGNVGcontext* wg, WGNVGfragUniforms* frag, NVGpaint* paint, NVGscissor* scissor, float width, float fringe, float strokeThr, int lineStyle) {
	WGNVGtexture* tex = nullptr;
	float invxform[6];

	memset(frag, 0, sizeof(*frag));

	frag->innerCol = wgnvg_premulColor(paint->innerColor);
	frag->outerCol = wgnvg_premulColor(paint->outerColor);

	if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
		memset(frag->scissorMat, 0, sizeof(frag->scissorMat));
		frag->scissorExt[0]   = 1.0f;
		frag->scissorExt[1]   = 1.0f;
		frag->scissorScale[0] = 1.0f;
		frag->scissorScale[1] = 1.0f;
	} else {
		nvgTransformInverse(invxform, scissor->xform);
		wgnvg_xformToMat3x4(frag->scissorMat, invxform);
		frag->scissorExt[0]   = scissor->extent[0];
		frag->scissorExt[1]   = scissor->extent[1];
		frag->scissorScale[0] = sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
		frag->scissorScale[1] = sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
	}

	memcpy(frag->extent, paint->extent, sizeof(frag->extent));
	frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
	frag->strokeThr  = strokeThr;
	frag->lineStyle  = lineStyle;

	if (paint->image != 0) {
		tex = wgnvg_findTexture(wg, paint->image);
		if (tex == nullptr)
			return 0;
		if ((tex->flags & NVG_IMAGE_FLIPY) != 0) {
			float m1[6], m2[6];
			nvgTransformTranslate(m1, 0.0f, frag->extent[1] * 0.5f);
			nvgTransformMultiply(m1, paint->xform);
			nvgTransformScale(m2, 1.0f, -1.0f);
			nvgTransformMultiply(m2, m1);
			nvgTransformTranslate(m1, 0.0f, -frag->extent[1] * 0.5f);
			nvgTransformMultiply(m1, m2);
			nvgTransformInverse(invxform, m1);
		} else {
			nvgTransformInverse(invxform, paint->xform);
		}
		frag->type = NSVG_SHADER_FILLIMG;

		if (tex->type == NVG_TEXTURE_RGBA)
			frag->texType = (tex->flags & NVG_IMAGE_PREMULTIPLIED) ? 0 : 1;
		else
			frag->texType = 2;
	} else {
		frag->type    = NSVG_SHADER_FILLGRAD;
		frag->radius  = paint->radius;
		frag->feather = paint->feather;
		nvgTransformInverse(invxform, paint->xform);
	}

	wgnvg_xformToMat3x4(frag->paintMat, invxform);

	return 1;
}

static WGPUBlendFactor wgnvg_NVGblendFactorToWGPUBlendFactor(enum NVGblendFactor factor) {
	switch (factor) {
		case NVG_ZERO: return WGPUBlendFactor_Zero;
		case NVG_ONE: return WGPUBlendFactor_One;
		case NVG_SRC_COLOR: return WGPUBlendFactor_Src;
		case NVG_ONE_MINUS_SRC_COLOR: return WGPUBlendFactor_OneMinusSrc;
		case NVG_DST_COLOR: return WGPUBlendFactor_Dst;
		case NVG_ONE_MINUS_DST_COLOR: return WGPUBlendFactor_OneMinusDst;
		case NVG_SRC_ALPHA: return WGPUBlendFactor_SrcAlpha;
		case NVG_ONE_MINUS_SRC_ALPHA: return WGPUBlendFactor_OneMinusSrcAlpha;
		case NVG_DST_ALPHA: return WGPUBlendFactor_DstAlpha;
		case NVG_ONE_MINUS_DST_ALPHA: return WGPUBlendFactor_OneMinusDstAlpha;
		case NVG_SRC_ALPHA_SATURATE: return WGPUBlendFactor_SrcAlphaSaturated;
		default: return WGPUBlendFactor_Undefined;
	}
}

static WGPUColorWriteMask wgnvg_colorWriteMask(const WGNVGpipelineKey* key) {
	if (key->stencilStroke == WGNVG_STENCIL_STROKE_CLEAR) {
		return WGPUColorWriteMask_None;
	}
	if (key->stencilFill) {
		return WGPUColorWriteMask_None;
	}
	return WGPUColorWriteMask_All;
}

static WGPUBlendState wgnvg_compositOperationToBlendState(const WGNVGpipelineKey* key) {
	WGPUBlendState state;
	memset(&state, 0, sizeof(state));
	state.color.operation = WGPUBlendOperation_Add;
	state.alpha.operation = WGPUBlendOperation_Add;

	state.color.srcFactor = wgnvg_NVGblendFactorToWGPUBlendFactor((enum NVGblendFactor) key->compositOperation.srcRGB);
	state.alpha.srcFactor = wgnvg_NVGblendFactorToWGPUBlendFactor((enum NVGblendFactor) key->compositOperation.srcAlpha);
	state.color.dstFactor = wgnvg_NVGblendFactorToWGPUBlendFactor((enum NVGblendFactor) key->compositOperation.dstRGB);
	state.alpha.dstFactor = wgnvg_NVGblendFactorToWGPUBlendFactor((enum NVGblendFactor) key->compositOperation.dstAlpha);

	if (state.color.srcFactor == WGPUBlendFactor_Undefined || state.alpha.srcFactor == WGPUBlendFactor_Undefined ||
	    state.color.dstFactor == WGPUBlendFactor_Undefined || state.alpha.dstFactor == WGPUBlendFactor_Undefined) {
		state.color.srcFactor = WGPUBlendFactor_One;
		state.alpha.srcFactor = WGPUBlendFactor_OneMinusSrcAlpha;
		state.color.dstFactor = WGPUBlendFactor_One;
		state.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	}
	return state;
}

static WGPUCullMode wgnvg_cullMode(const WGNVGpipelineKey* key) {
	return key->stencilFill ? WGPUCullMode_None : WGPUCullMode_Back;
}

// Depth is a deliberate no-op (the stencil attachment is what's actually used).
static WGPUDepthStencilState wgnvg_depthStencilState(WGNVGcontext* wg, const WGNVGpipelineKey* key) {
	WGPUDepthStencilState ds;
	memset(&ds, 0, sizeof(ds));
	ds.format            = wg->createInfo.depthStencilFormat;
	ds.depthWriteEnabled = WGPUOptionalBool_False;
	ds.depthCompare      = WGPUCompareFunction_Always;
	ds.stencilReadMask   = 0xff;
	ds.stencilWriteMask  = 0xff;

	WGPUStencilFaceState keepAlways = {WGPUCompareFunction_Always, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep};
	ds.stencilFront                = keepAlways;
	ds.stencilBack                 = keepAlways;

	if (key->stencilStroke) {
		WGPUStencilFaceState front = {WGPUCompareFunction_Equal, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep};
		WGPUStencilFaceState back  = front;
		back.passOp                = WGPUStencilOperation_DecrementClamp;

		switch (key->stencilStroke) {
			case WGNVG_STENCIL_STROKE_FILL:
				front.passOp = WGPUStencilOperation_IncrementClamp;
				back.passOp  = WGPUStencilOperation_DecrementClamp;
				break;
			case WGNVG_STENCIL_STROKE_DRAW_AA:
				front.passOp = WGPUStencilOperation_Keep;
				back.passOp  = WGPUStencilOperation_Keep;
				break;
			case WGNVG_STENCIL_STROKE_CLEAR:
				front.failOp      = WGPUStencilOperation_Zero;
				front.depthFailOp = WGPUStencilOperation_Zero;
				front.passOp      = WGPUStencilOperation_Zero;
				front.compare     = WGPUCompareFunction_Always;
				back              = front;
				break;
			default:
				break;
		}
		ds.stencilFront = front;
		ds.stencilBack  = back;
		return ds;
	}

	if (key->stencilFill) {
		WGPUStencilFaceState front = {WGPUCompareFunction_Always, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep, WGPUStencilOperation_IncrementWrap};
		WGPUStencilFaceState back  = front;
		back.passOp                = WGPUStencilOperation_DecrementWrap;
		ds.stencilFront            = front;
		ds.stencilBack             = back;
	} else if (key->stencilTest) {
		if (key->edgeAA) {
			WGPUStencilFaceState front = {WGPUCompareFunction_Equal, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep, WGPUStencilOperation_Keep};
			ds.stencilFront            = front;
			ds.stencilBack             = front;
		} else {
			WGPUStencilFaceState front = {WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero, WGPUStencilOperation_Zero, WGPUStencilOperation_Zero};
			ds.stencilFront            = front;
			ds.stencilBack             = front;
		}
	}

	return ds;
}

static WGNVGpipeline* wgnvg_createPipeline(WGNVGcontext* wg, WGNVGpipelineKey* key) {
	WGPUVertexAttribute attrs[3];
	attrs[0].format         = WGPUVertexFormat_Float32x2;
	attrs[0].offset         = 0;
	attrs[0].shaderLocation = 0;
	attrs[1].format         = WGPUVertexFormat_Float32x2;
	attrs[1].offset         = 2 * sizeof(float);
	attrs[1].shaderLocation = 1;
	attrs[2].format         = WGPUVertexFormat_Float32x2;
	attrs[2].offset         = 4 * sizeof(float);
	attrs[2].shaderLocation = 2;

	WGPUVertexBufferLayout vbLayout;
	memset(&vbLayout, 0, sizeof(vbLayout));
	vbLayout.arrayStride    = sizeof(struct NVGvertex);
	vbLayout.stepMode       = WGPUVertexStepMode_Vertex;
	vbLayout.attributeCount = 3;
	vbLayout.attributes     = attrs;

	WGPUBlendState blend = wgnvg_compositOperationToBlendState(key);
	key->colorWriteMask  = wgnvg_colorWriteMask(key);

	WGPUColorTargetState colorTarget;
	memset(&colorTarget, 0, sizeof(colorTarget));
	colorTarget.format    = wg->createInfo.colorFormat;
	colorTarget.blend     = &blend;
	colorTarget.writeMask = key->colorWriteMask;

	WGPUFragmentState fragState;
	memset(&fragState, 0, sizeof(fragState));
	fragState.module      = wg->shaderModule;
	fragState.entryPoint  = (WGPUStringView) {"fs_main", WGPU_STRLEN};
	fragState.targetCount = 1;
	fragState.targets     = &colorTarget;

	WGPUDepthStencilState depthStencil = wgnvg_depthStencilState(wg, key);

	WGPURenderPipelineDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.layout                        = wg->pipelineLayout;
	desc.vertex.module                 = wg->shaderModule;
	desc.vertex.entryPoint             = (WGPUStringView) {"vs_main", WGPU_STRLEN};
	desc.vertex.bufferCount            = 1;
	desc.vertex.buffers                = &vbLayout;
	desc.primitive.topology            = key->topology;
	desc.primitive.frontFace           = WGPUFrontFace_CCW;
	desc.primitive.cullMode            = wgnvg_cullMode(key);
	desc.depthStencil                  = &depthStencil;
	desc.multisample.count             = 1;
	desc.multisample.mask              = 0xFFFFFFFF;
	desc.fragment                      = &fragState;

	WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(wg->device, &desc);
	assert(pipeline != nullptr);

	WGNVGpipeline* ret = wgnvg_allocPipeline(wg);
	ret->createKey       = *key;
	ret->pipeline         = pipeline;
	return ret;
}

static WGPURenderPipeline wgnvg_bindPipeline(WGNVGcontext* wg, WGNVGpipelineKey* key) {
	key->colorWriteMask     = wgnvg_colorWriteMask(key); // always set before compare
	WGNVGpipeline* pipeline = wgnvg_findPipeline(wg, key);
	if (!pipeline) {
		pipeline = wgnvg_createPipeline(wg, key);
	}
	if (pipeline != wg->currentPipeline) {
		wgpuRenderPassEncoderSetPipeline(wg->pass, pipeline->pipeline);
		wgpuRenderPassEncoderSetStencilReference(wg->pass, 0);
		wg->currentPipeline = pipeline;
	}
	return pipeline->pipeline;
}

static int wgnvg_maxVertCountList(const NVGpath* paths, int npaths) {
	int i, count = 0;
	for (i = 0; i < npaths; i++) {
		count += (paths[i].nfill - 2) * 3;
		count += paths[i].nstroke;
	}
	return count;
}

static WGNVGcall* wgnvg_allocCall(WGNVGcontext* wg) {
	if (wg->ncalls + 1 > (uint32_t) wg->ccalls) {
		WGNVGcall* calls;
		int ccalls = wgnvg_maxi(wg->ncalls + 1, 128) + wg->ccalls / 2; // 1.5x Overallocate
		calls       = (WGNVGcall*) realloc(wg->calls, sizeof(WGNVGcall) * ccalls);
		if (calls == nullptr)
			return nullptr;
		wg->calls  = calls;
		wg->ccalls = ccalls;
	}
	WGNVGcall* ret = &wg->calls[wg->ncalls++];
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static int wgnvg_allocPaths(WGNVGcontext* wg, int n) {
	if (wg->npaths + n > wg->cpaths) {
		WGNVGpath* paths;
		int cpaths = wgnvg_maxi(wg->npaths + n, 128) + wg->cpaths / 2; // 1.5x Overallocate
		paths       = (WGNVGpath*) realloc(wg->paths, sizeof(WGNVGpath) * cpaths);
		if (paths == nullptr)
			return -1;
		wg->paths  = paths;
		wg->cpaths = cpaths;
	}
	int ret = wg->npaths;
	wg->npaths += n;
	return ret;
}

static int wgnvg_allocVerts(WGNVGcontext* wg, int n) {
	if (wg->nverts + n > wg->cverts) {
		struct NVGvertex* verts;
		int cverts = wgnvg_maxi(wg->nverts + n, 4096) + wg->cverts / 2; // 1.5x Overallocate
		verts       = (struct NVGvertex*) realloc(wg->verts, sizeof(struct NVGvertex) * cverts);
		if (verts == nullptr)
			return -1;
		wg->verts  = verts;
		wg->cverts = cverts;
	}
	int ret = wg->nverts;
	wg->nverts += n;
	return ret;
}

static int wgnvg_allocFragUniforms(WGNVGcontext* wg, int n) {
	int structSize = wg->uniformAlign;
	if (wg->nuniforms + n > wg->cuniforms) {
		unsigned char* uniforms;
		int cuniforms = wgnvg_maxi(wg->nuniforms + n, 128) + wg->cuniforms / 2; // 1.5x Overallocate
		uniforms       = (unsigned char*) realloc(wg->uniforms, (size_t) structSize * cuniforms);
		if (uniforms == nullptr)
			return -1;
		wg->uniforms  = uniforms;
		wg->cuniforms = cuniforms;
	}
	int ret = wg->nuniforms * structSize;
	wg->nuniforms += n;
	return ret;
}

static WGNVGfragUniforms* wgnvg_fragUniformPtr(WGNVGcontext* wg, int byteOffset) {
	return (WGNVGfragUniforms*) &wg->uniforms[byteOffset];
}

static void wgnvg_vset(struct NVGvertex* vtx, float x, float y, float u, float v) {
	vtx->x = x;
	vtx->y = y;
	vtx->u = u;
	vtx->v = v;
}

static WGNVGtexture* wgnvg_dummyTexture(WGNVGcontext* wg) {
	return wgnvg_findTexture(wg, 1);
}

static void wgnvg_setTextureBindGroup(WGNVGcontext* wg, int image) {
	WGNVGtexture* tex = (image != 0) ? wgnvg_findTexture(wg, image) : wgnvg_dummyTexture(wg);
	if (tex == nullptr)
		tex = wgnvg_dummyTexture(wg);
	wgpuRenderPassEncoderSetBindGroup(wg->pass, 1, tex->bindGroup, 0, nullptr);
}

static void wgnvg_setUniforms(WGNVGcontext* wg, int uniformOffset, int image) {
	uint32_t dynOffset = (uint32_t) uniformOffset;
	wgpuRenderPassEncoderSetBindGroup(wg->pass, 0, wg->bindGroups0[wg->flushIndex], 1, &dynOffset);
	wgnvg_setTextureBindGroup(wg, image);
}

static void wgnvg_fill(WGNVGcontext* wg, WGNVGcall* call) {
	WGNVGpath* paths = &wg->paths[call->pathOffset];
	int npaths        = call->pathCount;

	WGNVGpipelineKey key;
	memset(&key, 0, sizeof(key));
	key.compositOperation = call->compositOperation;
	key.topology           = WGPUPrimitiveTopology_TriangleList;
	key.stencilFill        = true;

	wgnvg_bindPipeline(wg, &key);
	wgnvg_setUniforms(wg, call->uniformOffset, call->image);
	for (int i = 0; i < npaths; i++) {
		wgpuRenderPassEncoderDraw(wg->pass, paths[i].fillCount, 1, paths[i].fillOffset, 0);
	}

	wgnvg_setUniforms(wg, call->uniformOffset + (int) wg->uniformAlign, call->image);

	if (wg->flags & NVG_ANTIALIAS) {
		key.compositOperation = call->compositOperation;
		key.topology           = WGPUPrimitiveTopology_TriangleStrip;
		key.stencilFill        = false;
		key.stencilTest        = true;
		key.edgeAA             = true;
		wgnvg_bindPipeline(wg, &key);
		wgnvg_setUniforms(wg, call->uniformOffset + (int) wg->uniformAlign, call->image);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}
	}

	key.compositOperation = call->compositOperation;
	key.topology           = WGPUPrimitiveTopology_TriangleStrip;
	key.stencilFill        = false;
	key.stencilTest        = true;
	key.edgeAA             = false;
	wgnvg_bindPipeline(wg, &key);
	wgnvg_setUniforms(wg, call->uniformOffset + (int) wg->uniformAlign, call->image);
	wgpuRenderPassEncoderDraw(wg->pass, call->triangleCount, 1, call->triangleOffset, 0);
}

static void wgnvg_convexFill(WGNVGcontext* wg, WGNVGcall* call) {
	WGNVGpath* paths = &wg->paths[call->pathOffset];
	int npaths        = call->pathCount;

	WGNVGpipelineKey key;
	memset(&key, 0, sizeof(key));
	key.compositOperation = call->compositOperation;
	key.topology           = WGPUPrimitiveTopology_TriangleList;

	wgnvg_bindPipeline(wg, &key);
	wgnvg_setUniforms(wg, call->uniformOffset, call->image);
	for (int i = 0; i < npaths; ++i) {
		wgpuRenderPassEncoderDraw(wg->pass, paths[i].fillCount, 1, paths[i].fillOffset, 0);
	}
	if (wg->flags & NVG_ANTIALIAS) {
		key.topology = WGPUPrimitiveTopology_TriangleStrip;
		wgnvg_bindPipeline(wg, &key);
		wgnvg_setUniforms(wg, call->uniformOffset, call->image);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}
	}
}

static void wgnvg_stroke(WGNVGcontext* wg, WGNVGcall* call) {
	WGNVGpath* paths = &wg->paths[call->pathOffset];
	int npaths        = call->pathCount;

	if (wg->flags & NVG_STENCIL_STROKES) {
		WGNVGpipelineKey key;
		memset(&key, 0, sizeof(key));
		key.compositOperation = call->compositOperation;
		key.topology           = WGPUPrimitiveTopology_TriangleStrip;

		key.stencilStroke = WGNVG_STENCIL_STROKE_FILL;
		wgnvg_bindPipeline(wg, &key);
		wgnvg_setUniforms(wg, call->uniformOffset + (int) wg->uniformAlign, call->image);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}

		key.stencilStroke = WGNVG_STENCIL_STROKE_DRAW_AA;
		wgnvg_bindPipeline(wg, &key);
		wgnvg_setUniforms(wg, call->uniformOffset, call->image);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}

		key.stencilStroke = WGNVG_STENCIL_STROKE_CLEAR;
		wgnvg_bindPipeline(wg, &key);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}
	} else {
		WGNVGpipelineKey key;
		memset(&key, 0, sizeof(key));
		key.compositOperation = call->compositOperation;
		key.topology           = WGPUPrimitiveTopology_TriangleStrip;

		wgnvg_bindPipeline(wg, &key);
		wgnvg_setUniforms(wg, call->uniformOffset, call->image);
		for (int i = 0; i < npaths; ++i) {
			wgpuRenderPassEncoderDraw(wg->pass, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
		}
	}
}

static void wgnvg_triangles(WGNVGcontext* wg, WGNVGcall* call) {
	if (call->triangleCount == 0)
		return;

	WGNVGpipelineKey key;
	memset(&key, 0, sizeof(key));
	key.compositOperation = call->compositOperation;
	key.topology           = WGPUPrimitiveTopology_TriangleList;

	wgnvg_bindPipeline(wg, &key);
	wgnvg_setUniforms(wg, call->uniformOffset, call->image);
	wgpuRenderPassEncoderDraw(wg->pass, call->triangleCount, 1, call->triangleOffset, 0);
}

static void wgnvg_ensureBuffer(WGPUDevice device, WGNVGbuffer* buf, WGPUBufferUsage usage, uint64_t neededSize) {
	if (buf->size >= neededSize)
		return;
	if (buf->buffer) {
		wgpuBufferDestroy(buf->buffer);
		wgpuBufferRelease(buf->buffer);
	}
	uint64_t newSize = neededSize + neededSize / 2; // 1.5x Overallocate
	newSize           = wgnvg_alignUp((uint32_t) newSize, 4);

	WGPUBufferDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.usage = usage | WGPUBufferUsage_CopyDst;
	desc.size  = newSize;

	buf->buffer = wgpuDeviceCreateBuffer(device, &desc);
	buf->size    = newSize;
}

static WGPUBindGroup wgnvg_createBindGroup0(WGNVGcontext* wg, WGPUBuffer uniformBuffer) {
	WGPUBindGroupEntry entries[2];
	memset(entries, 0, sizeof(entries));
	entries[0].binding = 0;
	entries[0].buffer  = uniformBuffer;
	entries[0].offset  = 0;
	entries[0].size    = wg->uniformAlign;
	entries[1].binding = 1;
	entries[1].buffer  = wg->viewSizeBuffer;
	entries[1].offset  = 0;
	entries[1].size    = 16;

	WGPUBindGroupDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.layout     = wg->bindGroupLayout0;
	desc.entryCount = 2;
	desc.entries    = entries;
	return wgpuDeviceCreateBindGroup(wg->device, &desc);
}

static WGPUBindGroup wgnvg_createBindGroup1(WGNVGcontext* wg, WGNVGtexture* tex) {
	WGPUBindGroupEntry entries[2];
	memset(entries, 0, sizeof(entries));
	entries[0].binding     = 0;
	entries[0].textureView = tex->view;
	entries[1].binding     = 1;
	entries[1].sampler     = tex->sampler;

	WGPUBindGroupDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.layout     = wg->bindGroupLayout1;
	desc.entryCount = 2;
	desc.entries    = entries;
	return wgpuDeviceCreateBindGroup(wg->device, &desc);
}

static void wgnvg_growFlushSlots(WGNVGcontext* wg, uint32_t newCount) {
	WGNVGbuffer* nv = (WGNVGbuffer*) realloc(wg->vertexBuffers, newCount * sizeof(WGNVGbuffer));
	WGNVGbuffer* nu = (WGNVGbuffer*) realloc(wg->uniformBuffers, newCount * sizeof(WGNVGbuffer));
	WGPUBindGroup* nb = (WGPUBindGroup*) realloc(wg->bindGroups0, newCount * sizeof(WGPUBindGroup));
	if (nv == nullptr || nu == nullptr || nb == nullptr)
		return; // keep old (possibly smaller) arrays; growth request effectively dropped
	uint32_t oldCount = wg->maxFlushesPerFrame;
	memset(nv + oldCount, 0, (newCount - oldCount) * sizeof(WGNVGbuffer));
	memset(nu + oldCount, 0, (newCount - oldCount) * sizeof(WGNVGbuffer));
	memset(nb + oldCount, 0, (newCount - oldCount) * sizeof(WGPUBindGroup));
	wg->vertexBuffers      = nv;
	wg->uniformBuffers     = nu;
	wg->bindGroups0        = nb;
	wg->maxFlushesPerFrame = newCount;
}

static int wgnvg_maxVertCount(const NVGpath* paths, int npaths) {
	int i, count = 0;
	for (i = 0; i < npaths; i++) {
		count += paths[i].nfill;
		count += paths[i].nstroke;
	}
	return count;
}

///==================================================================================================================
static int wgnvg_renderCreate(void* uptr) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;

	static const char wgslPrelude0[] = "const EDGE_AA: u32 = 0u;\n";
	static const char wgslPrelude1[] = "const EDGE_AA: u32 = 1u;\n";
	static const char wgslSource[]   = {
#include "fill.wgsl.inc"
	};

	const char* prelude = (wg->flags & NVG_ANTIALIAS) ? wgslPrelude1 : wgslPrelude0;
	size_t preludeLen    = strlen(prelude);
	size_t sourceLen     = sizeof(wgslSource); // includes trailing NUL from the .inc literal
	char* combined        = (char*) malloc(preludeLen + sourceLen + 1);
	memcpy(combined, prelude, preludeLen);
	memcpy(combined + preludeLen, wgslSource, sourceLen);
	combined[preludeLen + sourceLen] = '\0';

	WGPUShaderSourceWGSL wgslDesc;
	memset(&wgslDesc, 0, sizeof(wgslDesc));
	wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
	wgslDesc.code        = (WGPUStringView) {combined, WGPU_STRLEN};

	WGPUShaderModuleDescriptor smDesc;
	memset(&smDesc, 0, sizeof(smDesc));
	smDesc.nextInChain = &wgslDesc.chain;

	wg->shaderModule = wgpuDeviceCreateShaderModule(wg->device, &smDesc);
	free(combined);
	assert(wg->shaderModule != nullptr);

	WGPULimits limits;
	memset(&limits, 0, sizeof(limits));
	uint32_t minAlign = 256;
	if (wgpuDeviceGetLimits(wg->device, &limits) == WGPUStatus_Success) {
		minAlign = limits.minStorageBufferOffsetAlignment;
		if (minAlign < 1) minAlign = 256;
	}
	wg->uniformAlign = wgnvg_alignUp(wgnvg_maxi((int) sizeof(WGNVGfragUniforms) - 64 /* shader-visible size */, (int) minAlign), minAlign);
	if (wg->uniformAlign < sizeof(WGNVGfragUniforms))
		wg->uniformAlign = wgnvg_alignUp((uint32_t) sizeof(WGNVGfragUniforms), minAlign);

	WGPUBindGroupLayoutEntry g0entries[2];
	memset(g0entries, 0, sizeof(g0entries));
	g0entries[0].binding                            = 0;
	g0entries[0].visibility                         = WGPUShaderStage_Fragment;
	g0entries[0].buffer.type                        = WGPUBufferBindingType_ReadOnlyStorage;
	g0entries[0].buffer.hasDynamicOffset            = true;
	g0entries[0].buffer.minBindingSize              = wg->uniformAlign;
	g0entries[1].binding                            = 1;
	g0entries[1].visibility                         = WGPUShaderStage_Vertex;
	g0entries[1].buffer.type                        = WGPUBufferBindingType_Uniform;
	g0entries[1].buffer.minBindingSize              = 16;

	WGPUBindGroupLayoutDescriptor g0desc;
	memset(&g0desc, 0, sizeof(g0desc));
	g0desc.entryCount = 2;
	g0desc.entries    = g0entries;
	wg->bindGroupLayout0 = wgpuDeviceCreateBindGroupLayout(wg->device, &g0desc);

	WGPUBindGroupLayoutEntry g1entries[2];
	memset(g1entries, 0, sizeof(g1entries));
	g1entries[0].binding               = 0;
	g1entries[0].visibility            = WGPUShaderStage_Fragment;
	g1entries[0].texture.sampleType    = WGPUTextureSampleType_Float;
	g1entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
	g1entries[1].binding               = 1;
	g1entries[1].visibility            = WGPUShaderStage_Fragment;
	g1entries[1].sampler.type          = WGPUSamplerBindingType_Filtering;

	WGPUBindGroupLayoutDescriptor g1desc;
	memset(&g1desc, 0, sizeof(g1desc));
	g1desc.entryCount = 2;
	g1desc.entries    = g1entries;
	wg->bindGroupLayout1 = wgpuDeviceCreateBindGroupLayout(wg->device, &g1desc);

	WGPUBindGroupLayout layouts[2] = {wg->bindGroupLayout0, wg->bindGroupLayout1};
	WGPUPipelineLayoutDescriptor plDesc;
	memset(&plDesc, 0, sizeof(plDesc));
	plDesc.bindGroupLayoutCount = 2;
	plDesc.bindGroupLayouts     = layouts;
	wg->pipelineLayout           = wgpuDeviceCreatePipelineLayout(wg->device, &plDesc);

	WGPUBufferDescriptor vsDesc;
	memset(&vsDesc, 0, sizeof(vsDesc));
	vsDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
	vsDesc.size  = 16;
	wg->viewSizeBuffer = wgpuDeviceCreateBuffer(wg->device, &vsDesc);

	return 1;
}

static int wgnvg_renderCreateTexture(void* uptr, int type, int w, int h, int imageFlags, const unsigned char* data) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;

	WGNVGtexture* tex = wgnvg_allocTexture(wg);
	if (!tex)
		return 0;

	WGPUTextureFormat format = (type == NVG_TEXTURE_RGBA) ? WGPUTextureFormat_RGBA8Unorm : WGPUTextureFormat_R8Unorm;

	WGPUTextureDescriptor texDesc;
	memset(&texDesc, 0, sizeof(texDesc));
	texDesc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	texDesc.dimension     = WGPUTextureDimension_2D;
	texDesc.size          = (WGPUExtent3D) {(uint32_t) w, (uint32_t) h, 1};
	texDesc.format        = format;
	texDesc.mipLevelCount = 1;
	texDesc.sampleCount   = 1;

	WGPUTexture texture = wgpuDeviceCreateTexture(wg->device, &texDesc);
	if (!texture) {
		memset(tex, 0, sizeof(*tex));
		return 0;
	}

	WGPUTextureViewDescriptor viewDesc;
	memset(&viewDesc, 0, sizeof(viewDesc));
	viewDesc.format          = format;
	viewDesc.dimension       = WGPUTextureViewDimension_2D;
	viewDesc.mipLevelCount   = 1;
	viewDesc.arrayLayerCount = 1;
	WGPUTextureView view      = wgpuTextureCreateView(texture, &viewDesc);

	WGPUSamplerDescriptor sampDesc;
	memset(&sampDesc, 0, sizeof(sampDesc));
	sampDesc.magFilter    = (imageFlags & NVG_IMAGE_NEAREST) ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
	sampDesc.minFilter    = sampDesc.magFilter;
	sampDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
	sampDesc.addressModeU = (imageFlags & NVG_IMAGE_REPEATX) ? WGPUAddressMode_Repeat : WGPUAddressMode_ClampToEdge;
	sampDesc.addressModeV = (imageFlags & NVG_IMAGE_REPEATY) ? WGPUAddressMode_Repeat : WGPUAddressMode_ClampToEdge;
	sampDesc.addressModeW = WGPUAddressMode_ClampToEdge;
	sampDesc.maxAnisotropy = 1;
	WGPUSampler sampler     = wgpuDeviceCreateSampler(wg->device, &sampDesc);

	tex->texture = texture;
	tex->view    = view;
	tex->sampler = sampler;
	tex->width   = w;
	tex->height  = h;
	tex->type    = type;
	tex->flags   = imageFlags;
	tex->bindGroup = wgnvg_createBindGroup1(wg, tex);

	int comp = (type == NVG_TEXTURE_RGBA) ? 4 : 1;
	WGPUTexelCopyTextureInfo dst;
	memset(&dst, 0, sizeof(dst));
	dst.texture = texture;
	dst.origin  = (WGPUOrigin3D) {0, 0, 0};

	WGPUTexelCopyBufferLayout layout;
	memset(&layout, 0, sizeof(layout));
	layout.bytesPerRow  = (uint32_t) w * comp;
	layout.rowsPerImage = (uint32_t) h;

	WGPUExtent3D writeSize = {(uint32_t) w, (uint32_t) h, 1};

	if (data) {
		wgpuQueueWriteTexture(wg->queue, &dst, data, (size_t) w * h * comp, &layout, &writeSize);
	} else {
		// nanovg creates the font atlas (and some app textures) with data == NULL
		// and expects it to start fully zeroed.
		size_t size          = (size_t) w * h * comp;
		unsigned char* zeros = (unsigned char*) calloc(1, size);
		wgpuQueueWriteTexture(wg->queue, &dst, zeros, size, &layout, &writeSize);
		free(zeros);
	}

	return wgnvg_textureId(wg, tex);
}

static int wgnvg_renderDeleteTexture(void* uptr, int image) {
	WGNVGcontext* wg  = (WGNVGcontext*) uptr;
	WGNVGtexture* tex = wgnvg_findTexture(wg, image);
	if (!tex)
		return 0;
	return wgnvg_deleteTexture(tex);
}

static int wgnvg_renderUpdateTexture(void* uptr, int image, int x, int y, int w, int h, const unsigned char* data) {
	WGNVGcontext* wg  = (WGNVGcontext*) uptr;
	WGNVGtexture* tex = wgnvg_findTexture(wg, image);
	if (!tex)
		return 0;

	int comp = (tex->type == NVG_TEXTURE_RGBA) ? 4 : 1;

	WGPUTexelCopyTextureInfo dst;
	memset(&dst, 0, sizeof(dst));
	dst.texture = tex->texture;
	dst.origin  = (WGPUOrigin3D) {(uint32_t) x, (uint32_t) y, 0};

	WGPUTexelCopyBufferLayout layout;
	memset(&layout, 0, sizeof(layout));
	layout.bytesPerRow  = (uint32_t) tex->width * comp; // data points at the full-atlas base; row stride is the full width
	layout.rowsPerImage = (uint32_t) tex->height;

	const unsigned char* src = data + ((size_t) y * tex->width + x) * comp;
	WGPUExtent3D writeSize     = {(uint32_t) w, (uint32_t) h, 1};

	wgpuQueueWriteTexture(wg->queue, &dst, src, (size_t) tex->width * tex->height * comp, &layout, &writeSize);
	return 1;
}

static int wgnvg_renderGetTextureSize(void* uptr, int image, int* w, int* h) {
	WGNVGcontext* wg  = (WGNVGcontext*) uptr;
	WGNVGtexture* tex = wgnvg_findTexture(wg, image);
	if (tex) {
		*w = tex->width;
		*h = tex->height;
		return 1;
	}
	return 0;
}

static void wgnvg_renderViewport(void* uptr, float width, float height, float devicePixelRatio) {
	(void) devicePixelRatio;
	WGNVGcontext* wg = (WGNVGcontext*) uptr;
	wg->viewSize[0]   = width;
	wg->viewSize[1]   = height;
	wgpuQueueWriteBuffer(wg->queue, wg->viewSizeBuffer, 0, wg->viewSize, sizeof(wg->viewSize));

	wg->flushIndex = 0;

	// If a previous frame needed more flushes than provisioned, grow here where it's safe.
	if (wg->pendingMaxFlushes > wg->maxFlushesPerFrame) {
		wgnvg_growFlushSlots(wg, wg->pendingMaxFlushes);
	}
}

static void wgnvg_renderCancel(void* uptr) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;
	wg->nverts        = 0;
	wg->npaths        = 0;
	wg->ncalls        = 0;
	wg->nuniforms     = 0;
}

static void wgnvg_renderFlush(void* uptr) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;

	if (wg->maxFlushesPerFrame == 0) {
		wgnvg_growFlushSlots(wg, 2); // provision 2 slots by default; see the flushIndex comment in WGNVGcontext
	}

	uint32_t fi = wg->flushIndex;
	if (fi >= wg->maxFlushesPerFrame) {
		if (fi + 1 > wg->pendingMaxFlushes)
			wg->pendingMaxFlushes = fi + 1;
		fi = wg->maxFlushesPerFrame - 1; // clamp; a rare, transient one-frame glitch
	}
	wg->flushIndex = fi;

	if (wg->ncalls > 0) {
		wgnvg_ensureBuffer(wg->device, &wg->vertexBuffers[fi], WGPUBufferUsage_Vertex, (uint64_t) wg->nverts * sizeof(wg->verts[0]));
		wgnvg_ensureBuffer(wg->device, &wg->uniformBuffers[fi], WGPUBufferUsage_Storage, (uint64_t) wg->nuniforms * wg->uniformAlign);

		wgpuQueueWriteBuffer(wg->queue, wg->vertexBuffers[fi].buffer, 0, wg->verts, (size_t) wg->nverts * sizeof(wg->verts[0]));
		wgpuQueueWriteBuffer(wg->queue, wg->uniformBuffers[fi].buffer, 0, wg->uniforms, (size_t) wg->nuniforms * wg->uniformAlign);

		if (wg->bindGroups0[fi] != nullptr) {
			wgpuBindGroupRelease(wg->bindGroups0[fi]);
		}
		wg->bindGroups0[fi] = wgnvg_createBindGroup0(wg, wg->uniformBuffers[fi].buffer);

		wgpuRenderPassEncoderSetVertexBuffer(wg->pass, 0, wg->vertexBuffers[fi].buffer, 0, WGPU_WHOLE_SIZE);
		wg->currentPipeline = nullptr;

		for (uint32_t i = 0; i < wg->ncalls; i++) {
			WGNVGcall* call = &wg->calls[i];
			if (call->type == WGNVG_FILL) {
				wgnvg_fill(wg, call);
			} else if (call->type == WGNVG_CONVEXFILL) {
				wgnvg_convexFill(wg, call);
			} else if (call->type == WGNVG_STROKE) {
				wgnvg_stroke(wg, call);
			} else if (call->type == WGNVG_TRIANGLES) {
				wgnvg_triangles(wg, call);
			}
		}
	}

	wg->flushIndex++;
	wg->nverts    = 0;
	wg->npaths    = 0;
	wg->ncalls    = 0;
	wg->nuniforms = 0;
}

static void wgnvg_renderFill(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, float fringe, const float* bounds, const NVGpath* paths, int npaths) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;
	WGNVGcall* call   = wgnvg_allocCall(wg);
	struct NVGvertex* quad;
	WGNVGfragUniforms* frag;
	int i, maxverts, offset;

	if (call == nullptr)
		return;

	call->type          = WGNVG_FILL;
	call->triangleCount = 4;
	call->pathOffset    = wgnvg_allocPaths(wg, npaths);
	if (call->pathOffset == -1)
		goto error;
	call->pathCount         = npaths;
	call->image             = paint->image;
	call->compositOperation = compositeOperation;

	if (npaths == 1 && paths[0].convex) {
		call->type          = WGNVG_CONVEXFILL;
		call->triangleCount = 0;
	}

	maxverts = wgnvg_maxVertCountList(paths, npaths) + call->triangleCount;
	offset    = wgnvg_allocVerts(wg, maxverts);
	if (offset == -1)
		goto error;

	for (i = 0; i < npaths; i++) {
		WGNVGpath* copy     = &wg->paths[call->pathOffset + i];
		const NVGpath* path = &paths[i];
		memset(copy, 0, sizeof(*copy));
		if (path->nfill > 0) {
			copy->fillOffset = offset;
			copy->fillCount  = (path->nfill - 2) * 3;
			for (int j = 0; j < path->nfill - 2; j++) {
				wg->verts[offset]     = path->fill[0];
				wg->verts[offset + 1] = path->fill[j + 1];
				wg->verts[offset + 2] = path->fill[j + 2];
				offset += 3;
			}
		}
		if (path->nstroke > 0) {
			copy->strokeOffset = offset;
			copy->strokeCount  = path->nstroke;
			memcpy(&wg->verts[offset], path->stroke, sizeof(struct NVGvertex) * path->nstroke);
			offset += path->nstroke;
		}
	}

	if (call->type == WGNVG_FILL) {
		call->triangleOffset = offset;
		quad                  = &wg->verts[call->triangleOffset];
		wgnvg_vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
		wgnvg_vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
		wgnvg_vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
		wgnvg_vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);

		call->uniformOffset = wgnvg_allocFragUniforms(wg, 2);
		if (call->uniformOffset == -1)
			goto error;
		frag = wgnvg_fragUniformPtr(wg, call->uniformOffset);
		memset(frag, 0, sizeof(*frag));
		frag->strokeThr = -1.0f;
		frag->type      = NSVG_SHADER_SIMPLE;
		wgnvg_convertPaint(wg, wgnvg_fragUniformPtr(wg, call->uniformOffset + (int) wg->uniformAlign), paint, scissor, fringe, fringe, -1.0f, 0);
	} else {
		call->uniformOffset = wgnvg_allocFragUniforms(wg, 1);
		if (call->uniformOffset == -1)
			goto error;
		wgnvg_convertPaint(wg, wgnvg_fragUniformPtr(wg, call->uniformOffset), paint, scissor, fringe, fringe, -1.0f, 0);
	}

	return;

error:
	if (wg->ncalls > 0)
		wg->ncalls--;
}

static void wgnvg_renderStroke(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, float fringe, float strokeWidth, int lineStyle, const NVGpath* paths, int npaths) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;
	WGNVGcall* call   = wgnvg_allocCall(wg);
	int offset, maxverts, i;

	if (call == nullptr)
		return;

	call->type       = WGNVG_STROKE;
	call->pathOffset = wgnvg_allocPaths(wg, npaths);
	if (call->pathOffset == -1)
		goto error;
	call->pathCount         = npaths;
	call->image             = paint->image;
	call->compositOperation = compositeOperation;

	maxverts = wgnvg_maxVertCount(paths, npaths);
	offset    = wgnvg_allocVerts(wg, maxverts);
	if (offset == -1)
		goto error;

	for (i = 0; i < npaths; i++) {
		WGNVGpath* copy     = &wg->paths[call->pathOffset + i];
		const NVGpath* path = &paths[i];
		memset(copy, 0, sizeof(*copy));
		if (path->nstroke) {
			copy->strokeOffset = offset;
			copy->strokeCount  = path->nstroke;
			memcpy(&wg->verts[offset], path->stroke, sizeof(struct NVGvertex) * path->nstroke);
			offset += path->nstroke;
		}
	}

	if (wg->flags & NVG_STENCIL_STROKES) {
		call->uniformOffset = wgnvg_allocFragUniforms(wg, 2);
		if (call->uniformOffset == -1)
			goto error;
		wgnvg_convertPaint(wg, wgnvg_fragUniformPtr(wg, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f, lineStyle);
		wgnvg_convertPaint(wg, wgnvg_fragUniformPtr(wg, call->uniformOffset + (int) wg->uniformAlign), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f / 255.0f, lineStyle);
	} else {
		call->uniformOffset = wgnvg_allocFragUniforms(wg, 1);
		if (call->uniformOffset == -1)
			goto error;
		wgnvg_convertPaint(wg, wgnvg_fragUniformPtr(wg, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f, lineStyle);
	}

	return;

error:
	if (wg->ncalls > 0)
		wg->ncalls--;
}

static void wgnvg_renderTriangles(void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation, NVGscissor* scissor, const NVGvertex* verts, int nverts, float fringe) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;
	WGNVGcall* call   = wgnvg_allocCall(wg);
	WGNVGfragUniforms* frag;

	if (call == nullptr)
		return;

	call->type              = WGNVG_TRIANGLES;
	call->image             = paint->image;
	call->compositOperation = compositeOperation;

	call->triangleOffset = wgnvg_allocVerts(wg, nverts);
	if (call->triangleOffset == -1)
		goto error;
	call->triangleCount = nverts;
	memcpy(&wg->verts[call->triangleOffset], verts, sizeof(struct NVGvertex) * nverts);

	call->uniformOffset = wgnvg_allocFragUniforms(wg, 1);
	if (call->uniformOffset == -1)
		goto error;
	frag = wgnvg_fragUniformPtr(wg, call->uniformOffset);
	wgnvg_convertPaint(wg, frag, paint, scissor, 1.0f, fringe, -1.0f, 0);
	frag->type = NSVG_SHADER_IMG;

	return;

error:
	if (wg->ncalls > 0)
		wg->ncalls--;
}

static void wgnvg_renderDelete(void* uptr) {
	WGNVGcontext* wg = (WGNVGcontext*) uptr;

	for (int i = 0; i < wg->ntextures; i++) {
		if (wg->textures[i].texture != nullptr) {
			wgnvg_deleteTexture(&wg->textures[i]);
		}
	}

	for (uint32_t i = 0; i < wg->maxFlushesPerFrame; i++) {
		if (wg->vertexBuffers && wg->vertexBuffers[i].buffer) {
			wgpuBufferDestroy(wg->vertexBuffers[i].buffer);
			wgpuBufferRelease(wg->vertexBuffers[i].buffer);
		}
		if (wg->uniformBuffers && wg->uniformBuffers[i].buffer) {
			wgpuBufferDestroy(wg->uniformBuffers[i].buffer);
			wgpuBufferRelease(wg->uniformBuffers[i].buffer);
		}
		if (wg->bindGroups0 && wg->bindGroups0[i]) {
			wgpuBindGroupRelease(wg->bindGroups0[i]);
		}
	}

	if (wg->viewSizeBuffer) {
		wgpuBufferDestroy(wg->viewSizeBuffer);
		wgpuBufferRelease(wg->viewSizeBuffer);
	}

	for (int i = 0; i < wg->npipelines; i++) {
		wgpuRenderPipelineRelease(wg->pipelines[i].pipeline);
	}

	if (wg->pipelineLayout) wgpuPipelineLayoutRelease(wg->pipelineLayout);
	if (wg->bindGroupLayout0) wgpuBindGroupLayoutRelease(wg->bindGroupLayout0);
	if (wg->bindGroupLayout1) wgpuBindGroupLayoutRelease(wg->bindGroupLayout1);
	if (wg->shaderModule) wgpuShaderModuleRelease(wg->shaderModule);

	free(wg->vertexBuffers);
	free(wg->uniformBuffers);
	free(wg->bindGroups0);
	free(wg->textures);
	free(wg->pipelines);
	free(wg->calls);
	free(wg->paths);
	free(wg->verts);
	free(wg->uniforms);
	free(wg);
}

struct NVGcontext* nvgCreateWgpu(WGPUNVGCreateInfo createInfo, int flags) {
	NVGparams params;
	NVGcontext* ctx = nullptr;

	WGNVGcontext* wg = (WGNVGcontext*) malloc(sizeof(WGNVGcontext));
	if (wg == nullptr)
		goto error;
	memset(wg, 0, sizeof(*wg));

	wg->flags      = flags;
	wg->createInfo = createInfo;
	wg->device      = createInfo.device;
	wg->queue       = createInfo.queue;

	memset(&params, 0, sizeof(params));
	params.renderCreate         = wgnvg_renderCreate;
	params.renderCreateTexture  = wgnvg_renderCreateTexture;
	params.renderDeleteTexture  = wgnvg_renderDeleteTexture;
	params.renderUpdateTexture  = wgnvg_renderUpdateTexture;
	params.renderGetTextureSize = wgnvg_renderGetTextureSize;
	params.renderViewport       = wgnvg_renderViewport;
	params.renderCancel         = wgnvg_renderCancel;
	params.renderFlush          = wgnvg_renderFlush;
	params.renderFill           = wgnvg_renderFill;
	params.renderStroke         = wgnvg_renderStroke;
	params.renderTriangles      = wgnvg_renderTriangles;
	params.renderDelete         = wgnvg_renderDelete;
	params.userPtr              = wg;
	params.edgeAntiAlias        = flags & NVG_ANTIALIAS ? 1 : 0;

	ctx = nvgCreateInternal(&params);
	if (ctx == nullptr)
		goto error;

	// Real 1x1 opaque-white texture used whenever paint.
	{
		unsigned char white[4] = {255, 255, 255, 255};
		int dummyId              = wgnvg_renderCreateTexture(wg, NVG_TEXTURE_RGBA, 1, 1, 0, white);
		assert(dummyId == 1);
	}

	return ctx;

error:
	if (ctx != nullptr)
		nvgDeleteInternal(ctx);
	return nullptr;
}

void nvgDeleteWgpu(struct NVGcontext* ctx) { nvgDeleteInternal(ctx); }

void nvgWgpuBindRenderPass(struct NVGcontext* ctx, WGPURenderPassEncoder pass) {
	NVGparams* params = nvgInternalParams(ctx);
	WGNVGcontext* wg   = (WGNVGcontext*) params->userPtr;
	wg->pass            = pass;
	wg->currentPipeline = nullptr;
}

#if !defined(__cplusplus) || defined(NANOVG_WGPU_NO_nullptrPTR)
#undef nullptr
#endif
#endif // NANOVG_WGPU_IMPLEMENTATION

// nanovg playground -- WebAssembly host.
//
// Owns the single NVGcontext, the WebGL2 surface state, and the flat scalar ABI
// the JS side calls into.  Most of the API surface is generated into
// nanovg_web_gen.inc by playground/tools/gen_bindings.py; only signatures that
// can't be flattened mechanically (out params, byte buffers) live here.

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>

#include <GLES3/gl3.h>

#include "nanovg.h"
#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg_gl.h"

static NVGcontext* g_ctx = NULL;
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_gl = 0;

// --- value marshalling ------------------------------------------------------
//
// NVGcolor and NVGpaint are passed/returned by value in C.  Rather than fight
// the wasm struct ABI, colors flatten to four floats and are returned through a
// fixed scratch slot; paints are kept host-side and referred to by handle.

static float g_scratch[64]; // color returns, transform/bounds/metrics out params

static NVGcolor nvgw__col(float r, float g, float b, float a) {
	NVGcolor c;
	c.r = r;
	c.g = g;
	c.b = b;
	c.a = a;
	return c;
}

static void nvgw__retcol(NVGcolor c) { memcpy(g_scratch, c.rgba, sizeof(c.rgba)); }

// Paint slots are a ring: a handle stays valid until 256 further paints are
// created.  Sketch code always feeds a paint straight into fillPaint/strokePaint,
// so this is ample, and it can't leak.
#define NVGW_PAINT_SLOTS 256
static NVGpaint g_paints[NVGW_PAINT_SLOTS];
static int g_paintNext = 0;

static int nvgw__storepaint(NVGpaint p) {
	int slot        = g_paintNext;
	g_paints[slot]  = p;
	g_paintNext     = (g_paintNext + 1) % NVGW_PAINT_SLOTS;
	return slot;
}

static NVGpaint nvgw__paint(int handle) {
	if (handle < 0 || handle >= NVGW_PAINT_SLOTS) {
		NVGpaint empty;
		memset(&empty, 0, sizeof(empty));
		return empty;
	}
	return g_paints[handle];
}

EMSCRIPTEN_KEEPALIVE float* nvgw_scratch(void) { return g_scratch; }

#include "nanovg_web_gen.inc"

// --- lifecycle --------------------------------------------------------------

// Owning the WebGL context here (rather than handing one in from JS) keeps the
// attributes nanovg depends on in one place: a stencil buffer is mandatory
// because fills are stencil-then-cover, and antialias must be off so the
// backend's own geometric AA isn't doubled up by MSAA.
EMSCRIPTEN_KEEPALIVE int nvgw_create(const char* selector, int flags) {
	EmscriptenWebGLContextAttributes attrs;

	if (g_ctx != NULL) return 1;

	emscripten_webgl_init_context_attributes(&attrs);
	attrs.majorVersion              = 2; // WebGL2 == GLES3
	attrs.minorVersion              = 0;
	attrs.alpha                     = EM_TRUE;
	attrs.depth                     = EM_TRUE;
	attrs.stencil                   = EM_TRUE;
	attrs.antialias                 = EM_FALSE;
	attrs.premultipliedAlpha        = EM_TRUE;
	attrs.preserveDrawingBuffer     = EM_FALSE;
	attrs.powerPreference           = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
	attrs.failIfMajorPerformanceCaveat = EM_FALSE;
	attrs.enableExtensionsByDefault = EM_TRUE;

	g_gl = emscripten_webgl_create_context(selector, &attrs);
	if (g_gl == 0) return 0;
	if (emscripten_webgl_make_context_current(g_gl) != EMSCRIPTEN_RESULT_SUCCESS) return 0;

	g_ctx = nvgCreateGLES3(flags);
	return g_ctx != NULL;
}

EMSCRIPTEN_KEEPALIVE void nvgw_destroy(void) {
	if (g_ctx == NULL) return;
	nvgDeleteGLES3(g_ctx);
	g_ctx = NULL;
	if (g_gl != 0) {
		emscripten_webgl_destroy_context(g_gl);
		g_gl = 0;
	}
}

EMSCRIPTEN_KEEPALIVE void nvgw_beginFrame(float w, float h, float pxRatio) {
	glViewport(0, 0, (int) (w * pxRatio), (int) (h * pxRatio));
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	nvgBeginFrame(g_ctx, w, h, pxRatio);
}

EMSCRIPTEN_KEEPALIVE void nvgw_endFrame(void) { nvgEndFrame(g_ctx); }

EMSCRIPTEN_KEEPALIVE void nvgw_cancelFrame(void) { nvgCancelFrame(g_ctx); }

// --- draw-call inspector ----------------------------------------------------
//
// The GLES3 backend accumulates a frame's work into GLNVGcontext and submits it
// in one flush.  Snapshot those counters just before nvgEndFrame() resets them:
// this is what lets the playground show how paths coalesce into draw calls.

#define NVGW_STAT_COUNT 10

EMSCRIPTEN_KEEPALIVE int* nvgw_snapshotStats(void) {
	static int stats[NVGW_STAT_COUNT];
	GLNVGcontext* gl = (GLNVGcontext*) nvgInternalParams(g_ctx)->userPtr;
	int i;

	memset(stats, 0, sizeof(stats));
	stats[0] = gl->ncalls;
	stats[1] = gl->nverts;
	stats[2] = gl->npaths;
	stats[3] = gl->nuniforms;
	stats[4] = gl->ntextures;

	// stats[5..9] = per-call-type counts, in GLNVGcallType order from GLNVG_FILL.
	for (i = 0; i < gl->ncalls; i++) {
		int type = gl->calls[i].type;
		switch (type) {
		case GLNVG_FILL: stats[5]++; break;
		case GLNVG_CONVEXFILL: stats[6]++; break;
		case GLNVG_STROKE: stats[7]++; break;
		case GLNVG_TRIANGLES: stats[8]++; break;
		default: stats[9]++; break; // stencil variants
		}
	}
	return stats;
}

// --- manual bindings: out params -------------------------------------------
//
// Each writes into g_scratch; the JS wrapper reads it back out of HEAPF32.

EMSCRIPTEN_KEEPALIVE void nvgw_currentTransform(void) { nvgCurrentTransform(g_ctx, g_scratch); }

EMSCRIPTEN_KEEPALIVE float nvgw_textBounds(float x, float y, const char* str) {
	return nvgTextBounds(g_ctx, x, y, str, NULL, g_scratch);
}

EMSCRIPTEN_KEEPALIVE void nvgw_textBoxBounds(float x, float y, float breakRowWidth, const char* str) {
	nvgTextBoxBounds(g_ctx, x, y, breakRowWidth, str, NULL, g_scratch);
}

EMSCRIPTEN_KEEPALIVE void nvgw_textMetrics(void) {
	nvgTextMetrics(g_ctx, &g_scratch[0], &g_scratch[1], &g_scratch[2]);
}

EMSCRIPTEN_KEEPALIVE void nvgw_imageSize(int image) {
	int w = 0, h = 0;
	nvgImageSize(g_ctx, image, &w, &h);
	g_scratch[0] = (float) w;
	g_scratch[1] = (float) h;
}

// --- manual bindings: byte buffers -----------------------------------------
//
// The JS side fetches assets over HTTP, so only the *Mem/*RGBA loaders apply.
// nanovg takes ownership of font data (freeData=1), hence the copy.

EMSCRIPTEN_KEEPALIVE int nvgw_createFontMem(const char* name, const unsigned char* data, int ndata) {
	unsigned char* owned = (unsigned char*) malloc((size_t) ndata);
	if (owned == NULL) return -1;
	memcpy(owned, data, (size_t) ndata);
	return nvgCreateFontMem(g_ctx, name, owned, ndata, 1);
}

EMSCRIPTEN_KEEPALIVE int nvgw_createImageMem(int imageFlags, unsigned char* data, int ndata) {
	return nvgCreateImageMem(g_ctx, imageFlags, data, ndata);
}

EMSCRIPTEN_KEEPALIVE int nvgw_createImageRGBA(int w, int h, int imageFlags, const unsigned char* data) {
	return nvgCreateImageRGBA(g_ctx, w, h, imageFlags, data);
}

EMSCRIPTEN_KEEPALIVE void nvgw_updateImage(int image, const unsigned char* data) {
	nvgUpdateImage(g_ctx, image, data);
}

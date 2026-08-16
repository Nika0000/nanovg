#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <webgpu/webgpu.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifndef DEMO_ANTIALIAS
#define DEMO_ANTIALIAS 1
#endif
#ifndef DEMO_STENCIL_STROKES
#define DEMO_STENCIL_STROKES 1
#endif

#define NANOVG_WGPU_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_wgpu.h"

#include "demo.h"
#include "perf.h"

#include "wgpu_util.h"

static int blowup     = 0;
static bool resizeEvent = false;

static void errorcb(int error, const char* desc) { fprintf(stderr, "GLFW error %d: %s\n", error, desc); }

static void key(GLFWwindow* window, int key, int scancode, int action, int mods) {
	(void) scancode;
	(void) mods;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		blowup = !blowup;
}

static void framebufferSizeCb(GLFWwindow* window, int width, int height) {
	(void) window;
	(void) width;
	(void) height;
	resizeEvent = true;
}

// Acquire this frame's surface texture and begin the render pass nanovg draws into.
static WGPURenderPassEncoder prepareFrame(WGPUDevice device, WgpuFrame* frame, WGPUCommandEncoder* outEncoder, WGPUTexture* outSurfaceTexture) {
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(frame->surface, &surfaceTexture);
	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
	    surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
		resizeEvent = true;
		*outSurfaceTexture = NULL;
		return NULL;
	}

	WGPUTextureViewDescriptor viewDesc;
	memset(&viewDesc, 0, sizeof(viewDesc));
	viewDesc.format          = frame->colorFormat;
	viewDesc.dimension       = WGPUTextureViewDimension_2D;
	viewDesc.mipLevelCount   = 1;
	viewDesc.arrayLayerCount = 1;
	WGPUTextureView colorView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

	WGPUCommandEncoderDescriptor encDesc;
	memset(&encDesc, 0, sizeof(encDesc));
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encDesc);

	WGPURenderPassColorAttachment colorAttachment;
	memset(&colorAttachment, 0, sizeof(colorAttachment));
	colorAttachment.view       = colorView;
	colorAttachment.loadOp      = WGPULoadOp_Clear;
	colorAttachment.storeOp     = WGPUStoreOp_Store;
	colorAttachment.clearValue = (WGPUColor) {0.3, 0.3, 0.32, 1.0};
	colorAttachment.depthSlice  = WGPU_DEPTH_SLICE_UNDEFINED;

	WGPURenderPassDepthStencilAttachment depthAttachment;
	memset(&depthAttachment, 0, sizeof(depthAttachment));
	depthAttachment.view              = frame->depthStencilView;
	depthAttachment.depthLoadOp        = WGPULoadOp_Clear;
	depthAttachment.depthStoreOp       = WGPUStoreOp_Store;
	depthAttachment.depthClearValue    = 1.0f;
	depthAttachment.stencilLoadOp      = WGPULoadOp_Clear;
	depthAttachment.stencilStoreOp     = WGPUStoreOp_Store;
	depthAttachment.stencilClearValue = 0;

	WGPURenderPassDescriptor passDesc;
	memset(&passDesc, 0, sizeof(passDesc));
	passDesc.colorAttachmentCount   = 1;
	passDesc.colorAttachments        = &colorAttachment;
	passDesc.depthStencilAttachment = &depthAttachment;

	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
	wgpuTextureViewRelease(colorView);

	*outEncoder        = encoder;
	*outSurfaceTexture = surfaceTexture.texture;
	return pass;
}

static void submitFrame(WGPUDevice device, WGPUQueue queue, WgpuFrame* frame, WGPURenderPassEncoder pass, WGPUCommandEncoder encoder, WGPUTexture surfaceTexture) {
	(void) device;
	wgpuRenderPassEncoderEnd(pass);
	wgpuRenderPassEncoderRelease(pass);

	WGPUCommandBufferDescriptor cbDesc;
	memset(&cbDesc, 0, sizeof(cbDesc));
	WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(encoder, &cbDesc);
	wgpuCommandEncoderRelease(encoder);

	wgpuQueueSubmit(queue, 1, &cmdBuffer);
	wgpuCommandBufferRelease(cmdBuffer);

	wgpuTextureRelease(surfaceTexture);
	wgpuSurfacePresent(frame->surface);
}

int main(void) {
	if (!glfwInit()) {
		fprintf(stderr, "Failed to init GLFW.\n");
		return -1;
	}
	glfwSetErrorCallback(errorcb);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* window = glfwCreateWindow(1000, 600, "NanoVG Example WebGPU", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}
	glfwSetKeyCallback(window, key);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCb);
	glfwSetTime(0);

	WGPUInstanceDescriptor instanceDesc;
	memset(&instanceDesc, 0, sizeof(instanceDesc));
	WGPUInstance instance = wgpuCreateInstance(&instanceDesc);

	WgpuFrame frame;
	memset(&frame, 0, sizeof(frame));
	frame.surface             = wgpuutil_createSurface(instance, window);
	frame.colorFormat         = WGPUTextureFormat_BGRA8Unorm;
	frame.depthStencilFormat = WGPUTextureFormat_Depth24PlusStencil8;

	WGPUAdapter adapter = wgpuutil_requestAdapterSync(instance, frame.surface);
	if (!adapter) {
		fprintf(stderr, "No WebGPU adapter available.\n");
		return -1;
	}
	WGPUDevice device = wgpuutil_requestDeviceSync(instance, adapter);
	if (!device) {
		fprintf(stderr, "Failed to create WebGPU device.\n");
		return -1;
	}
	WGPUQueue queue = wgpuDeviceGetQueue(device);

	int winWidth, winHeight;
	glfwGetFramebufferSize(window, &winWidth, &winHeight);
	wgpuutil_configureSurface(device, &frame, (uint32_t) winWidth, (uint32_t) winHeight);

	WGPUNVGCreateInfo createInfo;
	memset(&createInfo, 0, sizeof(createInfo));
	createInfo.device              = device;
	createInfo.queue                = queue;
	createInfo.colorFormat         = frame.colorFormat;
	createInfo.depthStencilFormat = frame.depthStencilFormat;

	int flags = 0;
#ifndef NDEBUG
	flags |= NVG_DEBUG;
#endif
#if DEMO_ANTIALIAS
	flags |= NVG_ANTIALIAS;
#endif
#if DEMO_STENCIL_STROKES
	flags |= NVG_STENCIL_STROKES;
#endif

	NVGcontext* vg = nvgCreateWgpu(createInfo, flags);
	if (!vg) {
		fprintf(stderr, "Failed to create WebGPU nanovg context.\n");
		return -1;
	}

	DemoData data;
	PerfGraph fps;
	if (loadDemoData(vg, &data) == -1)
		return -1;
	initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");

	double prevt = glfwGetTime();

	while (!glfwWindowShouldClose(window)) {
		int cwinWidth, cwinHeight;
		glfwGetFramebufferSize(window, &cwinWidth, &cwinHeight);
		if (resizeEvent || (uint32_t) cwinWidth != frame.width || (uint32_t) cwinHeight != frame.height) {
			if (cwinWidth > 0 && cwinHeight > 0) {
				wgpuutil_configureSurface(device, &frame, (uint32_t) cwinWidth, (uint32_t) cwinHeight);
			}
			resizeEvent = false;
			glfwPollEvents();
			continue;
		}

		WGPUCommandEncoder encoder;
		WGPUTexture surfaceTexture;
		WGPURenderPassEncoder pass = prepareFrame(device, &frame, &encoder, &surfaceTexture);
		if (!pass) {
			glfwPollEvents();
			continue;
		}

		double t  = glfwGetTime();
		double dt = t - prevt;
		prevt      = t;
		updateGraph(&fps, (float) dt);

		double mx, my;
		glfwGetCursorPos(window, &mx, &my);

		int winW, winH;
		glfwGetWindowSize(window, &winW, &winH);
		float pxRatio = (float) frame.width / (float) winW;

		nvgWgpuBindRenderPass(vg, pass);
		nvgBeginFrame(vg, (float) winW, (float) winH, pxRatio);
		renderDemo(vg, mx, my, (float) winW, (float) winH, t, blowup, &data);
		renderGraph(vg, 5, 5, &fps);
		nvgEndFrame(vg);

		submitFrame(device, queue, &frame, pass, encoder, surfaceTexture);
		glfwPollEvents();
	}

	freeDemoData(vg, &data);
	nvgDeleteWgpu(vg);

	wgpuTextureViewRelease(frame.depthStencilView);
	wgpuTextureDestroy(frame.depthStencilTexture);
	wgpuTextureRelease(frame.depthStencilTexture);
	wgpuSurfaceUnconfigure(frame.surface);
	wgpuSurfaceRelease(frame.surface);
	wgpuQueueRelease(queue);
	wgpuDeviceRelease(device);
	wgpuAdapterRelease(adapter);
	wgpuInstanceRelease(instance);

	glfwDestroyWindow(window);
	glfwTerminate();

	printf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
	return 0;
}

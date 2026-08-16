// Example-only WebGPU/GLFW glue. Not part of the nanovg_wgpu.h backend contract.
#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <webgpu/webgpu.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#if defined(__APPLE__)
#import <QuartzCore/CAMetalLayer.h>
#endif

typedef struct WgpuAdapterRequest {
	WGPUAdapter adapter;
	bool done;
} WgpuAdapterRequest;

typedef struct WgpuDeviceRequest {
	WGPUDevice device;
	bool done;
} WgpuDeviceRequest;

static void wgpuutil_onAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2) {
	(void) userdata2;
	WgpuAdapterRequest* req = (WgpuAdapterRequest*) userdata1;
	if (status != WGPURequestAdapterStatus_Success) {
		fprintf(stderr, "wgpuInstanceRequestAdapter failed: %.*s\n", (int) message.length, message.data);
	}
	req->adapter = adapter;
	req->done     = true;
}

static void wgpuutil_onDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2) {
	(void) userdata2;
	WgpuDeviceRequest* req = (WgpuDeviceRequest*) userdata1;
	if (status != WGPURequestDeviceStatus_Success) {
		fprintf(stderr, "wgpuAdapterRequestDevice failed: %.*s\n", (int) message.length, message.data);
	}
	req->device = device;
	req->done    = true;
}

static void wgpuutil_onDeviceError(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
	(void) device;
	(void) type;
	(void) userdata1;
	(void) userdata2;
	fprintf(stderr, "WebGPU device error: %.*s\n", (int) message.length, message.data);
}

static WGPUAdapter wgpuutil_requestAdapterSync(WGPUInstance instance, WGPUSurface surface) {
	WgpuAdapterRequest req = {0};

	WGPURequestAdapterOptions options;
	memset(&options, 0, sizeof(options));
	options.compatibleSurface   = surface;
	options.powerPreference     = WGPUPowerPreference_HighPerformance;

	WGPURequestAdapterCallbackInfo cb;
	memset(&cb, 0, sizeof(cb));
	cb.mode      = WGPUCallbackMode_AllowSpontaneous;
	cb.callback  = wgpuutil_onAdapter;
	cb.userdata1 = &req;

	wgpuInstanceRequestAdapter(instance, &options, cb);
	while (!req.done) {
		wgpuInstanceProcessEvents(instance);
	}
	return req.adapter;
}

static WGPUDevice wgpuutil_requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter) {
	WgpuDeviceRequest req = {0};

	WGPUDeviceDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.uncapturedErrorCallbackInfo.callback = wgpuutil_onDeviceError;

	WGPURequestDeviceCallbackInfo cb;
	memset(&cb, 0, sizeof(cb));
	cb.mode      = WGPUCallbackMode_AllowSpontaneous;
	cb.callback  = wgpuutil_onDevice;
	cb.userdata1 = &req;

	wgpuAdapterRequestDevice(adapter, &desc, cb);
	while (!req.done) {
		wgpuInstanceProcessEvents(instance);
	}
	return req.device;
}

static WGPUSurface wgpuutil_createSurface(WGPUInstance instance, GLFWwindow* window) {
	WGPUSurfaceDescriptor desc;
	memset(&desc, 0, sizeof(desc));

#if defined(_WIN32)
	WGPUSurfaceSourceWindowsHWND fromHwnd;
	memset(&fromHwnd, 0, sizeof(fromHwnd));
	fromHwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
	fromHwnd.hinstance   = GetModuleHandle(NULL);
	fromHwnd.hwnd        = glfwGetWin32Window(window);
	desc.nextInChain      = &fromHwnd.chain;
	return wgpuInstanceCreateSurface(instance, &desc);
#elif defined(__APPLE__)
	NSWindow* nsWindow = glfwGetCocoaWindow(window);
	CAMetalLayer* layer = [CAMetalLayer layer];
	nsWindow.contentView.layer = layer;
	nsWindow.contentView.wantsLayer = YES;

	WGPUSurfaceSourceMetalLayer fromLayer;
	memset(&fromLayer, 0, sizeof(fromLayer));
	fromLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
	fromLayer.layer        = layer;
	desc.nextInChain        = &fromLayer.chain;
	return wgpuInstanceCreateSurface(instance, &desc);
#else
	WGPUSurfaceSourceXlibWindow fromXlib;
	memset(&fromXlib, 0, sizeof(fromXlib));
	fromXlib.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
	fromXlib.display      = glfwGetX11Display();
	fromXlib.window        = glfwGetX11Window(window);
	desc.nextInChain        = &fromXlib.chain;
	return wgpuInstanceCreateSurface(instance, &desc);
#endif
}

typedef struct WgpuFrame {
	WGPUSurface surface;
	WGPUTextureFormat colorFormat;

	WGPUTexture depthStencilTexture;
	WGPUTextureView depthStencilView;
	WGPUTextureFormat depthStencilFormat;

	uint32_t width, height;
} WgpuFrame;

static void wgpuutil_createDepthStencil(WGPUDevice device, WgpuFrame* frame) {
	if (frame->depthStencilView) {
		wgpuTextureViewRelease(frame->depthStencilView);
		wgpuTextureDestroy(frame->depthStencilTexture);
		wgpuTextureRelease(frame->depthStencilTexture);
	}

	WGPUTextureDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.usage         = WGPUTextureUsage_RenderAttachment;
	desc.dimension     = WGPUTextureDimension_2D;
	desc.size          = (WGPUExtent3D) {frame->width, frame->height, 1};
	desc.format        = frame->depthStencilFormat;
	desc.mipLevelCount = 1;
	desc.sampleCount   = 1;

	frame->depthStencilTexture = wgpuDeviceCreateTexture(device, &desc);

	WGPUTextureViewDescriptor viewDesc;
	memset(&viewDesc, 0, sizeof(viewDesc));
	viewDesc.format          = frame->depthStencilFormat;
	viewDesc.dimension       = WGPUTextureViewDimension_2D;
	viewDesc.mipLevelCount   = 1;
	viewDesc.arrayLayerCount = 1;
	frame->depthStencilView   = wgpuTextureCreateView(frame->depthStencilTexture, &viewDesc);
}

static void wgpuutil_configureSurface(WGPUDevice device, WgpuFrame* frame, uint32_t width, uint32_t height) {
	frame->width  = width;
	frame->height = height;

	WGPUSurfaceConfiguration config;
	memset(&config, 0, sizeof(config));
	config.device      = device;
	config.format       = frame->colorFormat;
	config.usage         = WGPUTextureUsage_RenderAttachment;
	config.width          = width;
	config.height         = height;
	config.presentMode   = WGPUPresentMode_Fifo;
	config.alphaMode     = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(frame->surface, &config);
	wgpuutil_createDepthStencil(device, frame);
}

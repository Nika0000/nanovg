/* Standalone visual example for NanoVG CPU filters (Windows / D3D11). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nanovg.h"
#define NANOVG_D3D11_IMPLEMENTATION
#include "nanovg_d3d11.h"
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define RELEASE(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
#define TILE_COUNT 9
static ID3D11Device* device;
static ID3D11DeviceContext* gpu;
static IDXGISwapChain* swap;
static ID3D11RenderTargetView* target;
static ID3D11Texture2D* depth;
static ID3D11DepthStencilView* stencil;
static NVGcontext* vg;
static int checkerImage;
static int images[TILE_COUNT], width = 1200, height = 900;
static int dirty = 1, pattern = 0, sourceW, sourceH;
static float strength = 1;
static unsigned char *photo, *testPixels;
static int photoW, photoH;
static double elapsedMs;
static const char* titles[TILE_COUNT] = {
    "Original", "Grayscale", "Brightness / contrast", "Box blur",
    "Gaussian blur", "Sharpen", "Unsharp mask", "Sobel edges", "Emboss kernel"
};

static int assetPath(char* path, size_t size, const char* name)
{
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)size);
    char* slash;
    if (!n || n >= size) return 0;
    slash = strrchr(path, '\\');
    if (!slash || (size_t)(slash + 1 - path) + strlen(name) + 1 > size) return 0;
    strcpy(slash + 1, name);
    return 1;
}

static int resizeTargets(void)
{
    ID3D11Texture2D* back = NULL;
    D3D11_TEXTURE2D_DESC desc;
    HRESULT hr;
    ID3D11DeviceContext_OMSetRenderTargets(gpu, 0, NULL, NULL);
    RELEASE(target); RELEASE(stencil); RELEASE(depth);
    hr = IDXGISwapChain_ResizeBuffers(swap, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return 0;
    hr = IDXGISwapChain_GetBuffer(swap, 0, &IID_ID3D11Texture2D, (void**)&back);
    if (FAILED(hr)) return 0;
    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back, NULL, &target);
    RELEASE(back);
    if (FAILED(hr)) return 0;
    memset(&desc, 0, sizeof(desc));
    desc.Width = width; desc.Height = height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &depth);
    if (FAILED(hr)) return 0;
    return SUCCEEDED(ID3D11Device_CreateDepthStencilView(device, (ID3D11Resource*)depth, NULL, &stencil));
}

static int initGraphics(HWND window)
{
    DXGI_SWAP_CHAIN_DESC desc;
    D3D_FEATURE_LEVEL level;
    HRESULT hr;
    memset(&desc, 0, sizeof(desc));
    desc.BufferDesc.Width = width; desc.BufferDesc.Height = height;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1; desc.OutputWindow = window; desc.Windowed = TRUE;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION, &desc, &swap, &device, &level, &gpu);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
            NULL, 0, D3D11_SDK_VERSION, &desc, &swap, &device, &level, &gpu);
    if (FAILED(hr) || !resizeTargets()) return 0;
    vg = nvgCreateD3D11(device, NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    return vg != NULL;
}

static int loadAssets(void)
{
    char path[MAX_PATH];
    int channels, x, y;
    unsigned char checker[24 * 24 * 4];
    if (!assetPath(path, sizeof(path), "filter-assets/Roboto-Regular.ttf") ||
        nvgCreateFont(vg, "sans", path) < 0) return 0;
    if (!assetPath(path, sizeof(path), "filter-assets/image1.jpg")) return 0;
    for (y = 0; y < 24; ++y) for (x = 0; x < 24; ++x) {
        unsigned char* p = checker + (y * 24 + x) * 4;
        int light = ((x / 12 + y / 12) & 1);
        p[0] = light ? 65 : 43; p[1] = light ? 74 : 52;
        p[2] = light ? 88 : 67; p[3] = 255;
    }
    checkerImage = nvgCreateImageRGBA(vg, 24, 24,
        NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_NEAREST, checker);
    if (!checkerImage) return 0;
    photo = stbi_load(path, &photoW, &photoH, &channels, 4);
    if (!photo) return 0;
    testPixels = (unsigned char*)malloc(320 * 200 * 4);
    if (!testPixels) return 0;
    for (y = 0; y < 200; ++y) for (x = 0; x < 320; ++x) {
        unsigned char* p = testPixels + (y * 320 + x) * 4;
        float dx = x - 160.0f, dy = y - 100.0f;
        p[0] = (unsigned char)(x * 255 / 319);
        p[1] = (unsigned char)(y * 255 / 199);
        p[2] = ((x / 12 + y / 12) & 1) ? 240 : 35;
        p[3] = dx * dx / (145 * 145) + dy * dy / (85 * 85) < 1 ? 255 : 0;
        if (x > 60 && x < 110 && y > 50 && y < 150) {
            p[0] = 255; p[1] = 205; p[2] = 35; p[3] = 128;
        }
    }
    return 1;
}

/* Regenerate only when a key changes the source or strength, before a frame. */
static int rebuildImages(void)
{
    const float emboss[] = {-2,-1,0, -1,1,1, 0,1,2};
    NVGpixelBuffer source = {0};
    NVGfilter filters[TILE_COUNT];
    int fresh[TILE_COUNT] = {0}, i;
    LARGE_INTEGER start, end, frequency;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    source.data = pattern ? testPixels : photo;
    source.width = pattern ? 320 : photoW;
    source.height = pattern ? 200 : photoH;
    sourceW = source.width; sourceH = source.height;
    filters[1] = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    filters[1].amount = fminf(1, strength);
    filters[2] = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    filters[2].brightness = .08f * strength; filters[2].contrast = 1 + .35f * strength;
    filters[3] = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    filters[3].radiusX = filters[3].radiusY = (int)(4 * strength);
    filters[4] = nvgFilterInit(NVG_FILTER_GAUSSIAN_BLUR);
    filters[4].sigma = 2.5f * strength;
    filters[5] = nvgFilterInit(NVG_FILTER_SHARPEN);
    filters[5].amount = .65f * strength;
    filters[6] = nvgFilterInit(NVG_FILTER_UNSHARP_MASK);
    filters[6].sigma = 2; filters[6].amount = 1.5f * strength;
    filters[7] = nvgFilterInit(NVG_FILTER_SOBEL);
    filters[7].amount = strength;
    filters[8] = nvgFilterInit(NVG_FILTER_CONVOLUTION);
    filters[8].kernel = emboss; filters[8].kernelWidth = filters[8].kernelHeight = 3;
    filters[8].divisor = 1 / strength; filters[8].bias = .5f;
    for (i = 0; i < TILE_COUNT; ++i) {
        NVGfilterStatus status = nvgCreateFilteredImageRGBA(vg, &source, 0,
            i ? filters + i : NULL, i ? 1 : 0, fresh + i);
        if (status != NVG_FILTER_OK) {
            fprintf(stderr, "Filter '%s' failed: %d\n", titles[i], status);
            for (i = 0; i < TILE_COUNT; ++i) if (fresh[i]) nvgDeleteImage(vg, fresh[i]);
            return 0;
        }
    }
    for (i = 0; i < TILE_COUNT; ++i) {
        if (images[i]) nvgDeleteImage(vg, images[i]);
        images[i] = fresh[i];
    }
    QueryPerformanceCounter(&end);
    elapsedMs = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
    dirty = 0;
    return 1;
}

static void rect(float x, float y, float w, float h, NVGcolor color)
{
    nvgBeginPath(vg); nvgRect(vg, x, y, w, h); nvgFillColor(vg, color); nvgFill(vg);
}

static void label(float x, float y, float size, const char* text, NVGcolor color)
{
    nvgFontFace(vg, "sans"); nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, color); nvgText(vg, x, y, text, NULL);
}

static void draw(void)
{
    D3D11_VIEWPORT viewport = {0};
    const float clear[] = {.047f, .063f, .09f, 1};
    float scale = fminf(width / 1200.0f, height / 900.0f);
    int i;
    char info[180];
    viewport.Width = (float)width; viewport.Height = (float)height; viewport.MaxDepth = 1;
    ID3D11DeviceContext_OMSetRenderTargets(gpu, 1, &target, stencil);
    ID3D11DeviceContext_RSSetViewports(gpu, 1, &viewport);
    ID3D11DeviceContext_ClearRenderTargetView(gpu, target, clear);
    ID3D11DeviceContext_ClearDepthStencilView(gpu, stencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    nvgBeginFrame(vg, (float)width, (float)height, 1);
    nvgTranslate(vg, (width - 1200 * scale) * .5f, (height - 900 * scale) * .5f);
    nvgScale(vg, scale, scale);
    label(28, 20, 28, "NanoVG / Image filters", nvgRGB(237,243,255));
    label(28, 58, 15, "Up / Down: strength    Space: photo / transparency pattern    R: reset    Esc: close",
          nvgRGB(153,173,201));
    snprintf(info, sizeof(info), "Strength %.2fx   |   %s %dx%d   |   Generated in %.1f ms (cached)",
        strength, pattern ? "Pattern" : "Photo", sourceW, sourceH, elapsedMs);
    label(28, 82, 14, info, nvgRGB(89,211,177));
    for (i = 0; i < TILE_COUNT; ++i) {
        float x = 28 + (i % 3) * 388.0f, y = 119 + (i / 3) * 253.0f;
        float boxW = 348, boxH = 190, imageScale, iw, ih, ix, iy;

        rect(x, y, 368, 237, nvgRGB(25,34,49));
        label(x + 12, y + 10, 17, titles[i], nvgRGB(237,243,255));
        imageScale = fminf(boxW / sourceW, boxH / sourceH);
        iw = sourceW * imageScale; ih = sourceH * imageScale;
        ix = x + 10 + (boxW - iw) * .5f; iy = y + 36 + (boxH - ih) * .5f;
        nvgSave(vg);
        nvgBeginPath(vg); nvgRect(vg, ix, iy, iw, ih);
        nvgFillPaint(vg, nvgImagePattern(vg, ix, iy, 24, 24, 0, checkerImage, 1));
        nvgFill(vg);
        nvgBeginPath(vg); nvgRect(vg, ix, iy, iw, ih);
        nvgFillPaint(vg, nvgImagePattern(vg, ix, iy, iw, ih, 0, images[i], 1));
        nvgFill(vg); nvgRestore(vg);
    }
    nvgEndFrame(vg);
}

/* Read the actual rendered frame for an unattended visual smoke test. */
static int savePreview(void)
{
    ID3D11Texture2D *back = NULL, *readback = NULL;
    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    int ok = 0;
    if (FAILED(IDXGISwapChain_GetBuffer(swap, 0, &IID_ID3D11Texture2D, (void**)&back))) return 0;
    ID3D11Texture2D_GetDesc(back, &desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    if (SUCCEEDED(ID3D11Device_CreateTexture2D(device, &desc, NULL, &readback))) {
        ID3D11DeviceContext_CopyResource(gpu, (ID3D11Resource*)readback, (ID3D11Resource*)back);
        if (SUCCEEDED(ID3D11DeviceContext_Map(gpu, (ID3D11Resource*)readback, 0, D3D11_MAP_READ, 0, &mapped))) {
            ok = stbi_write_png("filters-preview.png", width, height, 4, mapped.pData, mapped.RowPitch);
            ID3D11DeviceContext_Unmap(gpu, (ID3D11Resource*)readback, 0);
        }
    }
    RELEASE(readback); RELEASE(back);
    return ok;
}

static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM key, LPARAM value)
{
    switch (message) {
    case WM_SIZE:
        if (key != SIZE_MINIMIZED) {
            width = LOWORD(value); height = HIWORD(value);
            if (swap && !resizeTargets()) PostQuitMessage(1);
        }
        return 0;
    case WM_KEYDOWN:
        if (key == VK_ESCAPE) { DestroyWindow(window); return 0; }
        if (key == VK_SPACE) { pattern = !pattern; dirty = 1; }
        if (key == VK_UP) { strength = fminf(3, strength + .25f); dirty = 1; }
        if (key == VK_DOWN) { strength = fmaxf(.25f, strength - .25f); dirty = 1; }
        if (key == 'R') { strength = 1; dirty = 1; }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcA(window, message, key, value);
    }
}

int main(int argc, char** argv)
{
    WNDCLASSA wc = {0};
    RECT area = {0, 0, 1200, 900};
    HWND window = NULL;
    MSG msg = {0};
    int i, code = 1, smoke = argc > 1 && strcmp(argv[1], "--smoke-test") == 0;
    if (argc > 2 && strcmp(argv[2], "--pattern") == 0) pattern = 1;
    SetProcessDPIAware();
    wc.lpfnWndProc = windowProc; wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "NanoVGFilterDemo"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) goto cleanup;
    AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
    window = CreateWindowA(wc.lpszClassName, "NanoVG - Image filter comparison",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        area.right - area.left, area.bottom - area.top, NULL, NULL, wc.hInstance, NULL);
    if (!window || !initGraphics(window) || !loadAssets() || !rebuildImages()) {
        fprintf(stderr, "Could not initialize filter demo. Check D3D11 and filter-assets beside the executable.\n");
        goto cleanup;
    }
    if (smoke) {
        draw(); code = savePreview() ? 0 : 1;
        printf("Visual smoke test: %s\n", code ? "FAILED" : "filters-preview.png written");
        goto cleanup;
    }
    ShowWindow(window, SW_SHOW);
    puts("Up/Down: strength | Space: source | R: reset | Esc: close");
    code = 0;
    for (;;) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { code = (int)msg.wParam; break; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        } else if (IsIconic(window)) {
            WaitMessage();
        } else {
            if (dirty && !rebuildImages()) { code = 1; break; }
            draw();
            if (FAILED(IDXGISwapChain_Present(swap, 1, 0))) { code = 1; break; }
        }
    }
cleanup:
    if (vg) {
        for (i = 0; i < TILE_COUNT; ++i) if (images[i]) nvgDeleteImage(vg, images[i]);
        if (checkerImage) nvgDeleteImage(vg, checkerImage);
        nvgDeleteD3D11(vg);
    }
    stbi_image_free(photo); free(testPixels);
    if (gpu) ID3D11DeviceContext_ClearState(gpu);
    RELEASE(target); RELEASE(stencil); RELEASE(depth);
    RELEASE(swap); RELEASE(gpu); RELEASE(device);
    if (window && IsWindow(window)) DestroyWindow(window);
    return code;
}




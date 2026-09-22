#include <GLES3/gl3.h>
#include "nanovg.h"
#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "stb_image.h"

void draw(NVGcontext* vg, float width, float height, float time);
static NVGcontext* vg;
static int preloadedImage;
static unsigned char* imagePixels;
static int imageW, imageH;

extern "C" {

void preview_init() {
    vg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
}

void preview_frame(float width, float height, float ratio, float time) {
    glViewport(0, 0, (int)(width * ratio), (int)(height * ratio));
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(vg, width, height, ratio);
    draw(vg, width, height, time);
    nvgEndFrame(vg);
}

void preview_load_font(const char* name, unsigned char* data, int len) {
    nvgCreateFontMem(vg, name, data, len, 0);
}

void preview_load_image(unsigned char* data, int len) {
    int n;
    imagePixels = stbi_load_from_memory(data, len, &imageW, &imageH, &n, 4);
    if (imagePixels)
        preloadedImage = nvgCreateImageRGBA(vg, imageW, imageH, 0, imagePixels);
}

int preview_get_image() {
    return preloadedImage;
}

unsigned char* preview_get_image_pixels() {
    return imagePixels;
}

int preview_get_image_width() {
    return imageW;
}

int preview_get_image_height() {
    return imageH;
}

}

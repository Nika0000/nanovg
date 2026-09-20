// nanovg unit tests — no GPU context required
// Tests color utilities, transform math, CPU image filters, and null-backend context creation.

#include "nanovg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NVG_PI 3.14159265358979323846264338327f

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name)                                                                                                     \
    static void test_##name(void);                                                                                     \
    static void run_##name(void) {                                                                                     \
        g_tests_run++;                                                                                                 \
        test_##name();                                                                                                 \
    }                                                                                                                  \
    static void test_##name(void)

#define ASSERT_TRUE(expr)                                                                                              \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                          \
            g_tests_failed++;                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_FLOAT_EQ(a, b)                                                                                          \
    do {                                                                                                               \
        float _a = (a), _b = (b);                                                                                      \
        if (fabsf(_a - _b) > 1e-5f) {                                                                                  \
            fprintf(stderr, "  FAIL %s:%d: %.8f != %.8f\n", __FILE__, __LINE__, (double)_a, (double)_b);               \
            g_tests_failed++;                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_INT_EQ(a, b)                                                                                            \
    do {                                                                                                               \
        int _a = (a), _b = (b);                                                                                        \
        if (_a != _b) {                                                                                                \
            fprintf(stderr, "  FAIL %s:%d: %d != %d\n", __FILE__, __LINE__, _a, _b);                                   \
            g_tests_failed++;                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_BYTE_EQ(a, b)                                                                                           \
    do {                                                                                                               \
        unsigned char _a = (unsigned char)(a), _b = (unsigned char)(b);                                                \
        if (_a != _b) {                                                                                                \
            fprintf(stderr, "  FAIL %s:%d: 0x%02x != 0x%02x\n", __FILE__, __LINE__, _a, _b);                           \
            g_tests_failed++;                                                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define PASS() g_tests_passed++

// ---------------------------------------------------------------------------
// Color utilities
// ---------------------------------------------------------------------------

TEST(rgb) {
    NVGcolor c = nvgRGB(255, 128, 0);
    ASSERT_FLOAT_EQ(c.r, 1.0f);
    ASSERT_FLOAT_EQ(c.g, 128.0f / 255.0f);
    ASSERT_FLOAT_EQ(c.b, 0.0f);
    ASSERT_FLOAT_EQ(c.a, 1.0f);
    PASS();
}

TEST(rgba) {
    NVGcolor c = nvgRGBA(0, 0, 0, 128);
    ASSERT_FLOAT_EQ(c.r, 0.0f);
    ASSERT_FLOAT_EQ(c.a, 128.0f / 255.0f);
    PASS();
}

TEST(rgbf) {
    NVGcolor c = nvgRGBf(0.5f, 0.25f, 0.75f);
    ASSERT_FLOAT_EQ(c.r, 0.5f);
    ASSERT_FLOAT_EQ(c.g, 0.25f);
    ASSERT_FLOAT_EQ(c.b, 0.75f);
    ASSERT_FLOAT_EQ(c.a, 1.0f);
    PASS();
}

TEST(rgbaf) {
    NVGcolor c = nvgRGBAf(0.1f, 0.2f, 0.3f, 0.4f);
    ASSERT_FLOAT_EQ(c.r, 0.1f);
    ASSERT_FLOAT_EQ(c.g, 0.2f);
    ASSERT_FLOAT_EQ(c.b, 0.3f);
    ASSERT_FLOAT_EQ(c.a, 0.4f);
    PASS();
}

TEST(lerp_rgba) {
    NVGcolor a = nvgRGBAf(0.0f, 0.0f, 0.0f, 0.0f);
    NVGcolor b = nvgRGBAf(1.0f, 1.0f, 1.0f, 1.0f);
    NVGcolor m = nvgLerpRGBA(a, b, 0.5f);
    ASSERT_FLOAT_EQ(m.r, 0.5f);
    ASSERT_FLOAT_EQ(m.g, 0.5f);
    ASSERT_FLOAT_EQ(m.b, 0.5f);
    ASSERT_FLOAT_EQ(m.a, 0.5f);
    PASS();
}

TEST(lerp_rgba_boundaries) {
    NVGcolor a = nvgRGBAf(0.2f, 0.4f, 0.6f, 0.8f);
    NVGcolor b = nvgRGBAf(0.8f, 0.6f, 0.4f, 0.2f);
    NVGcolor c0 = nvgLerpRGBA(a, b, 0.0f);
    ASSERT_FLOAT_EQ(c0.r, 0.2f);
    ASSERT_FLOAT_EQ(c0.a, 0.8f);
    NVGcolor c1 = nvgLerpRGBA(a, b, 1.0f);
    ASSERT_FLOAT_EQ(c1.r, 0.8f);
    ASSERT_FLOAT_EQ(c1.a, 0.2f);
    PASS();
}

TEST(trans_rgba) {
    NVGcolor c = nvgRGB(255, 0, 0);
    NVGcolor t = nvgTransRGBA(c, 128);
    ASSERT_FLOAT_EQ(t.r, 1.0f);
    ASSERT_FLOAT_EQ(t.a, 128.0f / 255.0f);
    PASS();
}

TEST(trans_rgbaf) {
    NVGcolor c = nvgRGBf(0.0f, 1.0f, 0.0f);
    NVGcolor t = nvgTransRGBAf(c, 0.25f);
    ASSERT_FLOAT_EQ(t.g, 1.0f);
    ASSERT_FLOAT_EQ(t.a, 0.25f);
    PASS();
}

TEST(hsl_red) {
    NVGcolor c = nvgHSL(0.0f, 1.0f, 0.5f);
    ASSERT_TRUE(c.r > 0.95f);
    ASSERT_TRUE(c.g < 0.05f);
    ASSERT_TRUE(c.b < 0.05f);
    ASSERT_FLOAT_EQ(c.a, 1.0f);
    PASS();
}

TEST(hsl_green) {
    NVGcolor c = nvgHSL(1.0f / 3.0f, 1.0f, 0.5f);
    ASSERT_TRUE(c.g > 0.95f);
    ASSERT_TRUE(c.r < 0.05f);
    ASSERT_TRUE(c.b < 0.05f);
    PASS();
}

TEST(hsla) {
    NVGcolor c = nvgHSLA(0.0f, 0.0f, 1.0f, 128);
    ASSERT_TRUE(c.r > 0.95f);
    ASSERT_TRUE(c.g > 0.95f);
    ASSERT_TRUE(c.b > 0.95f);
    ASSERT_FLOAT_EQ(c.a, 128.0f / 255.0f);
    PASS();
}

// ---------------------------------------------------------------------------
// Angle conversion
// ---------------------------------------------------------------------------

TEST(deg_to_rad) {
    ASSERT_FLOAT_EQ(nvgDegToRad(0.0f), 0.0f);
    ASSERT_FLOAT_EQ(nvgDegToRad(180.0f), NVG_PI);
    ASSERT_FLOAT_EQ(nvgDegToRad(360.0f), 2.0f * NVG_PI);
    ASSERT_FLOAT_EQ(nvgDegToRad(90.0f), NVG_PI / 2.0f);
    PASS();
}

TEST(rad_to_deg) {
    ASSERT_FLOAT_EQ(nvgRadToDeg(0.0f), 0.0f);
    ASSERT_FLOAT_EQ(nvgRadToDeg(NVG_PI), 180.0f);
    ASSERT_FLOAT_EQ(nvgRadToDeg(2.0f * NVG_PI), 360.0f);
    PASS();
}

TEST(deg_rad_roundtrip) {
    float deg = 45.0f;
    ASSERT_FLOAT_EQ(nvgRadToDeg(nvgDegToRad(deg)), deg);
    PASS();
}

// ---------------------------------------------------------------------------
// Transform math
// ---------------------------------------------------------------------------

TEST(transform_identity) {
    float t[6];
    nvgTransformIdentity(t);
    ASSERT_FLOAT_EQ(t[0], 1.0f);
    ASSERT_FLOAT_EQ(t[1], 0.0f);
    ASSERT_FLOAT_EQ(t[2], 0.0f);
    ASSERT_FLOAT_EQ(t[3], 1.0f);
    ASSERT_FLOAT_EQ(t[4], 0.0f);
    ASSERT_FLOAT_EQ(t[5], 0.0f);
    PASS();
}

TEST(transform_translate) {
    float t[6];
    nvgTransformTranslate(t, 10.0f, 20.0f);
    ASSERT_FLOAT_EQ(t[0], 1.0f);
    ASSERT_FLOAT_EQ(t[3], 1.0f);
    ASSERT_FLOAT_EQ(t[4], 10.0f);
    ASSERT_FLOAT_EQ(t[5], 20.0f);
    PASS();
}

TEST(transform_scale) {
    float t[6];
    nvgTransformScale(t, 2.0f, 3.0f);
    ASSERT_FLOAT_EQ(t[0], 2.0f);
    ASSERT_FLOAT_EQ(t[3], 3.0f);
    ASSERT_FLOAT_EQ(t[4], 0.0f);
    ASSERT_FLOAT_EQ(t[5], 0.0f);
    PASS();
}

TEST(transform_rotate) {
    float t[6];
    nvgTransformRotate(t, NVG_PI / 2.0f);
    ASSERT_FLOAT_EQ(t[0], cosf(NVG_PI / 2.0f));
    ASSERT_FLOAT_EQ(t[1], sinf(NVG_PI / 2.0f));
    ASSERT_FLOAT_EQ(t[2], -sinf(NVG_PI / 2.0f));
    ASSERT_FLOAT_EQ(t[3], cosf(NVG_PI / 2.0f));
    PASS();
}

TEST(transform_point) {
    float t[6];
    nvgTransformTranslate(t, 5.0f, 10.0f);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 1.0f, 2.0f);
    ASSERT_FLOAT_EQ(dx, 6.0f);
    ASSERT_FLOAT_EQ(dy, 12.0f);
    PASS();
}

TEST(transform_point_scale) {
    float t[6];
    nvgTransformScale(t, 3.0f, 4.0f);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 2.0f, 5.0f);
    ASSERT_FLOAT_EQ(dx, 6.0f);
    ASSERT_FLOAT_EQ(dy, 20.0f);
    PASS();
}

TEST(transform_multiply_identity) {
    float a[6], b[6];
    nvgTransformTranslate(a, 3.0f, 7.0f);
    nvgTransformIdentity(b);
    nvgTransformMultiply(a, b);
    ASSERT_FLOAT_EQ(a[4], 3.0f);
    ASSERT_FLOAT_EQ(a[5], 7.0f);
    PASS();
}

TEST(transform_inverse) {
    float t[6], inv[6];
    nvgTransformTranslate(t, 10.0f, 20.0f);
    int ok = nvgTransformInverse(inv, t);
    ASSERT_TRUE(ok);
    ASSERT_FLOAT_EQ(inv[4], -10.0f);
    ASSERT_FLOAT_EQ(inv[5], -20.0f);
    PASS();
}

TEST(transform_inverse_roundtrip) {
    float t[6], inv[6];
    nvgTransformScale(t, 2.0f, 0.5f);
    t[4] = 10.0f;
    t[5] = -5.0f;
    int ok = nvgTransformInverse(inv, t);
    ASSERT_TRUE(ok);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 7.0f, 3.0f);
    float rx, ry;
    nvgTransformPoint(&rx, &ry, inv, dx, dy);
    ASSERT_FLOAT_EQ(rx, 7.0f);
    ASSERT_FLOAT_EQ(ry, 3.0f);
    PASS();
}

TEST(transform_skew_x) {
    float t[6];
    nvgTransformSkewX(t, 0.0f);
    ASSERT_FLOAT_EQ(t[0], 1.0f);
    ASSERT_FLOAT_EQ(t[2], 0.0f);
    ASSERT_FLOAT_EQ(t[3], 1.0f);
    PASS();
}

TEST(transform_skew_y) {
    float t[6];
    nvgTransformSkewY(t, 0.0f);
    ASSERT_FLOAT_EQ(t[0], 1.0f);
    ASSERT_FLOAT_EQ(t[1], 0.0f);
    ASSERT_FLOAT_EQ(t[3], 1.0f);
    PASS();
}

// ---------------------------------------------------------------------------
// CPU image filters
// ---------------------------------------------------------------------------

static void make_solid_buffer(NVGpixelBuffer* buf, int w, int h, unsigned char r, unsigned char g, unsigned char b,
                              unsigned char a) {
    buf->width          = w;
    buf->height         = h;
    buf->stride         = w * 4;
    buf->premultiplied  = 0;
    buf->data           = (unsigned char*)malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) {
        buf->data[i * 4 + 0] = r;
        buf->data[i * 4 + 1] = g;
        buf->data[i * 4 + 2] = b;
        buf->data[i * 4 + 3] = a;
    }
}

static void free_buffer(NVGpixelBuffer* buf) {
    free(buf->data);
    buf->data = NULL;
}

TEST(filter_init_defaults) {
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    ASSERT_INT_EQ(f.type, NVG_FILTER_GRAYSCALE);
    ASSERT_INT_EQ(f.border, NVG_FILTER_BORDER_CLAMP);
    ASSERT_FLOAT_EQ(f.amount, 1.0f);
    ASSERT_FLOAT_EQ(f.contrast, 1.0f);
    PASS();
}

TEST(filter_zero_count_copies) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 200, 100, 50, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, NULL, 0);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 200);
    ASSERT_BYTE_EQ(dst.data[1], 100);
    ASSERT_BYTE_EQ(dst.data[2], 50);
    ASSERT_BYTE_EQ(dst.data[3], 255);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_grayscale_full) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 255, 0, 0, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    // Full grayscale: all channels should be equal
    ASSERT_BYTE_EQ(dst.data[0], dst.data[1]);
    ASSERT_BYTE_EQ(dst.data[1], dst.data[2]);
    ASSERT_BYTE_EQ(dst.data[3], 255);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_grayscale_zero_is_passthrough) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 100, 150, 200, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    f.amount    = 0.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 100);
    ASSERT_BYTE_EQ(dst.data[1], 150);
    ASSERT_BYTE_EQ(dst.data[2], 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_brightness_contrast_identity) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 128, 64, 200, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f  = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    f.brightness = 0.0f;
    f.contrast   = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 128);
    ASSERT_BYTE_EQ(dst.data[1], 64);
    ASSERT_BYTE_EQ(dst.data[2], 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_brightness_increase) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 100, 100, 100, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f  = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    f.brightness = 0.5f;
    f.contrast   = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_TRUE(dst.data[0] > 100);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_box_blur) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 8, 8, 100, 100, 100, 255);
    // Set center pixel bright
    src.data[(4 * 8 + 4) * 4 + 0] = 255;
    src.data[(4 * 8 + 4) * 4 + 1] = 255;
    src.data[(4 * 8 + 4) * 4 + 2] = 255;
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    f.radiusX   = 1;
    f.radiusY   = 1;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    // Center should be dimmer after averaging with neighbors
    unsigned char center = dst.data[(4 * 8 + 4) * 4];
    ASSERT_TRUE(center < 255);
    ASSERT_TRUE(center > 100);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_null_source) {
    NVGpixelBuffer dst;
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f       = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    NVGfilterStatus s = nvgFilterRGBA(NULL, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&dst);
    PASS();
}

TEST(filter_null_destination) {
    NVGpixelBuffer src;
    make_solid_buffer(&src, 2, 2, 128, 128, 128, 255);
    NVGfilter f       = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    NVGfilterStatus s = nvgFilterRGBA(&src, NULL, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&src);
    PASS();
}

TEST(filter_chain_grayscale_then_brightness) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 200, 100, 50, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter chain[2];
    chain[0]            = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    chain[0].amount     = 1.0f;
    chain[1]            = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    chain[1].brightness = 0.2f;
    chain[1].contrast   = 1.0f;
    NVGfilterStatus s   = nvgFilterRGBA(&src, &dst, chain, 2);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    // After grayscale, R==G==B; after brightness increase, should be brighter than pure grayscale
    ASSERT_BYTE_EQ(dst.data[0], dst.data[1]);
    ASSERT_BYTE_EQ(dst.data[1], dst.data[2]);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_gaussian_blur) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 8, 8, 100, 100, 100, 255);
    src.data[(4 * 8 + 4) * 4 + 0] = 255;
    src.data[(4 * 8 + 4) * 4 + 1] = 255;
    src.data[(4 * 8 + 4) * 4 + 2] = 255;
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GAUSSIAN_BLUR);
    f.sigma     = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    unsigned char center = dst.data[(4 * 8 + 4) * 4];
    ASSERT_TRUE(center < 255);
    ASSERT_TRUE(center > 100);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_sharpen) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 8, 8, 128, 128, 128, 255);
    src.data[(4 * 8 + 4) * 4 + 0] = 200;
    src.data[(4 * 8 + 4) * 4 + 1] = 200;
    src.data[(4 * 8 + 4) * 4 + 2] = 200;
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_SHARPEN);
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    unsigned char center = dst.data[(4 * 8 + 4) * 4];
    ASSERT_TRUE(center > 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_sobel) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 8, 8, 0, 0, 0, 255);
    for (int y = 0; y < 8; y++)
        for (int x = 4; x < 8; x++) {
            int idx         = (y * 8 + x) * 4;
            src.data[idx]   = 255;
            src.data[idx+1] = 255;
            src.data[idx+2] = 255;
        }
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_SOBEL);
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    unsigned char edge = dst.data[(3 * 8 + 4) * 4];
    ASSERT_TRUE(edge > 0);
    unsigned char flat = dst.data[(3 * 8 + 0) * 4];
    ASSERT_TRUE(edge > flat);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_unsharp_mask) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 8, 8, 128, 128, 128, 255);
    src.data[(4 * 8 + 4) * 4 + 0] = 200;
    src.data[(4 * 8 + 4) * 4 + 1] = 200;
    src.data[(4 * 8 + 4) * 4 + 2] = 200;
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_UNSHARP_MASK);
    f.sigma     = 1.0f;
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_convolution_identity) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 100, 150, 200, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    float kernel[9] = {0,0,0, 0,1,0, 0,0,0};
    NVGfilter f      = nvgFilterInit(NVG_FILTER_CONVOLUTION);
    f.kernelWidth    = 3;
    f.kernelHeight   = 3;
    f.kernel         = kernel;
    f.divisor        = 1.0f;
    f.bias           = 0.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+0], 100);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+1], 150);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+2], 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_convolution_invalid_even_kernel) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 100, 100, 100, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    float kernel[4] = {1,1,1,1};
    NVGfilter f      = nvgFilterInit(NVG_FILTER_CONVOLUTION);
    f.kernelWidth    = 2;
    f.kernelHeight   = 2;
    f.kernel         = kernel;
    f.divisor        = 1.0f;
    f.bias           = 0.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_dimension_mismatch) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 128, 128, 128, 255);
    make_solid_buffer(&dst, 8, 8, 0, 0, 0, 0);
    NVGfilter f       = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_too_many_stages) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 128, 128, 128, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, NULL, 257);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_border_repeat) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 200, 100, 50, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    f.border    = NVG_FILTER_BORDER_REPEAT;
    f.radiusX   = 1;
    f.radiusY   = 1;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_border_mirror) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 150, 150, 150, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    f.border    = NVG_FILTER_BORDER_MIRROR;
    f.radiusX   = 1;
    f.radiusY   = 1;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 150);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_border_transparent) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 200, 200, 200, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    f.border    = NVG_FILTER_BORDER_TRANSPARENT;
    f.radiusX   = 1;
    f.radiusY   = 1;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    // Corner pixel alpha should be reduced (averaged with transparent outside)
    ASSERT_TRUE(dst.data[3] < 255);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_inplace_alias) {
    NVGpixelBuffer buf;
    make_solid_buffer(&buf, 4, 4, 255, 0, 0, 255);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&buf, &buf, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(buf.data[0], buf.data[1]);
    ASSERT_BYTE_EQ(buf.data[1], buf.data[2]);
    free_buffer(&buf);
    PASS();
}

TEST(filter_premultiplied_conversion) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 200, 100, 50, 128);
    src.premultiplied = 0;
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    dst.premultiplied = 1;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, NULL, 0);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[3], 128);
    ASSERT_TRUE(dst.data[0] < 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_zero_width_buffer) {
    NVGpixelBuffer src, dst;
    src.width = 0; src.height = 4; src.stride = 0; src.premultiplied = 0;
    src.data = (unsigned char*)malloc(1);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter f       = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free(src.data);
    free_buffer(&dst);
    PASS();
}

TEST(filter_null_data_buffer) {
    NVGpixelBuffer src, dst;
    src.width = 2; src.height = 2; src.stride = 8; src.premultiplied = 0;
    src.data = NULL;
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f       = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&dst);
    PASS();
}

TEST(filter_negative_count) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 128, 128, 128, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, NULL, -1);
    ASSERT_INT_EQ(s, NVG_FILTER_INVALID_ARGUMENT);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_brightness_decrease) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 200, 200, 200, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f  = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    f.brightness = -0.5f;
    f.contrast   = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_TRUE(dst.data[0] < 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_contrast_increase) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 160, 160, 160, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f  = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    f.brightness = 0.0f;
    f.contrast   = 2.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_TRUE(dst.data[0] > 160);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_contrast_zero) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 200, 50, 100, 255);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f  = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    f.brightness = 0.0f;
    f.contrast   = 0.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[0], 128);
    ASSERT_BYTE_EQ(dst.data[1], 128);
    ASSERT_BYTE_EQ(dst.data[2], 128);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_init_all_types) {
    NVGfilter f;
    f = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    ASSERT_INT_EQ(f.type, NVG_FILTER_BRIGHTNESS_CONTRAST);
    ASSERT_FLOAT_EQ(f.contrast, 1.0f);

    f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    ASSERT_INT_EQ(f.type, NVG_FILTER_BOX_BLUR);
    ASSERT_INT_EQ(f.radiusX, 1);
    ASSERT_INT_EQ(f.radiusY, 1);

    f = nvgFilterInit(NVG_FILTER_GAUSSIAN_BLUR);
    ASSERT_INT_EQ(f.type, NVG_FILTER_GAUSSIAN_BLUR);
    ASSERT_FLOAT_EQ(f.sigma, 1.0f);

    f = nvgFilterInit(NVG_FILTER_SHARPEN);
    ASSERT_INT_EQ(f.type, NVG_FILTER_SHARPEN);

    f = nvgFilterInit(NVG_FILTER_UNSHARP_MASK);
    ASSERT_INT_EQ(f.type, NVG_FILTER_UNSHARP_MASK);

    f = nvgFilterInit(NVG_FILTER_SOBEL);
    ASSERT_INT_EQ(f.type, NVG_FILTER_SOBEL);

    f = nvgFilterInit(NVG_FILTER_CONVOLUTION);
    ASSERT_INT_EQ(f.type, NVG_FILTER_CONVOLUTION);
    ASSERT_FLOAT_EQ(f.divisor, 1.0f);
    PASS();
}

TEST(filter_grayscale_partial) {
    NVGpixelBuffer src, dst_full, dst_half;
    make_solid_buffer(&src, 2, 2, 255, 0, 0, 255);
    make_solid_buffer(&dst_full, 2, 2, 0, 0, 0, 0);
    make_solid_buffer(&dst_half, 2, 2, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    f.amount    = 1.0f;
    nvgFilterRGBA(&src, &dst_full, &f, 1);
    f.amount = 0.5f;
    nvgFilterRGBA(&src, &dst_half, &f, 1);
    ASSERT_TRUE(dst_half.data[0] > dst_full.data[0]);
    ASSERT_TRUE(dst_half.data[0] < 255);
    free_buffer(&src);
    free_buffer(&dst_full);
    free_buffer(&dst_half);
    PASS();
}

TEST(filter_box_blur_radius_zero) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 4, 4, 100, 150, 200, 255);
    make_solid_buffer(&dst, 4, 4, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    f.radiusX   = 0;
    f.radiusY   = 0;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+0], 100);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+1], 150);
    ASSERT_BYTE_EQ(dst.data[4*(1*4+1)+2], 200);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

TEST(filter_alpha_preservation) {
    NVGpixelBuffer src, dst;
    make_solid_buffer(&src, 2, 2, 255, 0, 0, 128);
    make_solid_buffer(&dst, 2, 2, 0, 0, 0, 0);
    NVGfilter f = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    f.amount    = 1.0f;
    NVGfilterStatus s = nvgFilterRGBA(&src, &dst, &f, 1);
    ASSERT_INT_EQ(s, NVG_FILTER_OK);
    ASSERT_BYTE_EQ(dst.data[3], 128);
    free_buffer(&src);
    free_buffer(&dst);
    PASS();
}

// ---------------------------------------------------------------------------
// Transform math (additional)
// ---------------------------------------------------------------------------

TEST(transform_premultiply) {
    float a[6], b[6];
    nvgTransformTranslate(a, 10.0f, 20.0f);
    nvgTransformScale(b, 2.0f, 3.0f);
    // premultiply: a = b * a (scale first, then translate)
    nvgTransformPremultiply(a, b);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, a, 1.0f, 1.0f);
    ASSERT_FLOAT_EQ(dx, 12.0f);
    ASSERT_FLOAT_EQ(dy, 23.0f);
    PASS();
}

TEST(transform_multiply_scale_translate) {
    float a[6], b[6];
    nvgTransformScale(a, 2.0f, 2.0f);
    nvgTransformTranslate(b, 5.0f, 10.0f);
    nvgTransformMultiply(a, b);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, a, 1.0f, 1.0f);
    ASSERT_FLOAT_EQ(dx, 7.0f);
    ASSERT_FLOAT_EQ(dy, 12.0f);
    PASS();
}

TEST(transform_inverse_scale) {
    float t[6], inv[6];
    nvgTransformScale(t, 4.0f, 0.25f);
    int ok = nvgTransformInverse(inv, t);
    ASSERT_TRUE(ok);
    ASSERT_FLOAT_EQ(inv[0], 0.25f);
    ASSERT_FLOAT_EQ(inv[3], 4.0f);
    PASS();
}

TEST(transform_inverse_singular) {
    float t[6] = {0,0,0,0,0,0};
    float inv[6];
    int ok = nvgTransformInverse(inv, t);
    ASSERT_TRUE(!ok);
    PASS();
}

TEST(transform_skew_x_nonzero) {
    float t[6];
    nvgTransformSkewX(t, NVG_PI / 4.0f);
    ASSERT_FLOAT_EQ(t[0], 1.0f);
    ASSERT_FLOAT_EQ(t[3], 1.0f);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 0.0f, 1.0f);
    ASSERT_FLOAT_EQ(dx, tanf(NVG_PI / 4.0f));
    ASSERT_FLOAT_EQ(dy, 1.0f);
    PASS();
}

TEST(transform_skew_y_nonzero) {
    float t[6];
    nvgTransformSkewY(t, NVG_PI / 4.0f);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 1.0f, 0.0f);
    ASSERT_FLOAT_EQ(dx, 1.0f);
    ASSERT_FLOAT_EQ(dy, tanf(NVG_PI / 4.0f));
    PASS();
}

TEST(transform_rotate_180) {
    float t[6];
    nvgTransformRotate(t, NVG_PI);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, t, 1.0f, 0.0f);
    ASSERT_FLOAT_EQ(dx, -1.0f);
    ASSERT_FLOAT_EQ(dy, 0.0f);
    PASS();
}

TEST(transform_combined_rotate_translate) {
    float r[6], tr[6];
    nvgTransformRotate(r, NVG_PI / 2.0f);
    nvgTransformTranslate(tr, 10.0f, 0.0f);
    nvgTransformMultiply(r, tr);
    float dx, dy;
    nvgTransformPoint(&dx, &dy, r, 0.0f, 0.0f);
    ASSERT_FLOAT_EQ(dx, 10.0f);
    ASSERT_FLOAT_EQ(dy, 0.0f);
    PASS();
}

// ---------------------------------------------------------------------------
// Color edge cases
// ---------------------------------------------------------------------------

TEST(rgb_black) {
    NVGcolor c = nvgRGB(0, 0, 0);
    ASSERT_FLOAT_EQ(c.r, 0.0f);
    ASSERT_FLOAT_EQ(c.g, 0.0f);
    ASSERT_FLOAT_EQ(c.b, 0.0f);
    ASSERT_FLOAT_EQ(c.a, 1.0f);
    PASS();
}

TEST(rgb_white) {
    NVGcolor c = nvgRGB(255, 255, 255);
    ASSERT_FLOAT_EQ(c.r, 1.0f);
    ASSERT_FLOAT_EQ(c.g, 1.0f);
    ASSERT_FLOAT_EQ(c.b, 1.0f);
    PASS();
}

TEST(rgba_transparent) {
    NVGcolor c = nvgRGBA(255, 128, 64, 0);
    ASSERT_FLOAT_EQ(c.a, 0.0f);
    ASSERT_FLOAT_EQ(c.r, 1.0f);
    PASS();
}

TEST(rgba_all_channels) {
    NVGcolor c = nvgRGBA(51, 102, 153, 204);
    ASSERT_FLOAT_EQ(c.r, 51.0f / 255.0f);
    ASSERT_FLOAT_EQ(c.g, 102.0f / 255.0f);
    ASSERT_FLOAT_EQ(c.b, 153.0f / 255.0f);
    ASSERT_FLOAT_EQ(c.a, 204.0f / 255.0f);
    PASS();
}

TEST(lerp_rgba_quarter) {
    NVGcolor a = nvgRGBAf(0.0f, 0.0f, 0.0f, 0.0f);
    NVGcolor b = nvgRGBAf(1.0f, 1.0f, 1.0f, 1.0f);
    NVGcolor q = nvgLerpRGBA(a, b, 0.25f);
    ASSERT_FLOAT_EQ(q.r, 0.25f);
    ASSERT_FLOAT_EQ(q.a, 0.25f);
    PASS();
}

TEST(trans_rgba_full_opaque) {
    NVGcolor c = nvgRGBA(100, 200, 50, 64);
    NVGcolor t = nvgTransRGBA(c, 255);
    ASSERT_FLOAT_EQ(t.a, 1.0f);
    ASSERT_FLOAT_EQ(t.r, 100.0f / 255.0f);
    PASS();
}

TEST(trans_rgba_full_transparent) {
    NVGcolor c = nvgRGB(255, 255, 255);
    NVGcolor t = nvgTransRGBA(c, 0);
    ASSERT_FLOAT_EQ(t.a, 0.0f);
    PASS();
}

TEST(trans_rgbaf_clamp_check) {
    NVGcolor c = nvgRGBf(0.5f, 0.5f, 0.5f);
    NVGcolor t = nvgTransRGBAf(c, 0.0f);
    ASSERT_FLOAT_EQ(t.a, 0.0f);
    t = nvgTransRGBAf(c, 1.0f);
    ASSERT_FLOAT_EQ(t.a, 1.0f);
    PASS();
}

TEST(hsl_blue) {
    NVGcolor c = nvgHSL(2.0f / 3.0f, 1.0f, 0.5f);
    ASSERT_TRUE(c.b > 0.95f);
    ASSERT_TRUE(c.r < 0.05f);
    ASSERT_TRUE(c.g < 0.05f);
    PASS();
}

TEST(hsl_gray) {
    NVGcolor c = nvgHSL(0.0f, 0.0f, 0.5f);
    ASSERT_FLOAT_EQ(c.r, c.g);
    ASSERT_FLOAT_EQ(c.g, c.b);
    ASSERT_TRUE(fabsf(c.r - 0.5f) < 0.02f);
    PASS();
}

TEST(hsl_black_white) {
    NVGcolor black = nvgHSL(0.0f, 1.0f, 0.0f);
    ASSERT_TRUE(black.r < 0.01f);
    ASSERT_TRUE(black.g < 0.01f);
    ASSERT_TRUE(black.b < 0.01f);
    NVGcolor white = nvgHSL(0.0f, 1.0f, 1.0f);
    ASSERT_TRUE(white.r > 0.99f);
    ASSERT_TRUE(white.g > 0.99f);
    ASSERT_TRUE(white.b > 0.99f);
    PASS();
}

TEST(hsla_full_alpha) {
    NVGcolor c = nvgHSLA(0.5f, 1.0f, 0.5f, 255);
    ASSERT_FLOAT_EQ(c.a, 1.0f);
    PASS();
}

TEST(color_array_access) {
    NVGcolor c = nvgRGBAf(0.1f, 0.2f, 0.3f, 0.4f);
    ASSERT_FLOAT_EQ(c.rgba[0], 0.1f);
    ASSERT_FLOAT_EQ(c.rgba[1], 0.2f);
    ASSERT_FLOAT_EQ(c.rgba[2], 0.3f);
    ASSERT_FLOAT_EQ(c.rgba[3], 0.4f);
    PASS();
}

// ---------------------------------------------------------------------------
// Angle conversion (additional)
// ---------------------------------------------------------------------------

TEST(deg_to_rad_negative) {
    ASSERT_FLOAT_EQ(nvgDegToRad(-90.0f), -NVG_PI / 2.0f);
    PASS();
}

TEST(rad_to_deg_negative) {
    ASSERT_FLOAT_EQ(nvgRadToDeg(-NVG_PI), -180.0f);
    PASS();
}

// ---------------------------------------------------------------------------
// Null-backend context (tests state management without GPU)
// ---------------------------------------------------------------------------

static int null_renderCreate(void* uptr) {
    (void)uptr;
    return 1;
}
static int null_renderCreateTexture(void* uptr, int type, int w, int h, int flags, const unsigned char* data) {
    (void)uptr;
    (void)type;
    (void)w;
    (void)h;
    (void)flags;
    (void)data;
    return 1;
}
static int null_renderDeleteTexture(void* uptr, int image) {
    (void)uptr;
    (void)image;
    return 1;
}
static int null_renderUpdateTexture(void* uptr, int image, int x, int y, int w, int h, const unsigned char* data) {
    (void)uptr;
    (void)image;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
    return 1;
}
static int null_renderGetTextureSize(void* uptr, int image, int* w, int* h) {
    (void)uptr;
    (void)image;
    *w = 0;
    *h = 0;
    return 1;
}
static void null_renderViewport(void* uptr, float w, float h, float dpr) {
    (void)uptr;
    (void)w;
    (void)h;
    (void)dpr;
}
static void null_renderCancel(void* uptr) { (void)uptr; }
static void null_renderFlush(void* uptr) { (void)uptr; }
static void null_renderFill(void* uptr, NVGpaint* paint, NVGcompositeOperationState cop, NVGscissor* scissor,
                            float fringe, const float* bounds, const NVGpath* paths, int npaths) {
    (void)uptr;
    (void)paint;
    (void)cop;
    (void)scissor;
    (void)fringe;
    (void)bounds;
    (void)paths;
    (void)npaths;
}
static void null_renderStroke(void* uptr, NVGpaint* paint, NVGcompositeOperationState cop, NVGscissor* scissor,
                              float fringe, float strokeWidth, int lineStyle, const NVGpath* paths, int npaths) {
    (void)uptr;
    (void)paint;
    (void)cop;
    (void)scissor;
    (void)fringe;
    (void)strokeWidth;
    (void)lineStyle;
    (void)paths;
    (void)npaths;
}
static void null_renderTriangles(void* uptr, NVGpaint* paint, NVGcompositeOperationState cop, NVGscissor* scissor,
                                 const NVGvertex* verts, int nverts, float fringe) {
    (void)uptr;
    (void)paint;
    (void)cop;
    (void)scissor;
    (void)verts;
    (void)nverts;
    (void)fringe;
}
static void null_renderDelete(void* uptr) { (void)uptr; }

static NVGcontext* create_null_context(void) {
    NVGparams params;
    memset(&params, 0, sizeof(params));
    params.renderCreate         = null_renderCreate;
    params.renderCreateTexture  = null_renderCreateTexture;
    params.renderDeleteTexture  = null_renderDeleteTexture;
    params.renderUpdateTexture  = null_renderUpdateTexture;
    params.renderGetTextureSize = null_renderGetTextureSize;
    params.renderViewport       = null_renderViewport;
    params.renderCancel         = null_renderCancel;
    params.renderFlush          = null_renderFlush;
    params.renderFill           = null_renderFill;
    params.renderStroke         = null_renderStroke;
    params.renderTriangles      = null_renderTriangles;
    params.renderDelete         = null_renderDelete;
    params.edgeAntiAlias        = 1;
    return nvgCreateInternal(&params);
}

TEST(null_context_create_destroy) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_save_restore) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgSave(ctx);
    nvgStrokeWidth(ctx, 5.0f);
    nvgGlobalAlpha(ctx, 0.5f);
    nvgRestore(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_begin_end_frame) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 1920, 1080, 2.0f);
    nvgBeginPath(ctx);
    nvgRect(ctx, 10, 10, 100, 100);
    nvgFillColor(ctx, nvgRGB(255, 0, 0));
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_cancel_frame) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgCircle(ctx, 50, 50, 25);
    nvgCancelFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_path_operations) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgLineTo(ctx, 100, 0);
    nvgLineTo(ctx, 100, 100);
    nvgClosePath(ctx);
    nvgStrokeColor(ctx, nvgRGB(0, 255, 0));
    nvgStrokeWidth(ctx, 2.0f);
    nvgStroke(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, 10, 10, 50, 50, 5);
    nvgFillColor(ctx, nvgRGBA(0, 0, 255, 128));
    nvgFill(ctx);

    nvgBeginPath(ctx);
    nvgEllipse(ctx, 200, 200, 50, 30);
    nvgFill(ctx);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_transform_state) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgSave(ctx);
    nvgTranslate(ctx, 100, 200);
    nvgRotate(ctx, nvgDegToRad(45.0f));
    nvgScale(ctx, 2.0f, 2.0f);
    float xform[6];
    nvgCurrentTransform(ctx, xform);
    ASSERT_TRUE(xform[0] != 1.0f || xform[4] != 0.0f);
    nvgRestore(ctx);
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 1.0f);
    ASSERT_FLOAT_EQ(xform[4], 0.0f);
    ASSERT_FLOAT_EQ(xform[5], 0.0f);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_scissor) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgScissor(ctx, 10, 10, 200, 200);
    nvgIntersectScissor(ctx, 50, 50, 100, 100);
    nvgResetScissor(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_composite_ops) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgGlobalCompositeOperation(ctx, NVG_SOURCE_OVER);
    nvgGlobalCompositeBlendFunc(ctx, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 50, 50);
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_reset) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgStrokeWidth(ctx, 10.0f);
    nvgGlobalAlpha(ctx, 0.1f);
    nvgFillColor(ctx, nvgRGB(255, 0, 0));
    nvgReset(ctx);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 50, 50);
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_nested_save_restore) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    float xform[6];

    nvgSave(ctx);
    nvgTranslate(ctx, 10, 20);
    nvgSave(ctx);
    nvgScale(ctx, 2.0f, 2.0f);
    nvgSave(ctx);
    nvgRotate(ctx, nvgDegToRad(45.0f));

    nvgCurrentTransform(ctx, xform);
    ASSERT_TRUE(xform[0] != 1.0f);

    nvgRestore(ctx);
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 2.0f);

    nvgRestore(ctx);
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 1.0f);
    ASSERT_FLOAT_EQ(xform[4], 10.0f);
    ASSERT_FLOAT_EQ(xform[5], 20.0f);

    nvgRestore(ctx);
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 1.0f);
    ASSERT_FLOAT_EQ(xform[4], 0.0f);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_shape_antialias) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgShapeAntiAlias(ctx, 0);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 100, 100);
    nvgFill(ctx);
    nvgShapeAntiAlias(ctx, 1);
    nvgBeginPath(ctx);
    nvgCircle(ctx, 50, 50, 25);
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_bezier_quad) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgBezierTo(ctx, 50, 0, 50, 100, 100, 100);
    nvgStrokeColor(ctx, nvgRGB(255, 0, 0));
    nvgStroke(ctx);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgQuadTo(ctx, 50, 50, 100, 0);
    nvgStroke(ctx);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_arc_arcto) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgArc(ctx, 100, 100, 50, 0, NVG_PI, NVG_CW);
    nvgStroke(ctx);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgArcTo(ctx, 50, 0, 50, 50, 10);
    nvgStroke(ctx);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_rounded_rect_varying) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, 10, 10, 100, 100, 5, 10, 15, 20);
    nvgFillColor(ctx, nvgRGB(0, 128, 255));
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_path_winding) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 200, 200);
    nvgPathWinding(ctx, NVG_SOLID);
    nvgRect(ctx, 50, 50, 100, 100);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillColor(ctx, nvgRGB(255, 0, 0));
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_stroke_paint) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    NVGpaint p = nvgLinearGradient(ctx, 0, 0, 100, 100, nvgRGB(255,0,0), nvgRGB(0,0,255));
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgLineTo(ctx, 100, 100);
    nvgStrokePaint(ctx, p);
    nvgStrokeWidth(ctx, 3.0f);
    nvgStroke(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_fill_paint_gradients) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);

    NVGpaint lg = nvgLinearGradient(ctx, 0, 0, 200, 0, nvgRGB(255,0,0), nvgRGB(0,255,0));
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 200, 100);
    nvgFillPaint(ctx, lg);
    nvgFill(ctx);

    NVGpaint bg = nvgBoxGradient(ctx, 10, 10, 100, 100, 5, 10, nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
    nvgBeginPath(ctx);
    nvgRect(ctx, 10, 10, 100, 100);
    nvgFillPaint(ctx, bg);
    nvgFill(ctx);

    NVGpaint rg = nvgRadialGradient(ctx, 100, 100, 10, 50, nvgRGB(255,255,0), nvgRGBA(255,255,0,0));
    nvgBeginPath(ctx);
    nvgCircle(ctx, 100, 100, 50);
    nvgFillPaint(ctx, rg);
    nvgFill(ctx);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_global_alpha) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgGlobalAlpha(ctx, 0.5f);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 100, 100);
    nvgFillColor(ctx, nvgRGB(255, 0, 0));
    nvgFill(ctx);
    nvgGlobalAlpha(ctx, 1.0f);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_composite_all_ops) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    int ops[] = {NVG_SOURCE_OVER, NVG_SOURCE_IN, NVG_SOURCE_OUT, NVG_ATOP,
                 NVG_DESTINATION_OVER, NVG_DESTINATION_IN, NVG_DESTINATION_OUT,
                 NVG_DESTINATION_ATOP, NVG_LIGHTER, NVG_COPY, NVG_XOR};
    for (int i = 0; i < 11; i++) {
        nvgGlobalCompositeOperation(ctx, ops[i]);
        nvgBeginPath(ctx);
        nvgRect(ctx, (float)i * 10, 0, 10, 10);
        nvgFill(ctx);
    }
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_composite_blend_func_separate) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgGlobalCompositeBlendFuncSeparate(ctx, NVG_ONE, NVG_ONE_MINUS_SRC_ALPHA, NVG_ONE, NVG_ZERO);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 50, 50);
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_all_line_styles) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    int styles[] = {NVG_LINE_SOLID, NVG_LINE_DASHED, NVG_LINE_DOTTED, NVG_LINE_GLOW};
    int caps[]   = {NVG_BUTT, NVG_ROUND, NVG_SQUARE};
    int joins[]  = {NVG_MITER, NVG_ROUND, NVG_BEVEL};
    for (int s = 0; s < 4; s++) {
        nvgLineStyle(ctx, styles[s]);
        nvgLineCap(ctx, caps[s % 3]);
        nvgLineJoin(ctx, joins[s % 3]);
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, 0, (float)s * 20);
        nvgLineTo(ctx, 100, (float)s * 20);
        nvgStrokeWidth(ctx, 2.0f);
        nvgStroke(ctx);
    }
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_create_image_rgba) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    unsigned char pixels[4 * 4 * 4];
    memset(pixels, 128, sizeof(pixels));
    int img = nvgCreateImageRGBA(ctx, 4, 4, 0, pixels);
    ASSERT_TRUE(img > 0);
    nvgDeleteImage(ctx, img);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_image_pattern) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    unsigned char pixels[4 * 4 * 4];
    memset(pixels, 200, sizeof(pixels));
    int img = nvgCreateImageRGBA(ctx, 4, 4, 0, pixels);
    NVGpaint ip = nvgImagePattern(ctx, 0, 0, 100, 100, 0, img, 1.0f);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, 100, 100);
    nvgFillPaint(ctx, ip);
    nvgFill(ctx);
    nvgEndFrame(ctx);
    nvgDeleteImage(ctx, img);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_multiple_frames) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    for (int i = 0; i < 5; i++) {
        nvgBeginFrame(ctx, 800, 600, 1.0f);
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, (float)(i + 1) * 10, (float)(i + 1) * 10);
        nvgFillColor(ctx, nvgRGB(255, 0, 0));
        nvgFill(ctx);
        nvgEndFrame(ctx);
    }
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_internal_params) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    NVGparams* p = nvgInternalParams(ctx);
    ASSERT_TRUE(p != NULL);
    ASSERT_INT_EQ(p->edgeAntiAlias, 1);
    ASSERT_TRUE(p->renderCreate != NULL);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_text_state) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgFontSize(ctx, 24.0f);
    nvgFontBlur(ctx, 1.0f);
    nvgFontDilate(ctx, 0.5f);
    nvgTextLetterSpacing(ctx, 2.0f);
    nvgTextLineHeight(ctx, 1.5f);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFontQuality(ctx, 0.5f);
    int notFound = nvgFindFont(ctx, "nonexistent-font");
    ASSERT_INT_EQ(notFound, -1);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_skew_transforms) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgSave(ctx);
    nvgSkewX(ctx, nvgDegToRad(15.0f));
    nvgSkewY(ctx, nvgDegToRad(10.0f));
    float xform[6];
    nvgCurrentTransform(ctx, xform);
    ASSERT_TRUE(xform[1] != 0.0f || xform[2] != 0.0f);
    nvgRestore(ctx);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_reset_transform) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgTranslate(ctx, 100, 200);
    nvgScale(ctx, 3.0f, 3.0f);
    nvgResetTransform(ctx);
    float xform[6];
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 1.0f);
    ASSERT_FLOAT_EQ(xform[3], 1.0f);
    ASSERT_FLOAT_EQ(xform[4], 0.0f);
    ASSERT_FLOAT_EQ(xform[5], 0.0f);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_ctx_transform) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgTransform(ctx, 2, 0, 0, 3, 10, 20);
    float xform[6];
    nvgCurrentTransform(ctx, xform);
    ASSERT_FLOAT_EQ(xform[0], 2.0f);
    ASSERT_FLOAT_EQ(xform[3], 3.0f);
    ASSERT_FLOAT_EQ(xform[4], 10.0f);
    ASSERT_FLOAT_EQ(xform[5], 20.0f);
    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

TEST(null_context_line_styles) {
    NVGcontext* ctx = create_null_context();
    ASSERT_TRUE(ctx != NULL);
    nvgBeginFrame(ctx, 800, 600, 1.0f);
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 0, 0);
    nvgLineTo(ctx, 100, 100);

    nvgLineStyle(ctx, NVG_LINE_DASHED);
    nvgLineCap(ctx, NVG_ROUND);
    nvgLineJoin(ctx, NVG_BEVEL);
    nvgMiterLimit(ctx, 4.0f);
    nvgStrokeWidth(ctx, 3.0f);
    nvgStrokeColor(ctx, nvgRGB(255, 255, 0));
    nvgStroke(ctx);

    nvgEndFrame(ctx);
    nvgDeleteInternal(ctx);
    PASS();
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

#define RUN(name)                                                                                                      \
    do {                                                                                                               \
        int before = g_tests_failed;                                                                                   \
        printf("  %-45s", #name);                                                                                      \
        run_##name();                                                                                                  \
        printf("%s\n", g_tests_failed == before ? "OK" : "FAILED");                                                    \
    } while (0)

int main(void) {
    printf("nanovg test suite\n");
    printf("=================\n\n");

    printf("[color utilities]\n");
    RUN(rgb);
    RUN(rgba);
    RUN(rgbf);
    RUN(rgbaf);
    RUN(lerp_rgba);
    RUN(lerp_rgba_boundaries);
    RUN(trans_rgba);
    RUN(trans_rgbaf);
    RUN(hsl_red);
    RUN(hsl_green);
    RUN(hsla);
    RUN(rgb_black);
    RUN(rgb_white);
    RUN(rgba_transparent);
    RUN(rgba_all_channels);
    RUN(lerp_rgba_quarter);
    RUN(trans_rgba_full_opaque);
    RUN(trans_rgba_full_transparent);
    RUN(trans_rgbaf_clamp_check);
    RUN(hsl_blue);
    RUN(hsl_gray);
    RUN(hsl_black_white);
    RUN(hsla_full_alpha);
    RUN(color_array_access);

    printf("\n[angle conversion]\n");
    RUN(deg_to_rad);
    RUN(rad_to_deg);
    RUN(deg_rad_roundtrip);
    RUN(deg_to_rad_negative);
    RUN(rad_to_deg_negative);

    printf("\n[transform math]\n");
    RUN(transform_identity);
    RUN(transform_translate);
    RUN(transform_scale);
    RUN(transform_rotate);
    RUN(transform_point);
    RUN(transform_point_scale);
    RUN(transform_multiply_identity);
    RUN(transform_inverse);
    RUN(transform_inverse_roundtrip);
    RUN(transform_skew_x);
    RUN(transform_skew_y);
    RUN(transform_premultiply);
    RUN(transform_multiply_scale_translate);
    RUN(transform_inverse_scale);
    RUN(transform_inverse_singular);
    RUN(transform_skew_x_nonzero);
    RUN(transform_skew_y_nonzero);
    RUN(transform_rotate_180);
    RUN(transform_combined_rotate_translate);

    printf("\n[cpu image filters]\n");
    RUN(filter_init_defaults);
    RUN(filter_init_all_types);
    RUN(filter_zero_count_copies);
    RUN(filter_grayscale_full);
    RUN(filter_grayscale_zero_is_passthrough);
    RUN(filter_grayscale_partial);
    RUN(filter_brightness_contrast_identity);
    RUN(filter_brightness_increase);
    RUN(filter_brightness_decrease);
    RUN(filter_contrast_increase);
    RUN(filter_contrast_zero);
    RUN(filter_box_blur);
    RUN(filter_box_blur_radius_zero);
    RUN(filter_gaussian_blur);
    RUN(filter_sharpen);
    RUN(filter_sobel);
    RUN(filter_unsharp_mask);
    RUN(filter_convolution_identity);
    RUN(filter_convolution_invalid_even_kernel);
    RUN(filter_chain_grayscale_then_brightness);
    RUN(filter_null_source);
    RUN(filter_null_destination);
    RUN(filter_null_data_buffer);
    RUN(filter_zero_width_buffer);
    RUN(filter_dimension_mismatch);
    RUN(filter_too_many_stages);
    RUN(filter_negative_count);
    RUN(filter_border_repeat);
    RUN(filter_border_mirror);
    RUN(filter_border_transparent);
    RUN(filter_inplace_alias);
    RUN(filter_premultiplied_conversion);
    RUN(filter_alpha_preservation);

    printf("\n[null-backend context]\n");
    RUN(null_context_create_destroy);
    RUN(null_context_save_restore);
    RUN(null_context_nested_save_restore);
    RUN(null_context_reset);
    RUN(null_context_begin_end_frame);
    RUN(null_context_cancel_frame);
    RUN(null_context_multiple_frames);
    RUN(null_context_path_operations);
    RUN(null_context_bezier_quad);
    RUN(null_context_arc_arcto);
    RUN(null_context_rounded_rect_varying);
    RUN(null_context_path_winding);
    RUN(null_context_transform_state);
    RUN(null_context_reset_transform);
    RUN(null_context_ctx_transform);
    RUN(null_context_skew_transforms);
    RUN(null_context_shape_antialias);
    RUN(null_context_global_alpha);
    RUN(null_context_scissor);
    RUN(null_context_composite_ops);
    RUN(null_context_composite_all_ops);
    RUN(null_context_composite_blend_func_separate);
    RUN(null_context_line_styles);
    RUN(null_context_all_line_styles);
    RUN(null_context_stroke_paint);
    RUN(null_context_fill_paint_gradients);
    RUN(null_context_create_image_rgba);
    RUN(null_context_image_pattern);
    RUN(null_context_text_state);
    RUN(null_context_internal_params);

    printf("\n=================\n");
    printf("%d tests run, %d passed, %d failed\n", g_tests_run, g_tests_passed, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}

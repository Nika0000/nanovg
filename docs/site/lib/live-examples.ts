export const welcomeExample = `#include "nanovg.h"
#include <cmath>


void drawWindow(NVGcontext* vg, const char* title,
                float x, float y, float w, float h) {
    float cr = 3.0f;
    nvgSave(vg);

    NVGpaint shadow = nvgBoxGradient(vg, x, y+2, w, h, cr*2, 10,
        nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
    nvgBeginPath(vg);
    nvgRect(vg, x-10, y-10, w+20, h+30);
    nvgRoundedRect(vg, x, y, w, h, cr);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, cr);
    nvgFillColor(vg, nvgRGBA(28,30,34,192));
    nvgFill(vg);

    NVGpaint header = nvgLinearGradient(vg, x, y, x, y+15,
        nvgRGBA(255,255,255,8), nvgRGBA(0,0,0,16));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+1, y+1, w-2, 30, cr-1);
    nvgFillPaint(vg, header);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, x+0.5f, y+30.5f);
    nvgLineTo(vg, x+w-0.5f, y+30.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,32));
    nvgStroke(vg);

    nvgFontSize(vg, 15.0f);
    nvgFontFace(vg, "sans");
    nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGBA(0,0,0,128));
    nvgText(vg, x+w/2, y+16+1, title, nullptr);
    nvgFillColor(vg, nvgRGBA(220,220,220,160));
    nvgText(vg, x+w/2, y+16, title, nullptr);

    nvgRestore(vg);
}

void drawSearchBox(NVGcontext* vg, const char* text,
                   float x, float y, float w, float h) {
    float cr = h/2 - 1;
    NVGpaint bg = nvgBoxGradient(vg, x, y+1.5f, w, h, h/2, 5,
        nvgRGBA(0,0,0,16), nvgRGBA(0,0,0,92));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, cr);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgFontSize(vg, 17.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,32));
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+h*0.6f, y+h*0.5f, text, nullptr);
}

void drawDropDown(NVGcontext* vg, const char* text,
                  float x, float y, float w, float h) {
    float cr = 4.0f;
    NVGpaint bg = nvgLinearGradient(vg, x, y, x, y+h,
        nvgRGBA(255,255,255,16), nvgRGBA(0,0,0,16));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+1, y+1, w-2, h-2, cr-1);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+0.5f, y+0.5f, w-1, h-1, cr-0.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,48));
    nvgStroke(vg);

    nvgFontSize(vg, 17.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,160));
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+h*0.3f, y+h*0.5f, text, nullptr);
}

void drawEditBoxBase(NVGcontext* vg, float x, float y, float w, float h) {
    NVGpaint bg = nvgBoxGradient(vg, x+1, y+2.5f, w-2, h-2, 3, 4,
        nvgRGBA(255,255,255,32), nvgRGBA(32,32,32,32));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+1, y+1, w-2, h-2, 3);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+0.5f, y+0.5f, w-1, h-1, 3.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,48));
    nvgStroke(vg);
}

void drawEditBox(NVGcontext* vg, const char* text,
                 float x, float y, float w, float h) {
    drawEditBoxBase(vg, x, y, w, h);
    nvgFontSize(vg, 17.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,64));
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+h*0.3f, y+h*0.5f, text, nullptr);
}

void drawEditBoxNum(NVGcontext* vg, const char* text, const char* units,
                    float x, float y, float w, float h) {
    drawEditBoxBase(vg, x, y, w, h);
    float uw = nvgTextBounds(vg, 0, 0, units, nullptr, nullptr);

    nvgFontSize(vg, 15.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,64));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+w-h*0.3f, y+h*0.5f, units, nullptr);

    nvgFontSize(vg, 17.0f);
    nvgFillColor(vg, nvgRGBA(255,255,255,128));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+w-uw-h*0.5f, y+h*0.5f, text, nullptr);
}

void drawLabel(NVGcontext* vg, const char* text,
               float x, float y, float w, float h) {
    nvgFontSize(vg, 15.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,128));
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x, y+h*0.5f, text, nullptr);
}

void drawCheckBox(NVGcontext* vg, const char* text,
                  float x, float y, float w, float h) {
    nvgFontSize(vg, 15.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255,255,255,160));
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgText(vg, x+28, y+h*0.5f, text, nullptr);

    NVGpaint bg = nvgBoxGradient(vg, x+1, y+(int)(h*0.5f)-9+1,
        18, 18, 3, 3, nvgRGBA(0,0,0,32), nvgRGBA(0,0,0,92));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+1, y+(int)(h*0.5f)-9, 18, 18, 3);
    nvgFillPaint(vg, bg);
    nvgFill(vg);
}

void drawButton(NVGcontext* vg, const char* text,
                float x, float y, float w, float h, NVGcolor col) {
    int black = (col.r == 0 && col.g == 0 && col.b == 0 && col.a == 0);
    float cr = 4.0f;

    NVGpaint bg = nvgLinearGradient(vg, x, y, x, y+h,
        nvgRGBA(255,255,255, black?16:32),
        nvgRGBA(0,0,0, black?16:32));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+1, y+1, w-2, h-2, cr-1);
    if (!black) { nvgFillColor(vg, col); nvgFill(vg); }
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x+0.5f, y+0.5f, w-1, h-1, cr-0.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,48));
    nvgStroke(vg);

    nvgFontSize(vg, 17.0f);
    nvgFontFace(vg, "sans");
    float tw = nvgTextBounds(vg, 0, 0, text, nullptr, nullptr);
    nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGBA(0,0,0,160));
    nvgText(vg, x+w*0.5f-tw*0.5f, y+h*0.5f-1, text, nullptr);
    nvgFillColor(vg, nvgRGBA(255,255,255,160));
    nvgText(vg, x+w*0.5f-tw*0.5f, y+h*0.5f, text, nullptr);
}

void drawSlider(NVGcontext* vg, float pos, float x, float y, float w, float h) {
    float cy = y + (int)(h*0.5f);
    float kr = (int)(h*0.25f);

    nvgSave(vg);
    NVGpaint bg = nvgBoxGradient(vg, x, cy-2+1, w, 4, 2, 2,
        nvgRGBA(0,0,0,32), nvgRGBA(0,0,0,128));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, cy-2, w, 4, 2);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    bg = nvgRadialGradient(vg, x+(int)(pos*w), cy+1, kr-3, kr+3,
        nvgRGBA(0,0,0,64), nvgRGBA(0,0,0,0));
    nvgBeginPath(vg);
    nvgRect(vg, x+(int)(pos*w)-kr-5, cy-kr-5, kr*2+10, kr*2+10+3);
    nvgCircle(vg, x+(int)(pos*w), cy, kr);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    NVGpaint knob = nvgLinearGradient(vg, x, cy-kr, x, cy+kr,
        nvgRGBA(255,255,255,16), nvgRGBA(0,0,0,16));
    nvgBeginPath(vg);
    nvgCircle(vg, x+(int)(pos*w), cy, kr-1);
    nvgFillColor(vg, nvgRGBA(40,43,48,255));
    nvgFill(vg);
    nvgFillPaint(vg, knob);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, x+(int)(pos*w), cy, kr-0.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,92));
    nvgStroke(vg);
    nvgRestore(vg);
}

void drawEyes(NVGcontext* vg, float x, float y, float w, float h,
              float mx, float my, float t) {
    float ex = w * 0.23f;
    float ey = h * 0.5f;
    float lx = x + ex, ly = y + ey;
    float rx = x + w - ex, ry = y + ey;
    float br = (ex < ey ? ex : ey) * 0.5f;
    float blink = 1 - std::pow(std::sin(t*0.5f), 200.0f) * 0.8f;

    NVGpaint bg = nvgLinearGradient(vg, x, y+h*0.5f, x+w*0.1f, y+h,
        nvgRGBA(0,0,0,32), nvgRGBA(0,0,0,16));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx+3, ly+16, ex, ey);
    nvgEllipse(vg, rx+3, ry+16, ex, ey);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    bg = nvgLinearGradient(vg, x, y+h*0.25f, x+w*0.1f, y+h,
        nvgRGBA(220,220,220,255), nvgRGBA(128,128,128,255));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx, ly, ex, ey);
    nvgEllipse(vg, rx, ry, ex, ey);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    float dx = (mx - lx) / (ex * 10);
    float dy = (my - ly) / (ey * 10);
    float d = std::sqrt(dx*dx + dy*dy);
    if (d > 1.0f) { dx /= d; dy /= d; }
    dx *= ex*0.4f; dy *= ey*0.5f;
    nvgBeginPath(vg);
    nvgEllipse(vg, lx+dx, ly+dy+ey*0.25f*(1-blink), br, br*blink);
    nvgFillColor(vg, nvgRGBA(32,32,32,255));
    nvgFill(vg);

    dx = (mx - rx) / (ex * 10);
    dy = (my - ry) / (ey * 10);
    d = std::sqrt(dx*dx + dy*dy);
    if (d > 1.0f) { dx /= d; dy /= d; }
    dx *= ex*0.4f; dy *= ey*0.5f;
    nvgBeginPath(vg);
    nvgEllipse(vg, rx+dx, ry+dy+ey*0.25f*(1-blink), br, br*blink);
    nvgFillColor(vg, nvgRGBA(32,32,32,255));
    nvgFill(vg);

    NVGpaint gloss = nvgRadialGradient(vg, lx-ex*0.25f, ly-ey*0.5f,
        ex*0.1f, ex*0.75f, nvgRGBA(255,255,255,128), nvgRGBA(255,255,255,0));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx, ly, ex, ey);
    nvgFillPaint(vg, gloss);
    nvgFill(vg);

    gloss = nvgRadialGradient(vg, rx-ex*0.25f, ry-ey*0.5f,
        ex*0.1f, ex*0.75f, nvgRGBA(255,255,255,128), nvgRGBA(255,255,255,0));
    nvgBeginPath(vg);
    nvgEllipse(vg, rx, ry, ex, ey);
    nvgFillPaint(vg, gloss);
    nvgFill(vg);
}

void drawGraph(NVGcontext* vg, float x, float y, float w, float h, float t) {
    float samples[6], sx[6], sy[6];
    float dx = w / 5.0f;

    samples[0] = (1+std::sin(t*1.2345f+std::cos(t*0.33457f)*0.44f))*0.5f;
    samples[1] = (1+std::sin(t*0.68363f+std::cos(t*1.3f)*1.55f))*0.5f;
    samples[2] = (1+std::sin(t*1.1642f+std::cos(t*0.33457f)*1.24f))*0.5f;
    samples[3] = (1+std::sin(t*0.56345f+std::cos(t*1.63f)*0.14f))*0.5f;
    samples[4] = (1+std::sin(t*1.6245f+std::cos(t*0.254f)*0.3f))*0.5f;
    samples[5] = (1+std::sin(t*0.345f+std::cos(t*0.03f)*0.6f))*0.5f;

    for (int i = 0; i < 6; ++i) {
        sx[i] = x + i * dx;
        sy[i] = y + h * samples[i] * 0.8f;
    }

    NVGpaint bg = nvgLinearGradient(vg, x, y, x, y+h,
        nvgRGBA(0,160,192,0), nvgRGBA(0,160,192,64));
    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0]);
    for (int i = 1; i < 6; ++i)
        nvgBezierTo(vg, sx[i-1]+dx*0.5f, sy[i-1],
                         sx[i]-dx*0.5f, sy[i], sx[i], sy[i]);
    nvgLineTo(vg, x+w, y+h);
    nvgLineTo(vg, x, y+h);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0]+2);
    for (int i = 1; i < 6; ++i)
        nvgBezierTo(vg, sx[i-1]+dx*0.5f, sy[i-1]+2,
                         sx[i]-dx*0.5f, sy[i]+2, sx[i], sy[i]+2);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,32));
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0]);
    for (int i = 1; i < 6; ++i)
        nvgBezierTo(vg, sx[i-1]+dx*0.5f, sy[i-1],
                         sx[i]-dx*0.5f, sy[i], sx[i], sy[i]);
    nvgStrokeColor(vg, nvgRGBA(0,160,192,255));
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    for (int i = 0; i < 6; ++i) {
        bg = nvgRadialGradient(vg, sx[i], sy[i]+2, 3, 8,
            nvgRGBA(0,0,0,32), nvgRGBA(0,0,0,0));
        nvgBeginPath(vg);
        nvgRect(vg, sx[i]-10, sy[i]-10+2, 20, 20);
        nvgFillPaint(vg, bg);
        nvgFill(vg);
    }
    nvgBeginPath(vg);
    for (int i = 0; i < 6; ++i)
        nvgCircle(vg, sx[i], sy[i], 4.0f);
    nvgFillColor(vg, nvgRGBA(0,160,192,255));
    nvgFill(vg);
    nvgBeginPath(vg);
    for (int i = 0; i < 6; ++i)
        nvgCircle(vg, sx[i], sy[i], 2.0f);
    nvgFillColor(vg, nvgRGBA(220,220,220,255));
    nvgFill(vg);
    nvgStrokeWidth(vg, 1.0f);
}

void drawSpinner(NVGcontext* vg, float cx, float cy, float r, float t) {
    float a0 = 0.0f + t*6, a1 = NVG_PI + t*6;
    float r0 = r, r1 = r * 0.75f;
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, r0, a0, a1, NVG_CW);
    nvgArc(vg, cx, cy, r1, a1, a0, NVG_CCW);
    nvgClosePath(vg);
    float ax = cx + std::cos(a0)*(r0+r1)*0.5f;
    float ay = cy + std::sin(a0)*(r0+r1)*0.5f;
    float bx = cx + std::cos(a1)*(r0+r1)*0.5f;
    float by = cy + std::sin(a1)*(r0+r1)*0.5f;
    NVGpaint paint = nvgLinearGradient(vg, ax, ay, bx, by,
        nvgRGBA(0,0,0,0), nvgRGBA(0,0,0,128));
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    nvgRestore(vg);
}

void drawColorwheel(NVGcontext* vg, float x, float y,
                    float w, float h, float t) {
    float hue = std::sin(t * 0.12f);
    float cx = x + w*0.5f, cy = y + h*0.5f;
    float r1 = (w < h ? w : h) * 0.5f - 5.0f;
    float r0 = r1 - 20.0f;
    float aeps = 0.5f / r1;

    nvgSave(vg);
    for (int i = 0; i < 6; ++i) {
        float a0 = (float)i / 6.0f * NVG_PI * 2.0f - aeps;
        float a1 = (float)(i+1) / 6.0f * NVG_PI * 2.0f + aeps;
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r0, a0, a1, NVG_CW);
        nvgArc(vg, cx, cy, r1, a1, a0, NVG_CCW);
        nvgClosePath(vg);
        float ax = cx + std::cos(a0)*(r0+r1)*0.5f;
        float ay = cy + std::sin(a0)*(r0+r1)*0.5f;
        float bx = cx + std::cos(a1)*(r0+r1)*0.5f;
        float by = cy + std::sin(a1)*(r0+r1)*0.5f;
        NVGpaint paint = nvgLinearGradient(vg, ax, ay, bx, by,
            nvgHSLA(a0/(NVG_PI*2), 1.0f, 0.55f, 255),
            nvgHSLA(a1/(NVG_PI*2), 1.0f, 0.55f, 255));
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r0-0.5f);
    nvgCircle(vg, cx, cy, r1+0.5f);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,64));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgSave(vg);
    nvgTranslate(vg, cx, cy);
    nvgRotate(vg, hue * NVG_PI * 2);

    nvgStrokeWidth(vg, 2.0f);
    nvgBeginPath(vg);
    nvgRect(vg, r0-1, -3, r1-r0+2, 6);
    nvgStrokeColor(vg, nvgRGBA(255,255,255,192));
    nvgStroke(vg);

    NVGpaint paint = nvgBoxGradient(vg, r0-3, -5, r1-r0+6, 10, 2, 4,
        nvgRGBA(0,0,0,128), nvgRGBA(0,0,0,0));
    nvgBeginPath(vg);
    nvgRect(vg, r0-2-10, -4-10, r1-r0+4+20, 8+20);
    nvgRect(vg, r0-2, -4, r1-r0+4, 8);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, paint);
    nvgFill(vg);

    float r = r0 - 6;
    float tax = std::cos(120.0f/180.0f*NVG_PI) * r;
    float tay = std::sin(120.0f/180.0f*NVG_PI) * r;
    float tbx = std::cos(-120.0f/180.0f*NVG_PI) * r;
    float tby = std::sin(-120.0f/180.0f*NVG_PI) * r;
    nvgBeginPath(vg);
    nvgMoveTo(vg, r, 0);
    nvgLineTo(vg, tax, tay);
    nvgLineTo(vg, tbx, tby);
    nvgClosePath(vg);
    paint = nvgLinearGradient(vg, r, 0, tax, tay,
        nvgHSLA(hue, 1.0f, 0.5f, 255), nvgRGBA(255,255,255,255));
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    paint = nvgLinearGradient(vg, (r+tax)*0.5f, tay*0.5f, tbx, tby,
        nvgRGBA(0,0,0,0), nvgRGBA(0,0,0,255));
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(0,0,0,64));
    nvgStroke(vg);

    float sx = std::cos(120.0f/180.0f*NVG_PI) * r * 0.3f;
    float sy = std::sin(120.0f/180.0f*NVG_PI) * r * 0.4f;
    nvgStrokeWidth(vg, 2.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, sx, sy, 5);
    nvgStrokeColor(vg, nvgRGBA(255,255,255,192));
    nvgStroke(vg);

    paint = nvgRadialGradient(vg, sx, sy, 7, 9,
        nvgRGBA(0,0,0,64), nvgRGBA(0,0,0,0));
    nvgBeginPath(vg);
    nvgRect(vg, sx-20, sy-20, 40, 40);
    nvgCircle(vg, sx, sy, 7);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, paint);
    nvgFill(vg);

    nvgRestore(vg);
    nvgRestore(vg);
}

void draw(NVGcontext* vg, float width, float height, float time) {
    float mx = width*0.5f + std::cos(time*0.6f) * width*0.3f;
    float my = height*0.4f + std::sin(time*0.8f) * height*0.2f;

    drawEyes(vg, width - 220, 10, 130, 90, mx, my, time);
    drawGraph(vg, 0, height/2, width, height/2, time);
    drawColorwheel(vg, width - 260, height - 260, 230, 230, time);

    drawWindow(vg, "Widgets Stuff", 10, 10, 260, 340);
    float x = 20, y = 55;
    drawSearchBox(vg, "Search", x, y, 240, 25);
    y += 40;
    drawDropDown(vg, "Effects", x, y, 240, 28);
    y += 45;

    drawLabel(vg, "Login", x, y, 240, 20);
    y += 25;
    drawEditBox(vg, "Email", x, y, 240, 28);
    y += 35;
    drawEditBox(vg, "Password", x, y, 240, 28);
    y += 38;
    drawCheckBox(vg, "Remember me", x, y, 140, 28);
    drawButton(vg, "Sign in", x+138, y, 100, 28, nvgRGBA(0,96,128,255));
    y += 45;

    drawLabel(vg, "Diameter", x, y, 240, 20);
    y += 25;
    drawEditBoxNum(vg, "123.00", "px", x+150, y, 90, 28);
    drawSlider(vg, 0.4f, x, y, 140, 28);

    drawSpinner(vg, width/2, height/2, 40, time);
}`;

export const shapesExample = `#include "nanovg.h"
#include <cmath>

void circle(NVGcontext* vg, float x, float y, float radius, NVGcolor color) {
    nvgBeginPath(vg);
    nvgCircle(vg, x, y, radius);
    nvgFillColor(vg, color);
    nvgFill(vg);
}

void draw(NVGcontext* vg, float width, float height, float time) {
    for (int i = 0; i < 7; ++i) {
        float radius = 20 + 8 * std::sin(time * 2 + i * 0.6f);
        circle(vg, 64 + i * 84, height / 2, radius,
               nvgHSL(i / 7.0f, 0.75f, 0.6f));
    }
}`;

export const paintsExample = `#include "nanovg.h"

void draw(NVGcontext* vg, float width, float height, float time) {
    NVGpaint gradient = nvgLinearGradient(
        vg, 40, 0, width - 40, 0,
        nvgRGB(14, 165, 233), nvgRGB(168, 85, 247));

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 40, 40, width - 80, height - 80, 24);
    nvgFillPaint(vg, gradient);
    nvgFill(vg);

    nvgFontFace(vg, "sans");
    nvgFontSize(vg, 28);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgText(vg, width / 2, height / 2, "NanoVG + C++", nullptr);
}`;

export const pathsExample = `#include "nanovg.h"
#include <cmath>

void draw(NVGcontext* vg, float width, float height, float time) {
    // Triangle from line segments
    nvgBeginPath(vg);
    nvgMoveTo(vg, 60, 40);
    nvgLineTo(vg, 120, 160);
    nvgLineTo(vg, 0, 160);
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(14, 165, 233));
    nvgFill(vg);

    // Cubic bezier curve
    nvgBeginPath(vg);
    nvgMoveTo(vg, 160, 160);
    nvgBezierTo(vg, 180, 40, 260, 180, 280, 60);
    nvgStrokeColor(vg, nvgRGB(139, 92, 246));
    nvgStrokeWidth(vg, 3);
    nvgStroke(vg);

    // Rounded rectangle
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 320, 50, 120, 100, 16);
    nvgFillColor(vg, nvgRGB(16, 185, 129));
    nvgFill(vg);

    // Arc with animated sweep
    float sweep = 1.5f + 0.5f * std::sin(time * 2);
    nvgBeginPath(vg);
    nvgArc(vg, 530, 110, 50, 0, sweep * 3.14159f, NVG_CW);
    nvgStrokeColor(vg, nvgRGB(251, 146, 60));
    nvgStrokeWidth(vg, 4);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);
}`;

export const contextExample = `#include "nanovg.h"
#include <cmath>

void draw(NVGcontext* vg, float width, float height, float time) {
    float cx = width / 2, cy = height / 2;

    // Background circle
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 80);
    nvgFillColor(vg, nvgRGB(30, 41, 59));
    nvgFill(vg);

    // Overlapping translucent circles
    for (int i = 0; i < 5; ++i) {
        float angle = time + i * 1.2566f;
        float x = cx + 40 * std::cos(angle);
        float y = cy + 40 * std::sin(angle);
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, 36);
        nvgFillColor(vg, nvgHSLA(i / 5.0f, 0.8f, 0.6f, 160));
        nvgFill(vg);
    }

    // Center label
    nvgFontFace(vg, "sans");
    nvgFontSize(vg, 16);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgText(vg, cx, cy, "frame", nullptr);
}`;

export const stateExample = `#include "nanovg.h"

void card(NVGcontext* vg, float w, float h, NVGcolor color) {
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0, 0, w, h, 8);
    nvgFillColor(vg, color);
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 60));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
}

void draw(NVGcontext* vg, float width, float height, float time) {
    NVGcolor colors[] = {
        nvgRGB(14, 165, 233),
        nvgRGB(139, 92, 246),
        nvgRGB(16, 185, 129),
        nvgRGB(251, 146, 60),
        nvgRGB(244, 63, 94)
    };

    for (int i = 0; i < 5; ++i) {
        nvgSave(vg);
        nvgTranslate(vg, 24 + i * 120, 50 + i * 16);
        nvgGlobalAlpha(vg, 0.6f + 0.4f * (i / 4.0f));
        card(vg, 100, 80, colors[i]);
        nvgFontFace(vg, "sans");
        nvgFontSize(vg, 14);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgText(vg, 50, 40, "save/restore", nullptr);
        nvgRestore(vg);
    }
}`;

export const transformsExample = `#include "nanovg.h"
#include <cmath>

void draw(NVGcontext* vg, float width, float height, float time) {
    float cx = width / 2, cy = height / 2;

    for (int i = 0; i < 8; ++i) {
        float angle = i * 0.7854f + time * 0.4f;
        float dist = 70 + 10 * std::sin(time + i);
        float scale = 0.5f + 0.3f * (i / 7.0f);

        nvgSave(vg);
        nvgTranslate(vg, cx + dist * std::cos(angle),
                         cy + dist * std::sin(angle));
        nvgRotate(vg, angle);
        nvgScale(vg, scale, scale);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, -30, -20, 60, 40, 6);
        nvgFillColor(vg, nvgHSL(i / 8.0f, 0.75f, 0.55f));
        nvgFill(vg);

        nvgRestore(vg);
    }

    nvgFontFace(vg, "sans");
    nvgFontSize(vg, 14);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(148, 163, 184));
    nvgText(vg, cx, cy, "transforms", nullptr);
}`;

export const imagesExample = `#include "nanovg.h"
#include <algorithm>

extern "C" int preview_get_image();

void draw(NVGcontext* vg, float width, float height, float time) {
    int img = preview_get_image();
    if (img == 0) return;

    int iw, ih;
    nvgImageSize(vg, img, &iw, &ih);
    if (iw == 0 || ih == 0) return;

    float x = 40, y = 20, w = width - 80, h = height - 40;
    float scale = std::min(w / (float)iw, h / (float)ih);
    float pw = iw * scale, ph = ih * scale;
    float px = x + (w - pw) / 2, py = y + (h - ph) / 2;

    NVGpaint pattern = nvgImagePattern(
        vg, px, py, pw, ph, 0, img, 1.0f);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, px, py, pw, ph, 12);
    nvgFillPaint(vg, pattern);
    nvgFill(vg);

    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 80));
    nvgStrokeWidth(vg, 2);
    nvgStroke(vg);
}`;

export const imageFiltersExample = `#include "nanovg.h"
#include <algorithm>

#define TILE_COUNT 9

extern "C" {
    int preview_get_image();
    unsigned char* preview_get_image_pixels();
    int preview_get_image_width();
    int preview_get_image_height();
}

static int images[TILE_COUNT];
static int srcW, srcH;
static bool built = false;

void buildImages(NVGcontext* vg) {
    unsigned char* pixels = preview_get_image_pixels();
    srcW = preview_get_image_width();
    srcH = preview_get_image_height();
    if (!pixels || srcW == 0 || srcH == 0) return;

    const float emboss[] = {-2,-1,0, -1,1,1, 0,1,2};
    NVGpixelBuffer source = { pixels, srcW, srcH, 0, 0 };
    NVGfilter filters[TILE_COUNT];

    filters[1] = nvgFilterInit(NVG_FILTER_GRAYSCALE);
    filters[2] = nvgFilterInit(NVG_FILTER_BRIGHTNESS_CONTRAST);
    filters[2].brightness = 0.08f; filters[2].contrast = 1.35f;
    filters[3] = nvgFilterInit(NVG_FILTER_BOX_BLUR);
    filters[3].radiusX = filters[3].radiusY = 4;
    filters[4] = nvgFilterInit(NVG_FILTER_GAUSSIAN_BLUR);
    filters[4].sigma = 2.5f;
    filters[5] = nvgFilterInit(NVG_FILTER_SHARPEN);
    filters[5].amount = 0.65f;
    filters[6] = nvgFilterInit(NVG_FILTER_UNSHARP_MASK);
    filters[6].sigma = 2; filters[6].amount = 1.5f;
    filters[7] = nvgFilterInit(NVG_FILTER_SOBEL);
    filters[8] = nvgFilterInit(NVG_FILTER_CONVOLUTION);
    filters[8].kernel = emboss;
    filters[8].kernelWidth = filters[8].kernelHeight = 3;
    filters[8].divisor = 1; filters[8].bias = 0.5f;

    images[0] = preview_get_image();
    for (int i = 1; i < TILE_COUNT; ++i) {
        nvgCreateFilteredImageRGBA(vg, &source, 0,
            &filters[i], 1, &images[i]);
    }
}

void draw(NVGcontext* vg, float width, float height, float time) {
    if (!built) {
        buildImages(vg);
        built = true;
    }
    if (srcW == 0 || srcH == 0) return;

    int cols = 3, rows = 3;
    float pad = 8;
    float tileW = (width - pad * (cols + 1)) / cols;
    float tileH = (height - pad * (rows + 1)) / rows;

    for (int i = 0; i < TILE_COUNT; ++i) {
        int col = i % cols, row = i / cols;
        float x = pad + col * (tileW + pad);
        float y = pad + row * (tileH + pad);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, tileW, tileH, 6);
        nvgFillColor(vg, nvgRGB(25, 34, 49));
        nvgFill(vg);

        if (images[i]) {
            float scale = std::max(tileW / (float)srcW, tileH / (float)srcH);
            float iw = srcW * scale, ih = srcH * scale;
            float ix = x + (tileW - iw) / 2;
            float iy = y + (tileH - ih) / 2;

            nvgSave(vg);
            nvgIntersectScissor(vg, x, y, tileW, tileH);
            NVGpaint pat = nvgImagePattern(
                vg, ix, iy, iw, ih, 0, images[i], 1.0f);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x, y, tileW, tileH, 6);
            nvgFillPaint(vg, pat);
            nvgFill(vg);
            nvgRestore(vg);
        }
    }
}`;

export const textExample = `#include "nanovg.h"

void draw(NVGcontext* vg, float width, float height, float time) {
    nvgFontFace(vg, "sans");

    // Title - left aligned
    nvgFontSize(vg, 32);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(226, 232, 240));
    nvgText(vg, 32, 24, "NanoVG Text", nullptr);

    // Subtitle
    nvgFontSize(vg, 18);
    nvgFillColor(vg, nvgRGB(148, 163, 184));
    nvgText(vg, 32, 64, "Lightweight 2D vector graphics", nullptr);

    // Center aligned body
    nvgFontSize(vg, 15);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(100, 116, 139));
    nvgText(vg, width / 2, height / 2 + 16,
        "Fonts are rasterized into atlas textures.", nullptr);

    // Right aligned small text
    nvgFontSize(vg, 13);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    nvgFillColor(vg, nvgRGB(71, 85, 105));
    nvgText(vg, width - 32, height - 24,
        "UTF-8 strings with per-glyph positioning", nullptr);

    // Decorative line under title
    nvgBeginPath(vg);
    nvgMoveTo(vg, 32, 94);
    nvgLineTo(vg, width - 32, 94);
    nvgStrokeColor(vg, nvgRGBA(148, 163, 184, 60));
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);
}`;

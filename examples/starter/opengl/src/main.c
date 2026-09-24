#include <stdio.h>

#ifdef NANOVG_STARTER_GLEW
#include <GL/glew.h>
#endif
#ifdef __APPLE__
#define GLFW_INCLUDE_GLCOREARB
#endif
#include <GLFW/glfw3.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

static void glfw_error(int code, const char* message)
{
    fprintf(stderr, "GLFW error %d: %s\n", code, message);
}

int main(void)
{
    GLFWwindow* window;
    NVGcontext* vg;
    int font;

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    window = glfwCreateWindow(800, 520, "NanoVG OpenGL starter", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#ifdef NANOVG_STARTER_GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Could not initialize GLEW.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError(); /* GLEW can leave a harmless GL_INVALID_ENUM behind. */
#endif

    vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (vg == NULL) {
        fprintf(stderr, "Could not create the NanoVG context.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    font = nvgCreateFont(vg, "sans", "assets/Roboto-Regular.ttf");
    if (font == -1) {
        fprintf(stderr, "Could not load assets/Roboto-Regular.ttf.\n");
        nvgDeleteGL3(vg);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        int window_width, window_height, framebuffer_width, framebuffer_height;
        float pixel_ratio;
        NVGpaint background;

        glfwGetWindowSize(window, &window_width, &window_height);
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        pixel_ratio = window_width > 0
            ? (float)framebuffer_width / (float)window_width
            : 1.0f;

        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.055f, 0.067f, 0.090f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(vg, (float)window_width, (float)window_height, pixel_ratio);

        background = nvgLinearGradient(vg, 80.0f, 80.0f, 720.0f, 440.0f,
            nvgRGB(56, 189, 248), nvgRGB(139, 92, 246));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 80.0f, 80.0f,
            (float)window_width - 160.0f, (float)window_height - 160.0f, 24.0f);
        nvgFillPaint(vg, background);
        nvgFill(vg);

        nvgFontFace(vg, "sans");
        nvgFontSize(vg, 54.0f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgText(vg, (float)window_width * 0.5f,
            (float)window_height * 0.5f - 14.0f, "Hello, NanoVG!", NULL);

        nvgFontSize(vg, 20.0f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 190));
        nvgText(vg, (float)window_width * 0.5f,
            (float)window_height * 0.5f + 42.0f,
            "Your first vector frame", NULL);

        nvgEndFrame(vg);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    nvgDeleteGL3(vg);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

# NanoVG OpenGL starter

This is a minimal, standalone NanoVG application using OpenGL 3, GLFW, and
GLEW (GLEW is not needed on macOS). The download is organized like a small
application rather than a single-file demo:

```text
nanovg-opengl-starter/
|-- assets/
|   `-- Roboto-Regular.ttf
|-- extern/
|   `-- nanovg/
|       |-- include/nanovg/
|       |-- src/
|       `-- third_party/
|-- src/
|   `-- main.c
|-- CMakeLists.txt
`-- README.md
```

## Build

You need CMake, a C compiler, Git, and OpenGL development files. GLFW and GLEW
are used from installed CMake packages when available; otherwise CMake fetches
their pinned releases during configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run `build/nanovg_opengl` with a single-configuration generator, or
`build/Release/nanovg_opengl.exe` with a Visual Studio Release build. CMake
copies `assets/` beside the executable, so it can be launched directly from
its output directory.

The window and OpenGL context belong to the application. NanoVG is created
after the context becomes current, draws between `nvgBeginFrame()` and
`nvgEndFrame()`, and is destroyed before the window and context.

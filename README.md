NanoVG
==========

NanoVG is small antialiased vector graphics rendering library for OpenGL. It has lean API modeled after HTML5 canvas API. It is aimed to be a practical and fun toolset for building scalable user interfaces and visualizations.

> [!WARNING]
> This repository collects NanoVG ports and related implementations.
>
> For the official upstream NanoVG source, prefer:
> [memononen/nanovg](https://github.com/memononen/nanovg)

## Playground

[![playground](https://github.com/user-attachments/assets/c0675fd2-e816-4ef7-9090-7e4805ba79e0)](https://nika0000.github.io/nanovg/)

Browser [playground](https://nika0000.github.io/nanovg/) for this fork, JS mirrors the C API 1:1. Build/run locally: [`playground/README.md`](/playground/README.md).

Usage
=====

The NanoVG API is modeled loosely on HTML5 canvas API. If you know canvas, you're up to speed with NanoVG in no time.

## Creating drawing context

The drawing context is created using platform specific constructor function. If you're using the OpenGL 2.0 back-end the context is created as follows:
```C
#define NANOVG_GL2_IMPLEMENTATION	// Use GL2 implementation.
#include "nanovg_gl.h"
...
struct NVGcontext* vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
```

The first parameter defines flags for creating the renderer.

- `NVG_ANTIALIAS` means that the renderer adjusts the geometry to include anti-aliasing. If you're using MSAA, you can omit this flags. 
- `NVG_STENCIL_STROKES` means that the render uses better quality rendering for (overlapping) strokes. The quality is mostly visible on wider strokes. If you want speed, you can omit this flag.

Currently there is an OpenGL back-end for NanoVG: [nanovg_gl.h](/include/nanovg/nanovg_gl.h) for OpenGL 2.0, OpenGL ES 2.0, OpenGL 3.2 core profile and OpenGL ES 3. The implementation can be chosen using a define as in above example. See the header file and examples for further info. 

*NOTE:* The render target you're rendering to must have stencil buffer.

## Drawing shapes with NanoVG

Drawing a simple shape using NanoVG consists of four steps: 1) begin a new shape, 2) define the path to draw, 3) set fill or stroke, 4) and finally fill or stroke the path.

```C
nvgBeginPath(vg);
nvgRect(vg, 100,100, 120,30);
nvgFillColor(vg, nvgRGBA(255,192,0,255));
nvgFill(vg);
```

Calling `nvgBeginPath()` will clear any existing paths and start drawing from blank slate. There are number of number of functions to define the path to draw, such as rectangle, rounded rectangle and ellipse, or you can use the common moveTo, lineTo, bezierTo and arcTo API to compose the paths step by step.

## Understanding Composite Paths

Because of the way the rendering backend is build in NanoVG, drawing a composite path, that is path consisting from multiple paths defining holes and fills, is a bit more involved. NanoVG uses even-odd filling rule and by default the paths are wound in counter clockwise order. Keep that in mind when drawing using the low level draw API. In order to wind one of the predefined shapes as a hole, you should call `nvgPathWinding(vg, NVG_HOLE)`, or `nvgPathWinding(vg, NVG_CW)` _after_ defining the path.

``` C
nvgBeginPath(vg);
nvgRect(vg, 100,100, 120,30);
nvgCircle(vg, 120,120, 5);
nvgPathWinding(vg, NVG_HOLE);	// Mark circle as a hole.
nvgFillColor(vg, nvgRGBA(255,192,0,255));
nvgFill(vg);
```

## Rendering is wrong, what to do?

- make sure you have created NanoVG context using one of the `nvgCreatexxx()` calls
- make sure you have initialised OpenGL with *stencil buffer*
- make sure you have cleared stencil buffer
- make sure all rendering calls happen between `nvgBeginFrame()` and `nvgEndFrame()`
- to enable more checks for OpenGL errors, add `NVG_DEBUG` flag to `nvgCreatexxx()`
- if the problem still persists, please report an issue!

## OpenGL state touched by the backend

The OpenGL back-end touches following states:

When textures are uploaded or updated, the following pixel store is set to defaults: `GL_UNPACK_ALIGNMENT`, `GL_UNPACK_ROW_LENGTH`, `GL_UNPACK_SKIP_PIXELS`, `GL_UNPACK_SKIP_ROWS`. Texture binding is also affected. Texture updates can happen when the user loads images, or when new font glyphs are added. Glyphs are added as needed between calls to  `nvgBeginFrame()` and `nvgEndFrame()`.

The data for the whole frame is buffered and flushed in `nvgEndFrame()`. The following code illustrates the OpenGL state touched by the rendering code:
```C
    glUseProgram(prog);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0xffffffff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_UNIFORM_BUFFER, buf);
    glBindVertexArray(arr);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniformBlockBinding(... , GLNVG_FRAG_BINDING);
```

## Backends

Same pattern as above, just swap the header/define/create call:

| Backend | Header | Define | Create call |
|---|---|---|---|
| OpenGL 2 | [`nanovg_gl.h`](/include/nanovg/nanovg_gl.h) | `NANOVG_GL2_IMPLEMENTATION` | `nvgCreateGL2()` |
| OpenGL 3 core | [`nanovg_gl.h`](/include/nanovg/nanovg_gl.h) | `NANOVG_GL3_IMPLEMENTATION` | `nvgCreateGL3()` |
| OpenGL ES 2 | [`nanovg_gl.h`](/include/nanovg/nanovg_gl.h) | `NANOVG_GLES2_IMPLEMENTATION` | `nvgCreateGLES2()` |
| OpenGL ES 3 | [`nanovg_gl.h`](/include/nanovg/nanovg_gl.h) | `NANOVG_GLES3_IMPLEMENTATION` | `nvgCreateGLES3()` |
| Direct3D 11 | [`nanovg_d3d11.h`](/include/nanovg/backends/nanovg_d3d11.h) | `NANOVG_D3D11_IMPLEMENTATION` | `nvgCreateD3D11()` |
| Vulkan | [`nanovg_vk.h`](/include/nanovg/backends/nanovg_vk.h) | `NANOVG_VULKAN_IMPLEMENTATION` | `nvgCreateVk()` |
| Metal | [`nanovg_mtl.h`](/include/nanovg/backend/nanovg_mtl.h) | Obj-C, `src/metal/nanovg_mtl.m` | `nvgCreateMTL()` |
| deko3d (Switch) | [`nanovg_dk.h`](/include/nanovg/backends/nanovg_dk.h) | `src/deko3d/*.cpp` | `nvgCreateDk()` |
| PS4 | [`nanovg_ps4.h`](/include/nanovg/backends/nanovg_ps4.h) | `src/ps4/nanovg_ps4.c` | `nvgCreatePS4()` |
| WebGPU | [`nanovg_wgpu.h`](/include/nanovg/nanovg_wgpu.h) | `NANOVG_WGPU_IMPLEMENTATION` | `nvgCreateWgpu()` |

Vulkan needs `dynamicRendering` + `synchronization2` device features and
`vkCmdBeginRendering`/`vkCmdEndRendering` around the frame (see `VKNVGCreateInfo`
in the header). Everything else about drawing is identical across backends.

WebGPU targets the standard C `webgpu.h` header (Dawn, wgpu-native, or Emscripten's
`emdawnwebgpu`), not a specific implementation — include it before `nanovg_wgpu.h`.
Begin a render pass with one color attachment and a depth-stencil attachment matching
`WGPUNVGCreateInfo.depthStencilFormat` (stencil cleared to 0), call
`nvgWgpuBindRenderPass()` before any draw calls, then proceed as usual.

To build and run [`example_wgpu.c`](/examples/example_wgpu.c) against
[Dawn](https://dawn.googlesource.com/dawn) (no depot_tools required):

```bash
git clone --depth 1 https://dawn.googlesource.com/dawn dawn
cmake -B dawn/build -S dawn -DCMAKE_BUILD_TYPE=Release \
    -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_BUILD_SAMPLES=OFF -DTINT_BUILD_TESTS=OFF \
    -DDAWN_BUILD_NODE_BINDINGS=OFF -DDAWN_ENABLE_INSTALL=ON
cmake --build dawn/build --target webgpu_dawn --config Release -j
cmake --install dawn/build --prefix dawn/install --config Release

cmake -B build -DCMAKE_BUILD_TYPE=Release -DNANOVG_BUILD_EXAMPLES=ON \
    -DNANOVG_PLATFORM_WEBGPU=ON -DCMAKE_PREFIX_PATH="$PWD/dawn/install"
cmake --build build --config Release --target example_wgpu

./build/example_wgpu   # or build/Release/example_wgpu.exe on Windows
```

`Dawn_DIR`/`CMAKE_PREFIX_PATH` must point at a Dawn install tree so
`find_package(Dawn)` can resolve the `dawn::webgpu_dawn` target; on wgpu-native
or Emscripten set `NANOVG_WEBGPU_TARGET` to the appropriate target/link flags instead.

## API Reference

See the header file [nanovg.h](/include/nanovg/nanovg.h) for API reference.

## Ports

- [DX11 port](https://github.com/cmaughan/nanovg) by [Chris Maughan](https://github.com/cmaughan)
- [Metal port](https://github.com/ollix/MetalNanoVG) by [Olli Wang](https://github.com/olliwang)
- [bgfx port](https://github.com/bkaradzic/bgfx/tree/master/examples/20-nanovg) by [Branimir Karadžić](https://github.com/bkaradzic)

## Projects using NanoVG

- [Processing API simulation by vinjn](https://github.com/island-org/island/blob/master/include/sketch2d.h)
- [NanoVG for .NET, C# P/Invoke binding](https://github.com/sbarisic/nanovg_dotnet)

## License
The library is licensed under [zlib license](LICENSE.txt)
Fonts used in examples:
- Roboto licensed under [Apache license](http://www.apache.org/licenses/LICENSE-2.0)
- Entypo licensed under CC BY-SA 4.0.
- Noto Emoji licensed under [SIL Open Font License, Version 1.1](http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=OFL)

## Discussions
[NanoVG mailing list](https://groups.google.com/forum/#!forum/nanovg)

## Links
Uses [stb_truetype](http://nothings.org) (or, optionally, [freetype](http://freetype.org)) for font rendering.
Uses [stb_image](http://nothings.org) for image loading.

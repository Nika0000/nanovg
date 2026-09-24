# NanoVG WebGPU starter

This starter contains surface/device creation, a depth-stencil target, render
pass binding, NanoVG drawing, resize handling, submission, and presentation.

Provide a Dawn or wgpu-native CMake target when configuring:

```sh
cmake -S . -B build -DNANOVG_WEBGPU_TARGET=your_webgpu_target
cmake --build build --config Release
```


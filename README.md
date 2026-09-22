<h3 align="center">NanoVG</h3>
<p align="center">Antialiased 2D vector graphics library for C and C++</p>

<p align="center">
  <a href="https://github.com/Nika0000/nanovg/releases"><img src="https://img.shields.io/badge/version-0.1.0-blue" alt="Version"></a>
  <a href="https://github.com/Nika0000/nanovg/actions/workflows/ci.yml"><img src="https://github.com/Nika0000/nanovg/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/Nika0000/nanovg/actions/workflows/ci.yml"><img src="https://img.shields.io/badge/tests-ctest-green" alt="Tests"></a>
  <a href="https://nika0000.github.io/nanovg"><img src="https://img.shields.io/badge/docs-llms.txt-8A2BE2" alt="Docs"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-zlib-orange" alt="License"></a>
</p>

NanoVG is a lightweight, antialiased 2D vector graphics library for C and C++, designed for applications that need a small rendering layer with a straightforward API and minimal integration overhead. It can be used for user interfaces, development tools, overlays, editors, visualizations, custom controls, and other real-time 2D rendering workloads where bringing in a complete UI or graphics framework would be unnecessary.

This repository builds on the original [NanoVG](https://github.com/memononen/nanovg) project and brings together ports and implementations for a wider range of modern graphics APIs and platforms while keeping the NanoVG API consistent between them. Supported backends include OpenGL, OpenGL ES, Direct3D 11, Vulkan, Metal, WebGPU, deko3d, and PlayStation 4, allowing the same library and rendering model to be used across desktop and mobile applications, game consoles, embedded systems, and other constrained or platform-specific environments. For integration, configuration, examples, and getting started, see the [documentation](https://nika0000.github.io/nanovg/docs/). NanoVG is distributed under the [MIT license](LICENSE).

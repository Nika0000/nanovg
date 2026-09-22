# Changelog

## [0.10.2](https://github.com/Nika0000/nanovg/compare/v0.10.1...v0.10.2) (2026-09-22)


### Features

* add D3D11 example and make GLFW/GLEW optional in CMake ([c4cf8e0](https://github.com/Nika0000/nanovg/commit/c4cf8e0f0b2203ff1196875fd5ac615c807ccb77))
* add docs site and remove playground ([f547a21](https://github.com/Nika0000/nanovg/commit/f547a21e1116653fef5bcf4ad91053b8942a5e56))
* add filtered image creation from files and memory buffers ([a0f2088](https://github.com/Nika0000/nanovg/commit/a0f20881425af9a502780f718980d4897e382ffe))
* add framebuffer blur support ([e87147d](https://github.com/Nika0000/nanovg/commit/e87147db9b2102e6c024d9c6e0c931861efaff4c))
* add image filters ([24157bf](https://github.com/Nika0000/nanovg/commit/24157bff6eadaf7224427200929f8e2d708ae8e7))
* add line styles ([f10ffd1](https://github.com/Nika0000/nanovg/commit/f10ffd1bc0db9253e932807ffaeb857f123f17d2))
* add line styles ([740ea96](https://github.com/Nika0000/nanovg/commit/740ea960a5b4ffbf5faddab431a6eb97a3698d5c))
* add Metal backend CMake option ([efd7778](https://github.com/Nika0000/nanovg/commit/efd77781b1caa490005fdb8ab48fae888e3cbc43))
* add metal backend for nanovg ([33026b9](https://github.com/Nika0000/nanovg/commit/33026b9446e432a6b25b2177af2ba0775080fef3))
* add nvgBlurRegion API with GL, D3D11, and Metal backend implementations ([5240efb](https://github.com/Nika0000/nanovg/commit/5240efbe361db11bd9a21bd88216f2590af368f1))
* add playground for nanovg-web ([942f40a](https://github.com/Nika0000/nanovg/commit/942f40a84010b6fd117151bd998a4019000d993b))
* add Vulkan backend ([46f920c](https://github.com/Nika0000/nanovg/commit/46f920c9d5a72847aed8a6223c48ead10013e575))
* add wgpu backend for nanovg ([10fea45](https://github.com/Nika0000/nanovg/commit/10fea459f4cb97220bf1e64c7c35dbeee5399930))
* switch Vulkan backend to dynamic rendering ([74a53d6](https://github.com/Nika0000/nanovg/commit/74a53d6f4f20cbdf2f2f8593d6ff9dc9a2fc3047))


### Bug Fixes

* correct blur coordinate transform and D3D11 render pipeline ([82fb3c5](https://github.com/Nika0000/nanovg/commit/82fb3c5b3428d7795e716dcd5916e365427a60c7))
* exclude Vulkan example on Apple and add X11/xcb dependency on Linux ([f902da0](https://github.com/Nika0000/nanovg/commit/f902da01f1cf02c8894bcc05b67c01a0b82a19b8))
* generate Dawn WebGPU headers in CI ([c69f108](https://github.com/Nika0000/nanovg/commit/c69f108f11b32396f0e89dd33dcc4a279b172a3c))
* improve Vulkan backend API and fix compiler warnings ([03bce3d](https://github.com/Nika0000/nanovg/commit/03bce3df7dca3a65715efcd865d9d75d3cce2e18))
* link math library on Linux for examples build ([4403271](https://github.com/Nika0000/nanovg/commit/440327173e549381e4349d608485b33a2a618a32))
* metal shader for line styles ([da2627b](https://github.com/Nika0000/nanovg/commit/da2627b511768c3728bb7ff55a5fd8d88b25d12a))
* pass missing paint and scissor params to `D3Dnvg__convertPaint` function ([a89de6d](https://github.com/Nika0000/nanovg/commit/a89de6d5b01b9a0e93b8572127676461398e7b80))
* qualify NVGblendFactor cast with enum for C compilation in nanovg_vk.h ([c089d08](https://github.com/Nika0000/nanovg/commit/c089d0827ee288e15ef733cc971a006f14d96274))
* set minimum OS version flags for Metal shader compilation ([e698201](https://github.com/Nika0000/nanovg/commit/e698201d817791530bf7f4ea857e1d652830db9d))
* skip GL3 example if GLEW missing, silence macOS OpenGL deprecatio ([7985ede](https://github.com/Nika0000/nanovg/commit/7985ede5fa6c3248575949f4d6761f58f582b1df))
* snap nanovg text glyphs to pixel grid ([54c6e7a](https://github.com/Nika0000/nanovg/commit/54c6e7a479579c8d5f81efea0ae948f4015293ac))


### Reverts

* remove blur support (nvgBlurRegion API and backend implementations) ([166cbf2](https://github.com/Nika0000/nanovg/commit/166cbf22c4398bf337eeabeb6d86814b00aa614c))

## Changelog

Release notes for this NanoVG fork are maintained by release-please.

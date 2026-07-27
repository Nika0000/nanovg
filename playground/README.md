# NanoVG Playground

A browser playground for this NanoVG fork: `src/nanovg.c` plus the GLES3 backend
from `include/nanovg/nanovg_gl.h`, compiled to WebAssembly and rendering into a
WebGL2 canvas. Sketches are written in JavaScript against a 1:1 mirror of the C
API, so anything you write here ports straight to C.

## Building locally

Needs [emsdk](https://emscripten.org/docs/getting_started/downloads.html) on
`PATH` (3.1.x or newer):

```bash
emcmake cmake -B build-web -DNANOVG_BUILD_PLAYGROUND=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

The result is a self-contained static site in `build-web/playground/site/`.
ES modules need a real origin, so serve it rather than opening `index.html`:

```bash
python -m http.server 8000 -d build-web/playground/site
```

Editing anything under `www/` only needs `cmake --build build-web` to re-stage —
no wasm relink.

## Bindings

`tools/gen_bindings.py` parses `include/nanovg/nanovg.h` and emits the shim, the
JS wrapper and the docs index. The generated files are **committed**, matching
how `shaders/**` artifacts are handled, so building the playground needs no
Python. After changing the public API:

```bash
python playground/tools/gen_bindings.py
```

Three C shapes need special handling:

- **`NVGcolor`** is passed and returned by value. It flattens to four floats;
  returns go through a scratch slot the JS side reads out of `HEAPF32`.
- **`NVGpaint`** is a large by-value struct. Host-side slots keep the value and
  hand JS an integer handle, used exactly like the C value.
- **Out params** (`nvgTextBounds`, `nvgTextMetrics`, `nvgCurrentTransform`, …)
  write into the same scratch buffer; the wrapper returns an object.

## Editor

Completion is driven by the same generated docs index the reference panel uses,
so it can't drift from `nanovg.h`: each entry carries the header's doc comment
and the C signature the JS call maps to. `nvg.` opens it, `Ctrl`/`Cmd`+`Space`
forces it anywhere, matching is a subsequence (`rndrct` → `roundedRect`, and the
C name finds the JS one), and `Enter`/`Tab` accepts. Inside a call, a strip above
the caret shows the parameter list with the current argument in bold.

The members that aren't generated — `textBounds` and friends, the asset loaders,
`hex` — are documented by `EXTRA_DOCS` in `www/nvg.js`, which uses the same entry
shape. Add there when adding a manual binding, or it won't be discoverable.

Caret geometry lives in `editor.js` (it owns the padding and scroll offsets) and
assumes a monospace font: one measured advance, no per-character mirror.

## API differences from C

- No `ctx` first argument — there's one context.
- Colors are `[r, g, b, a]` arrays (0–1). `nvg.hex('#7aa2f7')` is a shortcut.
- Enum names work with or without the `NVG_` prefix.
- The optional `end` substring pointer is omitted; strings are always whole.
- Measurement functions return objects instead of filling arrays.
- Filesystem loaders are replaced by `nvg.loadFont(name, url)` and
  `nvg.loadImage(url, flags)`, which fetch and hand off to the `*Mem` variants.
- Not exposed: the internal render API, the free-standing `nvgTransform*` matrix
  helpers (do that math in JS), `nvgTextGlyphPositions` and `nvgTextBreakLines`
  (struct-array out params). See `SKIP` in the generator.

## Sketch contract

The sketch body runs once per frame between `nvgBeginFrame()` and
`nvgEndFrame()`, with `nvg`, `t`, `dt`, `frame`, `w`, `h`, `mouse` and `cache` in
scope. `cache` survives across frames and resets on recompile. Fonts `sans`,
`bold`, `light`, `icons` and `cache.img` are preloaded.

Throws are caught, located to a line via the sketch's `sourceURL`, and shown in
the gutter and console; three consecutive throws pause the loop.

## Frame inspector

`nvgw_snapshotStats()` reads `GLNVGcontext`'s per-frame counters immediately
before the flush resets them, so the panel shows what the backend actually
submitted: draw calls, vertices, paths, uniform blocks, and a per-call-type
breakdown. NanoVG queues one call per `fill`/`stroke`/`text`, so the useful
signal is *what kind* of call each one is (convex vs. stencil-then-cover) and how
many vertices the tessellator produced.

## Scope

Only the OpenGL ES 3 backend runs in a browser. The fork's other ports — D3D11,
Metal, Vulkan, deko3d, PS4 — are unaffected by anything here; nothing in
`playground/` is compiled into the `nanovg` library target.

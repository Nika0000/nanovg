// nanovg playground -- runtime.
//
// Boots the wasm module against a WebGL2 canvas and assembles the `nvg` object
// that sketch code is evaluated against.  The generated half of the API lives in
// nvg_gen.js; this file owns the marshalling helpers it depends on, the manual
// bindings, and the frame loop.

import createNanoVG from './nanovg.js';
import { ENUMS, DOCS, bindGenerated } from './nvg_gen.js';

export const CREATE_FLAGS = { ANTIALIAS: 1 << 0, STENCIL_STROKES: 1 << 1, DEBUG: 1 << 2 };

// Docs for the members that aren't generated: the reshaped out-param calls, the
// asset loaders and the sugar. Same shape as the generated DOCS entries so the
// reference panel and completion treat them identically.
const EXTRA_DOCS = [
  {
    name: 'nvgCurrentTransform', js: 'currentTransform', section: 'Transforms',
    signature: 'void nvgCurrentTransform(NVGcontext* ctx, float* xform)', args: [],
    doc: 'Returns the current transform as a 6-element array [a, b, c, d, e, f].',
  },
  {
    name: 'nvgTextBounds', js: 'textBounds', section: 'Text',
    signature: 'float nvgTextBounds(NVGcontext* ctx, float x, float y, const char* string, const char* end, float* bounds)',
    args: ['x', 'y', 'string'],
    doc: 'Measures the specified text string. Returns { advance, xmin, ymin, xmax, ymax, width, height, bounds }. Measured values are in local coordinate space.',
  },
  {
    name: 'nvgTextBoxBounds', js: 'textBoxBounds', section: 'Text',
    signature: 'void nvgTextBoxBounds(NVGcontext* ctx, float x, float y, float breakRowWidth, const char* string, const char* end, float* bounds)',
    args: ['x', 'y', 'breakRowWidth', 'string'],
    doc: 'Measures the specified multi-text string, wrapped at breakRowWidth. Returns { xmin, ymin, xmax, ymax, width, height, bounds }.',
  },
  {
    name: 'nvgTextMetrics', js: 'textMetrics', section: 'Text',
    signature: 'void nvgTextMetrics(NVGcontext* ctx, float* ascender, float* descender, float* lineh)',
    args: [],
    doc: 'Returns the vertical metrics of the current font as { ascender, descender, lineHeight }.',
  },
  {
    name: 'nvgImageSize', js: 'imageSize', section: 'Images',
    signature: 'void nvgImageSize(NVGcontext* ctx, int image, int* w, int* h)',
    args: ['image'],
    doc: 'Returns the dimensions of a created image as { width, height }.',
  },
  {
    name: 'nvgCreateFontMem', js: 'createFontMem', section: 'Text',
    signature: 'int nvgCreateFontMem(NVGcontext* ctx, const char* name, unsigned char* data, int ndata, int freeData)',
    args: ['name', 'bytes'],
    doc: 'Creates a font from a Uint8Array of TTF data and returns its handle, or -1 on failure. Prefer nvg.loadFont(), which fetches for you.',
  },
  {
    name: 'nvgCreateImageMem', js: 'createImageMem', section: 'Images',
    signature: 'int nvgCreateImageMem(NVGcontext* ctx, int imageFlags, unsigned char* data, int ndata)',
    args: ['bytes', 'flags'],
    doc: 'Creates an image by decoding a Uint8Array of PNG/JPG data. Returns a handle, 0 on failure. Prefer nvg.loadImage().',
  },
  {
    name: 'nvgCreateImageRGBA', js: 'createImageRGBA', section: 'Images',
    signature: 'int nvgCreateImageRGBA(NVGcontext* ctx, int w, int h, int imageFlags, const unsigned char* data)',
    args: ['w', 'h', 'bytes', 'flags'],
    doc: 'Creates an image from raw premultiplied RGBA bytes (w * h * 4). Returns a handle.',
  },
  {
    name: 'nvgUpdateImage', js: 'updateImage', section: 'Images',
    signature: 'void nvgUpdateImage(NVGcontext* ctx, int image, const unsigned char* data)',
    args: ['image', 'bytes'],
    doc: 'Replaces the pixel data of an existing image. Same size and format as when it was created.',
  },
  {
    name: '', js: 'loadFont', section: 'Playground extras',
    signature: 'async loadFont(name, url) -> fontId', args: ['name', 'url'],
    doc: 'Fetches a TTF and registers it under name. Playground-only: in C you would call nvgCreateFont() with a path. Results are memoized, so calling it every frame is safe. Returns a promise.',
  },
  {
    name: '', js: 'loadImage', section: 'Playground extras',
    signature: 'async loadImage(url, flags) -> imageId', args: ['url', 'flags'],
    doc: 'Fetches and decodes an image, returning a promise for its handle. Playground-only replacement for nvgCreateImage(). Memoized per url+flags.',
  },
  {
    name: '', js: 'hex', section: 'Playground extras',
    signature: "hex('#7aa2f7', alpha = 255) -> color", args: ['value', 'alpha'],
    doc: 'Shortcut for nvg.rgba() from a CSS-style hex string or a 0xRRGGBB number.',
  },
  {
    name: '', js: 'PI', kind: 'const', section: 'Playground extras',
    signature: 'PI = 3.14159265358979', args: [],
    doc: 'Math.PI, for angles in radians.',
  },
];

const STAT_LABELS = [
  'calls', 'verts', 'paths', 'uniforms', 'textures',
  'fill', 'convexFill', 'stroke', 'triangles', 'stencil',
];

// `selector` is a CSS selector for the target <canvas>; the wasm side resolves
// it and attaches its WebGL2 context to that element.
export async function createContext(selector = '#canvas') {
  const m = await createNanoVG();

  // The wasm side creates the WebGL2 context so the attributes nanovg needs
  // (stencil on, MSAA off) live next to the backend that relies on them.
  const selPtr = m._malloc(m.lengthBytesUTF8(selector) + 1);
  m.stringToUTF8(selector, selPtr, m.lengthBytesUTF8(selector) + 1);
  const ok = m._nvgw_create(selPtr, CREATE_FLAGS.ANTIALIAS | CREATE_FLAGS.STENCIL_STROKES);
  m._free(selPtr);
  if (!ok) {
    throw new Error('Could not create a WebGL2 context with a stencil buffer.');
  }

  const scratchPtr = m._nvgw_scratch();

  // Scratch string buffers.  Sketch code calls text() every frame, so reuse
  // fixed allocations instead of malloc/free churn; slots let one call take
  // more than one string without them clobbering each other.
  const strBufs = [];
  const strBuf = (slot) => {
    if (!strBufs[slot]) strBufs[slot] = { ptr: m._malloc(1024), size: 1024 };
    return strBufs[slot];
  };

  const h = {
    col(c) {
      if (typeof c === 'number') throw new TypeError('expected a color, got a number');
      return c;
    },
    paint(p) {
      if (typeof p !== 'number') throw new TypeError('expected a paint handle');
      return p;
    },
    str(s, slot) {
      const text = s == null ? '' : String(s);
      const need = m.lengthBytesUTF8(text) + 1;
      let buf = strBuf(slot);
      if (need > buf.size) {
        m._free(buf.ptr);
        const size = Math.max(need, buf.size * 2);
        buf = strBufs[slot] = { ptr: m._malloc(size), size };
      }
      m.stringToUTF8(text, buf.ptr, buf.size);
      return buf.ptr;
    },
    readColor() {
      const i = scratchPtr >> 2;
      return [m.HEAPF32[i], m.HEAPF32[i + 1], m.HEAPF32[i + 2], m.HEAPF32[i + 3]];
    },
    wrapPaint(handle) {
      return handle;
    },
    readScratch(n) {
      const i = scratchPtr >> 2;
      return Array.from(m.HEAPF32.subarray(i, i + n));
    },
  };

  const nvg = Object.create(null);
  bindGenerated(nvg, m, h);
  Object.assign(nvg, ENUMS);

  // --- manual bindings ------------------------------------------------------

  nvg.currentTransform = () => { m._nvgw_currentTransform(); return h.readScratch(6); };

  nvg.textBounds = (x, y, str) => {
    const advance = m._nvgw_textBounds(x, y, h.str(str, 0));
    const b = h.readScratch(4);
    return { advance, xmin: b[0], ymin: b[1], xmax: b[2], ymax: b[3],
             width: b[2] - b[0], height: b[3] - b[1], bounds: b };
  };

  nvg.textBoxBounds = (x, y, breakRowWidth, str) => {
    m._nvgw_textBoxBounds(x, y, breakRowWidth, h.str(str, 0));
    const b = h.readScratch(4);
    return { xmin: b[0], ymin: b[1], xmax: b[2], ymax: b[3],
             width: b[2] - b[0], height: b[3] - b[1], bounds: b };
  };

  nvg.textMetrics = () => {
    m._nvgw_textMetrics();
    const v = h.readScratch(3);
    return { ascender: v[0], descender: v[1], lineHeight: v[2] };
  };

  nvg.imageSize = (image) => {
    m._nvgw_imageSize(image);
    const v = h.readScratch(2);
    return { width: v[0], height: v[1] };
  };

  const withBytes = (bytes, fn) => {
    const ptr = m._malloc(bytes.length);
    m.HEAPU8.set(bytes, ptr);
    try {
      return fn(ptr, bytes.length);
    } finally {
      m._free(ptr);
    }
  };

  nvg.createFontMem = (name, bytes) =>
    withBytes(bytes, (ptr, n) => m._nvgw_createFontMem(h.str(name, 0), ptr, n));

  nvg.createImageMem = (bytes, flags = 0) =>
    withBytes(bytes, (ptr, n) => m._nvgw_createImageMem(flags, ptr, n));

  nvg.createImageRGBA = (w, height, bytes, flags = 0) =>
    withBytes(bytes, (ptr) => m._nvgw_createImageRGBA(w, height, flags, ptr));

  nvg.updateImage = (image, bytes) =>
    withBytes(bytes, (ptr) => m._nvgw_updateImage(image, ptr));

  // --- conveniences ---------------------------------------------------------
  // Sugar only; each maps onto the C API above so sketches stay portable.

  nvg.hex = (value, alpha = 255) => {
    const v = typeof value === 'string' ? parseInt(value.replace('#', ''), 16) : value;
    return nvg.rgba((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff, alpha);
  };

  nvg.degToRad = (deg) => (deg / 180) * Math.PI;
  nvg.radToDeg = (rad) => (rad / Math.PI) * 180;
  nvg.PI = Math.PI;

  // --- asset loading --------------------------------------------------------

  const fonts = new Map();
  const images = new Map();

  nvg.loadFont = async (name, url) => {
    if (fonts.has(name)) return fonts.get(name);
    const res = await fetch(url);
    if (!res.ok) throw new Error(`font ${name}: ${res.status} ${res.statusText}`);
    const id = nvg.createFontMem(name, new Uint8Array(await res.arrayBuffer()));
    if (id === -1) throw new Error(`font ${name}: nanovg rejected the data`);
    fonts.set(name, id);
    return id;
  };

  nvg.loadImage = async (url, flags = 0) => {
    const key = `${url}|${flags}`;
    if (images.has(key)) return images.get(key);
    const res = await fetch(url);
    if (!res.ok) throw new Error(`image ${url}: ${res.status} ${res.statusText}`);
    const id = nvg.createImageMem(new Uint8Array(await res.arrayBuffer()), flags);
    if (id === 0) throw new Error(`image ${url}: decode failed`);
    images.set(key, id);
    return id;
  };

  // --- frame driving --------------------------------------------------------

  return {
    nvg,
    docs: [...DOCS, ...EXTRA_DOCS],
    enums: ENUMS,
    beginFrame(w, height, pxRatio) { m._nvgw_beginFrame(w, height, pxRatio); },
    endFrame() { m._nvgw_endFrame(); },
    cancelFrame() { m._nvgw_cancelFrame(); },
    // Must be called between the last draw call and endFrame(): the backend
    // resets its per-frame counters during the flush.
    stats() {
      const ptr = m._nvgw_snapshotStats() >> 2;
      const raw = m.HEAP32.subarray(ptr, ptr + STAT_LABELS.length);
      const out = {};
      STAT_LABELS.forEach((label, i) => { out[label] = raw[i]; });
      return out;
    },
  };
}

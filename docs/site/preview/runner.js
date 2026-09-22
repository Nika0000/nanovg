/* WebGL2 runtime for NanoVG wasm modules produced by clang-cc1 + wasm-ld.
   Provides GL function imports, minimal libc shims, and the render loop. */

let gl, memory, wasmExports;
let nextHandle = 1;
const textures = new Map();
const buffers = new Map();
const programs = new Map();
const shaders = new Map();
const vaos = new Map();
const ulocs = new Map();
const warned = new Set();
let unpackRowLength = 0, unpackSkipPixels = 0, unpackSkipRows = 0, unpackAlignment = 4;

function handle(map, obj) { const h = nextHandle++; map.set(h, obj); return h; }
function drop(map, h) { const obj = map.get(h); map.delete(h); return obj; }

function cstr(ptr) {
  const mem = new Uint8Array(memory.buffer);
  let end = ptr;
  while (mem[end]) end++;
  return new TextDecoder().decode(mem.subarray(ptr, end));
}

function writeStr(ptr, maxLen, str) {
  const enc = new TextEncoder().encode(str);
  const n = Math.min(enc.length, maxLen - 1);
  new Uint8Array(memory.buffer, ptr, n + 1).set(enc.subarray(0, n));
  new Uint8Array(memory.buffer)[ptr + n] = 0;
  return n;
}

function i32(ptr, count) { return new Int32Array(memory.buffer, ptr, count); }
function u32(ptr, count) { return new Uint32Array(memory.buffer, ptr, count); }
function f32(ptr, count) { return new Float32Array(memory.buffer, ptr, count); }

function channelCount(fmt) {
  if (fmt === 0x1903 || fmt === 0x1906) return 1; // GL_RED, GL_ALPHA
  if (fmt === 0x1907) return 3; // GL_RGB
  return 4; // GL_RGBA
}

const glImpl = {
  glGenTextures(n, ptr) { const v = i32(ptr, n); for (let i = 0; i < n; i++) v[i] = handle(textures, gl.createTexture()); },
  glDeleteTextures(n, ptr) { const v = i32(ptr, n); for (let i = 0; i < n; i++) gl.deleteTexture(drop(textures, v[i])); },
  glBindTexture(t, h) { gl.bindTexture(t, h ? textures.get(h) : null); },
  glTexImage2D(target, level, ifmt, w, h, border, fmt, type, ptr) {
    if (ptr) {
      const ch = channelCount(fmt);
      const rowLen = unpackRowLength || w;
      const total = (unpackSkipRows + h) * rowLen * ch;
      gl.texImage2D(target, level, ifmt, w, h, border, fmt, type,
        new Uint8Array(memory.buffer, ptr, total));
    } else {
      gl.texImage2D(target, level, ifmt, w, h, border, fmt, type, null);
    }
  },
  glTexSubImage2D(target, level, xo, yo, w, h, fmt, type, ptr) {
    if (ptr) {
      const ch = channelCount(fmt);
      const rowLen = unpackRowLength || w;
      const total = (unpackSkipRows + h) * rowLen * ch;
      gl.texSubImage2D(target, level, xo, yo, w, h, fmt, type,
        new Uint8Array(memory.buffer, ptr, total));
    } else {
      gl.texSubImage2D(target, level, xo, yo, w, h, fmt, type, null);
    }
  },
  glTexParameteri(t, p, v) { gl.texParameteri(t, p, v); },
  glPixelStorei(p, v) {
    if (p === 0x0CF2) unpackRowLength = v;    // GL_UNPACK_ROW_LENGTH
    else if (p === 0x0CF4) unpackSkipPixels = v; // GL_UNPACK_SKIP_PIXELS
    else if (p === 0x0CF3) unpackSkipRows = v;   // GL_UNPACK_SKIP_ROWS
    else if (p === 0x0CF5) unpackAlignment = v;  // GL_UNPACK_ALIGNMENT
    gl.pixelStorei(p, v);
  },
  glGenerateMipmap(t) { gl.generateMipmap(t); },
  glActiveTexture(t) { gl.activeTexture(t); },

  glCreateShader(type) { return handle(shaders, gl.createShader(type)); },
  glDeleteShader(h) { gl.deleteShader(drop(shaders, h)); },
  glShaderSource(sh, count, strPtr, lenPtr) {
    const ptrs = u32(strPtr, count);
    let src = '';
    for (let j = 0; j < count; j++) {
      if (lenPtr) { const lens = i32(lenPtr, count); src += lens[j] >= 0 ? new TextDecoder().decode(new Uint8Array(memory.buffer, ptrs[j], lens[j])) : cstr(ptrs[j]); }
      else src += cstr(ptrs[j]);
    }
    gl.shaderSource(shaders.get(sh), src);
  },
  glCompileShader(h) { gl.compileShader(shaders.get(h)); },
  glGetShaderiv(h, pname, ptr) { const v = gl.getShaderParameter(shaders.get(h), pname); i32(ptr, 1)[0] = typeof v === 'boolean' ? +v : v; },
  glGetShaderInfoLog(h, max, lenPtr, bufPtr) { const n = writeStr(bufPtr, max, gl.getShaderInfoLog(shaders.get(h)) || ''); if (lenPtr) i32(lenPtr, 1)[0] = n; },
  glShaderBinary() {},

  glCreateProgram() { return handle(programs, gl.createProgram()); },
  glDeleteProgram(h) { gl.deleteProgram(drop(programs, h)); },
  glAttachShader(p, s) { gl.attachShader(programs.get(p), shaders.get(s)); },
  glLinkProgram(h) { gl.linkProgram(programs.get(h)); },
  glGetProgramiv(h, pname, ptr) { const v = gl.getProgramParameter(programs.get(h), pname); i32(ptr, 1)[0] = typeof v === 'boolean' ? +v : v; },
  glGetProgramInfoLog(h, max, lenPtr, bufPtr) { const n = writeStr(bufPtr, max, gl.getProgramInfoLog(programs.get(h)) || ''); if (lenPtr) i32(lenPtr, 1)[0] = n; },
  glUseProgram(h) { gl.useProgram(h ? programs.get(h) : null); },
  glBindAttribLocation(p, idx, namePtr) { gl.bindAttribLocation(programs.get(p), idx, cstr(namePtr)); },
  glGetUniformLocation(p, namePtr) { const loc = gl.getUniformLocation(programs.get(p), cstr(namePtr)); return loc ? handle(ulocs, loc) : -1; },
  glGetUniformBlockIndex(p, namePtr) { return gl.getUniformBlockIndex(programs.get(p), cstr(namePtr)); },
  glUniformBlockBinding(p, idx, bind) { gl.uniformBlockBinding(programs.get(p), idx, bind); },

  glUniform1i(loc, v) { if (loc > 0) gl.uniform1i(ulocs.get(loc), v); },
  glUniform1f(loc, v) { if (loc > 0) gl.uniform1f(ulocs.get(loc), v); },
  glUniform2f(loc, a, b) { if (loc > 0) gl.uniform2f(ulocs.get(loc), a, b); },
  glUniform2fv(loc, n, ptr) { if (loc > 0) gl.uniform2fv(ulocs.get(loc), f32(ptr, n * 2)); },
  glUniform4fv(loc, n, ptr) { if (loc > 0) gl.uniform4fv(ulocs.get(loc), f32(ptr, n * 4)); },

  glGenBuffers(n, ptr) { const v = i32(ptr, n); for (let k = 0; k < n; k++) v[k] = handle(buffers, gl.createBuffer()); },
  glDeleteBuffers(n, ptr) { const v = i32(ptr, n); for (let k = 0; k < n; k++) gl.deleteBuffer(drop(buffers, v[k])); },
  glBindBuffer(t, h) { gl.bindBuffer(t, h ? buffers.get(h) : null); },
  glBufferData(t, sz, ptr, usage) { gl.bufferData(t, ptr ? new Uint8Array(memory.buffer, ptr, sz) : sz, usage); },
  glBindBufferRange(t, idx, h, off, sz) { gl.bindBufferRange(t, idx, buffers.get(h), off, sz); },

  glGenVertexArrays(n, ptr) { const v = i32(ptr, n); for (let k = 0; k < n; k++) v[k] = handle(vaos, gl.createVertexArray()); },
  glDeleteVertexArrays(n, ptr) { const v = i32(ptr, n); for (let k = 0; k < n; k++) gl.deleteVertexArray(drop(vaos, v[k])); },
  glBindVertexArray(h) { gl.bindVertexArray(h ? vaos.get(h) : null); },

  glEnableVertexAttribArray(i) { gl.enableVertexAttribArray(i); },
  glDisableVertexAttribArray(i) { gl.disableVertexAttribArray(i); },
  glVertexAttribPointer(i, sz, type, norm, stride, off) { gl.vertexAttribPointer(i, sz, type, !!norm, stride, off); },

  glEnable(c) { gl.enable(c); },
  glDisable(c) { gl.disable(c); },
  glBlendFuncSeparate(sr, dr, sa, da) { gl.blendFuncSeparate(sr, dr, sa, da); },
  glColorMask(r, g, b, a) { gl.colorMask(!!r, !!g, !!b, !!a); },
  glStencilMask(m) { gl.stencilMask(m); },
  glStencilFunc(f, r, m) { gl.stencilFunc(f, r, m); },
  glStencilOp(sf, dpf, dpp) { gl.stencilOp(sf, dpf, dpp); },
  glStencilOpSeparate(face, sf, dpf, dpp) { gl.stencilOpSeparate(face, sf, dpf, dpp); },
  glCullFace(m) { gl.cullFace(m); },
  glFrontFace(m) { gl.frontFace(m); },
  glFinish() { gl.finish(); },

  glViewport(x, y, w, h) { gl.viewport(x, y, w, h); },
  glClearColor(r, g, b, a) { gl.clearColor(r, g, b, a); },
  glClear(m) { gl.clear(m); },
  glDrawArrays(mode, first, n) { gl.drawArrays(mode, first, n); },

  glGetError() { return gl.getError(); },
  glGetIntegerv(pname, ptr) {
    const v = gl.getParameter(pname);
    if (v instanceof Int32Array || Array.isArray(v)) {
      const dst = i32(ptr, v.length); for (let k = 0; k < v.length; k++) dst[k] = v[k];
    } else { i32(ptr, 1)[0] = v | 0; }
  },
};

const envShims = {
  emscripten_resize_heap(requested) {
    const delta = Math.ceil(requested / 65536) - (memory.buffer.byteLength / 65536);
    if (delta > 0) { try { memory.grow(delta); return 1; } catch { return 0; } }
    return 1;
  },
  emscripten_memcpy_js(dst, src, len) { new Uint8Array(memory.buffer).copyWithin(dst, src, src + len); },
  emscripten_get_now() { return performance.now(); },
  __assert_fail() { throw new Error('assertion failed'); },
  _abort_js() { throw new Error('abort'); },
  abort() { throw new Error('abort'); },
  __syscall_openat() { return -44; },
  __syscall_ioctl() { return -25; },
  __syscall_fcntl64() { return -25; },
  __syscall_writev(fd, iovPtr, iovcnt) {
    const dv = new DataView(memory.buffer);
    let total = 0;
    for (let i = 0; i < iovcnt; i++) {
      const p = dv.getUint32(iovPtr + i * 8, true);
      const l = dv.getUint32(iovPtr + i * 8 + 4, true);
      total += l;
    }
    return total;
  },
  __syscall_write(fd, bufPtr, count) { return count; },
  __syscall_read() { return -9; },
  __syscall_close() { return 0; },
  __syscall_stat64() { return -44; },
  __syscall_fstat64() { return -9; },
  __syscall_lstat64() { return -44; },
  __syscall_getdents64() { return -9; },
  __syscall_unlinkat() { return -1; },
  __syscall_mkdirat() { return -1; },
  emscripten_date_now() { return Date.now(); },
  _emscripten_get_now_is_monotonic() { return 1; },
  emscripten_err(ptr) { console.warn('[wasm]', cstr(ptr)); },
  emscripten_out(ptr) { console.log('[wasm]', cstr(ptr)); },
  _emscripten_throw_longjmp() { throw new Error('longjmp'); },
  __cxa_throw() { throw new Error('C++ exception'); },
  __resumeException() { throw new Error('C++ exception'); },
  __cxa_find_matching_catch_2() { return 0; },
  __cxa_find_matching_catch_3() { return 0; },
  __cxa_begin_catch() { return 0; },
  __cxa_end_catch() {},
  __cxa_allocate_exception() { return 0; },
  invoke_ii(idx, a1) { try { return wasmExports.__indirect_function_table.get(idx)(a1); } catch { return 0; } },
  invoke_iii(idx, a1, a2) { try { return wasmExports.__indirect_function_table.get(idx)(a1, a2); } catch { return 0; } },
  invoke_viii(idx, a1, a2, a3) { try { wasmExports.__indirect_function_table.get(idx)(a1, a2, a3); } catch {} },
};

const knownImports = { ...glImpl, ...envShims };

self.onmessage = async ({ data }) => {
  try {
    if (data.type === 'start') {
      gl = data.canvas.getContext('webgl2', { stencil: true, premultipliedAlpha: true, antialias: false, alpha: true });
      if (!gl) throw new Error('WebGL2 with stencil buffer is required.');

      const wasmModule = await WebAssembly.compile(data.wasm);
      const imports = {};
      for (const imp of WebAssembly.Module.imports(wasmModule)) {
        if (!imports[imp.module]) imports[imp.module] = {};
        if (imp.kind === 'function') {
          if (knownImports[imp.name]) {
            imports[imp.module][imp.name] = knownImports[imp.name];
          } else {
            if (!warned.has(imp.name)) { warned.add(imp.name); console.warn('[nanovg-runner] stub import:', imp.module + '.' + imp.name); }
            imports[imp.module][imp.name] = () => 0;
          }
        } else if (imp.kind === 'memory') {
          memory = new WebAssembly.Memory({ initial: 32, maximum: 1024 });
          imports[imp.module][imp.name] = memory;
        } else if (imp.kind === 'table') {
          imports[imp.module][imp.name] = new WebAssembly.Table({ initial: 128, maximum: 65536, element: 'anyfunc' });
        } else if (imp.kind === 'global') {
          imports[imp.module][imp.name] = new WebAssembly.Global({ value: 'i32', mutable: true }, 0);
        }
      }

      const instance = await WebAssembly.instantiate(wasmModule, imports);
      wasmExports = instance.exports;
      if (!memory && wasmExports.memory) memory = wasmExports.memory;

      if (wasmExports.__wasm_call_ctors) wasmExports.__wasm_call_ctors();
      wasmExports.preview_init();

      if (data.font) {
        const fLen = data.font.length;
        const fPtr = wasmExports.malloc(fLen);
        new Uint8Array(memory.buffer, fPtr, fLen).set(data.font);
        const name = new TextEncoder().encode('sans\0');
        const nPtr = wasmExports.malloc(name.length);
        new Uint8Array(memory.buffer, nPtr, name.length).set(name);
        wasmExports.preview_load_font(nPtr, fPtr, fLen);
        wasmExports.free(nPtr);
      }
      if (data.image) {
        const iLen = data.image.length;
        const iPtr = wasmExports.malloc(iLen);
        new Uint8Array(memory.buffer, iPtr, iLen).set(data.image);
        wasmExports.preview_load_image(iPtr, iLen);
        wasmExports.free(iPtr);
      }
      self.postMessage({ type: 'ready' });
    } else if (data.type === 'frame') {
      wasmExports.preview_frame(data.width, data.height, data.ratio, data.time);
      self.postMessage({ type: 'frame' });
    }
  } catch (err) {
    self.postMessage({ type: 'error', message: err instanceof Error ? err.message : String(err) });
  }
};

import { HeadlessIOProvider } from 'emception';
import { WorkerClient } from '@preview/worker-client';
import { wrapWorkerClient } from '@preview/facade';

let compiler;
let client;
let runWorker;
let sequence = 0;
let frameTimer;
let animation;
let buildTimer;
let libraryReady = false;
let assets;
const send = (message, busy = false, error = '') =>
  parent.postMessage({ type: 'nanovg-status', message, busy, error }, location.origin);
const ready = () => parent.postMessage({ type: 'nanovg-ready' }, location.origin);
const root = new URL('./', import.meta.url);

async function bytes(path) {
  const response = await fetch(new URL(path, root));
  if (!response.ok) throw new Error('Could not load ' + path + ' (' + response.status + ').');
  return new Uint8Array(await response.arrayBuffer());
}
function stopRenderer() {
  cancelAnimationFrame(animation);
  clearTimeout(frameTimer);
  runWorker?.terminate();
  runWorker = null;
}
function stop() {
  sequence++;
  stopRenderer();
  clearTimeout(buildTimer);
  compiler?.dispose();
  client?.terminate();
  compiler = undefined;
  client = undefined;
  libraryReady = false;
}

// IndexedDB cache for compiled .o files
const DB_NAME = 'nanovg-preview';
const STORE = 'objects';
function openDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => req.result.createObjectStore(STORE);
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}
async function getCached(key) {
  try {
    const db = await openDB();
    return await new Promise(r => {
      const tx = db.transaction(STORE, 'readonly');
      const req = tx.objectStore(STORE).get(key);
      req.onsuccess = () => r(req.result || null);
      req.onerror = () => r(null);
    });
  } catch { return null; }
}
async function setCached(key, value) {
  try {
    const db = await openDB();
    await new Promise(r => {
      const tx = db.transaction(STORE, 'readwrite');
      tx.objectStore(STORE).put(value, key);
      tx.oncomplete = () => r();
    });
  } catch { /* caching is best-effort */ }
}

let sourceHash = '';
async function boot(token) {
  if (compiler) return compiler;
  const worker = new Worker(new URL('./toolchain-worker.js', root), { type: 'module' });
  client = new WorkerClient(worker, new HeadlessIOProvider());
  await client.boot(new URL('./compiler/manifest.json', root).href);
  if (token !== sequence) throw new Error('Stopped');
  compiler = wrapWorkerClient(client);
  const source = await (await fetch(new URL('./sources.json', root))).json();
  sourceHash = '';
  for (const [path, content] of Object.entries(source)) {
    await compiler.workspace.writeFile('/home/user/' + path, content);
    sourceHash += path + ':' + content.length + ';';
  }
  return compiler;
}

async function command(em, tool, args) {
  const result = await em.run(tool, args, { cwd: '/home/user' });
  if (result.exitCode !== 0) {
    throw new Error((result.stderr || result.stdout || 'Compilation failed.').replace(/\x1b\[[0-9;]*m/g, ''));
  }
}

// Compile flags aligned with emception presets
const cc1Base = [
  'clang', '-cc1',
  '-triple', 'wasm32-unknown-emscripten',
  '-emit-obj', '-O1',
  '-disable-free', '-clear-ast-before-backend',
  '-disable-llvm-verifier', '-discard-value-names',
  '-mrelocation-model', 'static', '-mframe-pointer=none',
  '-ffp-contract=on', '-fno-rounding-math',
  '-mconstructor-aliases', '-target-cpu', 'generic',
  '-fvisibility=hidden',
];
const cc1Tail = [
  '-I/home/user/include', '-I/home/user/third_party',
  '-fdeprecated-macro', '-ferror-limit', '19', '-fgnuc-version=4.2.1',
];
const cc1C = [
  ...cc1Base,
  '-resource-dir', '/usr/lib/clang/23',
  '-internal-isystem', '/usr/lib/clang/23/include',
  '-internal-isystem', '/usr/include',
  ...cc1Tail,
  '-x', 'c',
];
const cc1Cpp = [
  ...cc1Base,
  '-internal-isystem', '/usr/include/c++/v1',
  '-internal-isystem', '/usr/include/compat',
  '-resource-dir', '/usr/lib/clang/23',
  '-internal-isystem', '/usr/lib/clang/23/include',
  '-internal-isystem', '/usr/include',
  ...cc1Tail,
  '-std=c++17', '-fcxx-exceptions', '-fexceptions',
  '-x', 'c++',
];

async function run(code, width, height) {
  if (typeof code !== 'string' || code.length > 100000) return send('Invalid source', false, 'Examples must be under 100 KB.');
  const token = ++sequence;
  stopRenderer();
  const current = () => token === sequence;
  send('Loading C++ toolchain…', true);
  buildTimer = setTimeout(() => {
    if (current()) { stop(); send('Build timed out', false, 'The build exceeded five minutes. Run again to retry.'); }
  }, 300000);
  try {
    const em = await boot(token);
    if (!current()) return;

    if (!libraryReady) {
      const cacheKey = 'obj-' + sourceHash;
      const cached = await getCached(cacheKey);
      if (cached) {
        send('Restoring cached objects…', true);
        await em.workspace.writeFile('/home/user/nanovg.o', cached.nanovgO);
        await em.workspace.writeFile('/home/user/host.o', cached.hostO);
      } else {
        send('Compiling NanoVG (first run)…', true);
        await command(em, 'clang', [...cc1C, '-main-file-name', 'nanovg.c', '-o', 'nanovg.o', 'nanovg.c']);
        if (!current()) return;
        send('Compiling host…', true);
        await command(em, 'clang', [...cc1Cpp, '-main-file-name', 'host.cpp', '-o', 'host.o', 'host.cpp']);
        if (!current()) return;
        const nanovgO = await em.workspace.readFile('/home/user/nanovg.o');
        const hostO = await em.workspace.readFile('/home/user/host.o');
        if (nanovgO && hostO) await setCached(cacheKey, { nanovgO, hostO });
      }
      libraryReady = true;
    }

    send('Compiling example…', true);
    await em.workspace.writeFile('/home/user/example.cpp', code);
    await command(em, 'clang', [...cc1Cpp, '-main-file-name', 'example.cpp', '-o', 'example.o', 'example.cpp']);
    if (!current()) return;

    send('Linking…', true);
    await command(em, 'wasm-ld', [
      'wasm-ld',
      'example.o', 'nanovg.o', 'host.o',
      '-L/usr/lib/emscripten/cache/sysroot/lib/wasm32-emscripten',
      '--no-entry', '--import-undefined', '--allow-undefined',
      '--export-table', '--table-base=1',
      '--export=__wasm_call_ctors',
      '--export=preview_init', '--export=preview_frame',
      '--export=preview_load_font', '--export=preview_load_image',
      '--export=preview_get_image', '--export=preview_get_image_pixels',
      '--export=preview_get_image_width', '--export=preview_get_image_height',
      '--export=malloc', '--export=free',
      '--export=memory',
      '-lstubs', '-lc', '-ldlmalloc', '-lcompiler_rt',
      '-lc++-noexcept', '-lc++abi-noexcept',
      '-z', 'stack-size=65536',
      '--initial-memory=2097152', '--max-memory=67108864',
      '-o', 'example.wasm',
    ]);
    if (!current()) return;

    const wasm = await em.workspace.readFile('/home/user/example.wasm');
    if (!wasm) throw new Error('The compiler did not produce a preview module.');

    assets ??= Promise.all([bytes('assets/Roboto-Regular.ttf'), bytes('assets/image1.jpg')]);
    const [font, image] = await assets;
    if (!current()) return;
    clearTimeout(buildTimer);

    const canvas = document.createElement('canvas');
    canvas.setAttribute('aria-label', 'Rendered NanoVG example');
    document.querySelector('canvas').replaceWith(canvas);
    const ratio = Math.min(devicePixelRatio || 1, 2);
    canvas.width = Math.round(width * ratio);
    canvas.height = Math.round(height * ratio);
    const surface = canvas.transferControlToOffscreen();

    runWorker = new Worker(new URL('./runner.js', root), { type: 'module' });
    const started = performance.now();
    const fail = (message) => { if (!current()) return; stopRenderer(); send('Preview stopped', false, message); };
    const nextFrame = () => {
      if (!current() || !runWorker) return;
      runWorker.postMessage({ type: 'frame', width, height, ratio, time: (performance.now() - started) / 1000 });
      frameTimer = setTimeout(() => fail('The example took more than two seconds to draw a frame. Check for an infinite loop.'), 2000);
    };
    runWorker.onmessage = ({ data }) => {
      if (!current()) return;
      if (data.type === 'error') return fail(data.message);
      if (data.type === 'ready' || data.type === 'frame') {
        clearTimeout(frameTimer);
        send('Live preview');
        animation = requestAnimationFrame(nextFrame);
      }
    };
    runWorker.onerror = (event) => fail(event.message || 'The preview worker failed.');
    frameTimer = setTimeout(() => fail('The preview could not start within 30 seconds.'), 30000);
    runWorker.postMessage({ type: 'start', wasm, canvas: surface, font, image, width, height }, [surface]);
    send('Starting preview…', true);
  } catch (error) {
    if (!current()) return;
    clearTimeout(buildTimer);
    send('Build failed', false, error instanceof Error ? error.message : String(error));
  }
}

window.addEventListener('message', ({ source, origin, data }) => {
  if (source !== parent || origin !== location.origin) return;
  if (data?.type === 'nanovg-connect') ready();
  if (data?.type === 'nanovg-stop') { stop(); send('Stopped'); }
  if (data?.type === 'nanovg-run') {
    const width = Math.max(1, Math.min(2048, Number(data.width) || 640));
    const height = Math.max(1, Math.min(2048, Number(data.height) || 240));
    void run(data.code, width, height);
  }
});
window.addEventListener('pagehide', stop);
ready();

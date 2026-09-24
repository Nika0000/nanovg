import { build } from 'esbuild';
import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repo = resolve(site, '../..');
const output = resolve(site, 'public/preview');
const browser = resolve(site, 'node_modules/@gameguild/emception-browser/dist');
await mkdir(output, { recursive: true });

// Package backend starters alongside the generated documentation. Keeping the
// file list here makes each download reproducible and prevents build output or
// unrelated example assets from slipping into an archive.
const starterRoot = resolve(repo, 'examples/starter');
const downloads = resolve(site, 'public/downloads');
const openGLPackage = resolve(downloads, 'nanovg-opengl-starter');
await mkdir(openGLPackage, { recursive: true });
const starterFiles = ['CMakeLists.txt', 'README.md', 'src/main.c'];
for (const file of starterFiles) {
  const destination = resolve(openGLPackage, file);
  await mkdir(dirname(destination), { recursive: true });
  await cp(resolve(starterRoot, 'opengl', file), destination);
}
await mkdir(resolve(openGLPackage, 'assets'), { recursive: true });
await cp(resolve(repo, 'examples/Roboto-Regular.ttf'),
  resolve(openGLPackage, 'assets/Roboto-Regular.ttf'));
const libraryFiles = ['src/nanovg.c', 'include/nanovg/nanovg.h',
  'include/nanovg/nanovg_gl.h', 'third_party/fontstash.h',
  'third_party/stb_image.h', 'third_party/stb_truetype.h', 'LICENSE'];
for (const file of libraryFiles) {
  const destination = resolve(openGLPackage, 'extern/nanovg', file);
  await mkdir(dirname(destination), { recursive: true });
  await cp(resolve(repo, file), destination);
}

function crc32(data) {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

async function writeZip(destination, entries) {
  const localParts = [];
  const centralParts = [];
  let offset = 0;

  for (const entry of entries) {
    const name = Buffer.from(entry.name.replaceAll('\\', '/'));
    const data = await readFile(entry.source);
    const checksum = crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt32LE(checksum, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(name.length, 26);
    localParts.push(local, name, data);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt32LE(checksum, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt32LE(offset, 42);
    centralParts.push(central, name);
    offset += local.length + name.length + data.length;
  }

  const centralSize = centralParts.reduce((size, part) => size + part.length, 0);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralSize, 12);
  end.writeUInt32LE(offset, 16);
  await writeFile(destination, Buffer.concat([...localParts, ...centralParts, end]));
}

await writeZip(resolve(downloads, 'nanovg-opengl-starter.zip'), [
  ...starterFiles.map((file) => ({
    name: `nanovg-opengl-starter/${file}`,
    source: resolve(openGLPackage, file),
  })),
  {
    name: 'nanovg-opengl-starter/assets/Roboto-Regular.ttf',
    source: resolve(openGLPackage, 'assets/Roboto-Regular.ttf'),
  },
  ...libraryFiles.map((file) => ({
    name: `nanovg-opengl-starter/extern/nanovg/${file}`,
    source: resolve(openGLPackage, 'extern/nanovg', file),
  })),
]);

async function packageBackendStarter(config) {
  const packageName = `nanovg-${config.name}-starter`;
  const packageRoot = resolve(downloads, packageName);
  const entries = [];

  async function add(source, archivePath, transform) {
    const destination = resolve(packageRoot, archivePath);
    await mkdir(dirname(destination), { recursive: true });
    if (transform) {
      await writeFile(destination, transform(await readFile(source, 'utf8')));
    } else {
      await cp(source, destination);
    }
    entries.push({ name: `${packageName}/${archivePath}`, source: destination });
  }

  await add(resolve(starterRoot, config.name, 'CMakeLists.txt'), 'CMakeLists.txt');
  await add(resolve(starterRoot, config.name, 'README.md'), 'README.md');
  await add(resolve(repo, `examples/${config.main}`), 'src/main.c', config.transform);
  for (const file of [...config.support, ...config.utilities]) {
    await add(resolve(repo, 'examples', file), `src/${file}`);
  }
  for (const file of ['Roboto-Regular.ttf', 'LICENSE_OFL.txt']) {
    await add(resolve(repo, 'examples', file), `assets/${file}`);
  }
  for (const file of [...libraryFiles, `include/nanovg/${config.header}`,
    ...config.shaders]) {
    await add(resolve(repo, file), `extern/nanovg/${file}`);
  }

  await writeZip(resolve(downloads, `${packageName}.zip`), entries);
}

const helloFunction = `
static void drawHello(NVGcontext* vg, float width, float height) {
  NVGpaint background = nvgLinearGradient(vg, 80.0f, 80.0f, width - 80.0f, height - 80.0f,
    nvgRGB(56, 189, 248), nvgRGB(139, 92, 246));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, 80.0f, 80.0f, width - 160.0f, height - 160.0f, 24.0f);
  nvgFillPaint(vg, background);
  nvgFill(vg);
  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 54.0f);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGB(255, 255, 255));
  nvgText(vg, width * 0.5f, height * 0.5f - 14.0f, "Hello, NanoVG!", NULL);
  nvgFontSize(vg, 20.0f);
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 190));
  nvgText(vg, width * 0.5f, height * 0.5f + 42.0f, "Your first vector frame", NULL);
}
`;

function makeHelloStarter(backend) {
  return (input) => {
    let source = input.replaceAll('\r\n', '\n')
      .replace('#include "demo.h"', '').replace('#include "perf.h"', '');
    source = source.replace(/\nstatic int blowup\s*=\s*0;/, '');
    source = source.replace(/\nint blowup = 0;/, '');
    const insertion = backend === 'd3d11' ? '#include <windowsx.h>' :
      backend === 'vulkan' ? '#include "vulkan_util.h"' : '#include "wgpu_util.h"';
    source = source.replace(insertion, insertion + '\n' + helloFunction);

    if (backend === 'd3d11') {
      source = source.replace('struct GPUtimer gpuTimer;\nstruct PerfGraph fps, cpuGraph, gpuGraph;\ndouble prevt = 0, cpuTime = 0;\nstruct DemoData data;\n', '');
      source = source.replace(/    int n;\n    int i;\n    float gpuTimes\[3\];\n    double dt;\n    double t;\n/, '');
      source = source.replace(/    t = getCPUTime\(\);\n    dt = t - prevt;\n\tprevt = t;\n/, '');
      source = source.replace(/    renderDemo\([^;]+;\n\n    renderGraph[\s\S]*?\tnvgEndFrame\(vg\);/, '    drawHello(vg, (float)xWin, (float)yWin);\n\n\tnvgEndFrame(vg);');
      source = source.replace(/\n    \/\/ Measure the CPU time[\s\S]*?\n\tif \(screenshot\)/, '\n\tif (screenshot)');
      source = source.replace(/\n\tif \(loadDemoData[\s\S]*?startGPUTimer\(&gpuTimer\);/, '\n\tif (nvgCreateFont(vg, "sans", "assets/Roboto-Regular.ttf") == -1) return FALSE;');
      source = source.replace(/\n    freeDemoData\(vg, &data\);/, '');
      source = source.replace(/\n            else if \(GetKeyState\(VK_SPACE\)\)[\s\S]*?            }\n            else if \(wParam == 'P'\)/, "\n            else if (wParam == 'P')");
      source = source.replace(/\n\tprintf\("Average Frame Time:[\s\S]*?getGraphAverage\(&gpuGraph\) \* 1000\.0f\);\n/, '\n');
    } else if (backend === 'vulkan') {
      source = source.replace(/  DemoData data;[\s\S]*?  double prevt = glfwGetTime\(\);/, '  if (nvgCreateFont(vg, "sans", "assets/Roboto-Regular.ttf") == -1) return -1;');
      source = source.replace('    double mx, my, t, dt;', '');
      source = source.replace(/      t = glfwGetTime\(\);[\s\S]*?      glfwGetCursorPos\(window, &mx, &my\);\n/, '');
      source = source.replace(/      renderDemo\([^;]+;\n      renderGraph\([^;]+;/, '      drawHello(vg, (float)winWidth, (float)winHeight);');
      source = source.replace('  freeDemoData(vg, &data);\n', '');
      source = source.replace(/\n  printf\("Average Frame Time:[^;]+;[\s\S]*?gpuGraph\) \* 1000\.0f\);/, '');
    } else {
      source = source.replace(/\n\tDemoData data;[\s\S]*?\tdouble prevt = glfwGetTime\(\);/, '\n\tif (nvgCreateFont(vg, "sans", "assets/Roboto-Regular.ttf") == -1) return -1;');
      source = source.replace(/\n\t\tdouble t[\s\S]*?glfwGetCursorPos\(window, &mx, &my\);\n/, '\n');
      source = source.replace(/\t\trenderDemo\([^;]+;\n\t\trenderGraph\([^;]+;/, '\t\tdrawHello(vg, (float)winW, (float)winH);');
      source = source.replace('\n\tfreeDemoData(vg, &data);', '');
      source = source.replace(/\n\tprintf\("Average Frame Time:[^;]+;/, '');
    }
    return source;
  };
}

await packageBackendStarter({
  name: 'd3d11',
  main: 'example_d3d11.c',
  header: 'nanovg_d3d11.h',
  utilities: [],
  support: [],
  transform: makeHelloStarter('d3d11'),
  shaders: ['shaders/d3d11/D3D11PixelShader.h',
    'shaders/d3d11/D3D11PixelShaderAA.h', 'shaders/d3d11/D3D11VertexShader.h'],
});
await packageBackendStarter({
  name: 'vulkan',
  main: 'example_vulkan.c',
  header: 'nanovg_vk.h',
  utilities: ['vulkan_util.h'],
  support: [],
  transform: makeHelloStarter('vulkan'),
  shaders: ['shaders/vulkan/fill.vert.inc', 'shaders/vulkan/fill.frag.inc'],
});
await packageBackendStarter({
  name: 'webgpu',
  main: 'example_wgpu.c',
  header: 'nanovg_wgpu.h',
  utilities: ['wgpu_util.h'],
  support: [],
  transform: makeHelloStarter('webgpu'),
  shaders: ['shaders/wgpu/fill.wgsl.inc'],
});

await build({
  entryPoints: {
    preview: resolve(site, 'preview/preview.js'),
    'toolchain-worker': resolve(browser, 'worker-entry.js'),
  },
  outdir: output,
  bundle: true,
  format: 'esm',
  platform: 'browser',
  target: 'es2022',
  minify: true,
  alias: {
    '@preview/worker-client': resolve(browser, 'worker-client.js'),
    '@preview/facade': resolve(browser, 'createEmception.js'),
  },
  loader: { '.py': 'text' },
});
await cp(resolve(site, 'node_modules/emception/cdn'), resolve(output, 'compiler'), { recursive: true });

for (const file of ['index.html', 'runner.js']) {
  await cp(resolve(site, 'preview', file), resolve(output, file));
}
await cp(resolve(site, 'preview/isolation.js'), resolve(site, 'public/preview-isolation.js'));
await mkdir(resolve(output, 'assets'), { recursive: true });
await cp(resolve(repo, 'examples/Roboto-Regular.ttf'), resolve(output, 'assets/Roboto-Regular.ttf'));
await cp(resolve(repo, 'examples/images/image1.jpg'), resolve(output, 'assets/image1.jpg'));

const sources = {
  'nanovg.c': await readFile(resolve(repo, 'src/nanovg.c'), 'utf8'),
  'host.cpp': await readFile(resolve(site, 'preview/host.cpp'), 'utf8'),
};
for (const file of ['nanovg.h', 'nanovg_gl.h']) {
  sources['include/' + file] = await readFile(resolve(repo, 'include/nanovg', file), 'utf8');
}
for (const file of ['fontstash.h', 'stb_image.h', 'stb_truetype.h']) {
  sources['third_party/' + file] = await readFile(resolve(repo, 'third_party', file), 'utf8');
}
await writeFile(resolve(output, 'sources.json'), JSON.stringify(sources));
console.log('Prepared browser C++ compiler and NanoVG preview sources.');

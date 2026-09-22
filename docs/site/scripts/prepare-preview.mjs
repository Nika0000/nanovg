import { build } from 'esbuild';
import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const site = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repo = resolve(site, '../..');
const output = resolve(site, 'public/preview');
const browser = resolve(site, 'node_modules/@gameguild/emception-browser/dist');
await mkdir(output, { recursive: true });

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

import { readdir, readFile } from 'node:fs/promises';
import { relative, resolve, sep } from 'node:path';
import process from 'node:process';

const contentRoot = resolve('content/docs');

async function collect(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const files = [];

  for (const entry of entries) {
    const path = resolve(directory, entry.name);
    if (entry.isDirectory()) files.push(...await collect(path));
    else if (entry.name.endsWith('.mdx')) files.push(path);
  }

  return files;
}

function routeFor(file) {
  const parts = relative(contentRoot, file).split(sep);
  const name = parts.pop().replace(/\.mdx$/, '');
  if (name !== 'index') parts.push(name);
  return `/${parts.join('/')}`.replace(/\/$/, '') || '/';
}

const files = await collect(contentRoot);
const routes = new Set(files.map(routeFor));
const failures = [];
const markdownLink = /(?<!!)\[[^\]]*\]\(([^)]+)\)/g;

for (const file of files) {
  const source = await readFile(file, 'utf8');
  for (const match of source.matchAll(markdownLink)) {
    const destination = match[1].trim().replace(/^<|>$/g, '');
    if (!destination.startsWith('/') || destination.startsWith('//')) continue;

    const route = destination.split(/[?#]/, 1)[0].replace(/\/$/, '') || '/';
    if (!routes.has(route)) {
      failures.push(`${relative(contentRoot, file)}: ${destination}`);
    }
  }
}

if (failures.length > 0) {
  console.error('Broken internal documentation links:');
  for (const failure of failures) console.error(`- ${failure}`);
  process.exitCode = 1;
} else {
  console.log(`Checked internal links across ${files.length} documentation pages.`);
}

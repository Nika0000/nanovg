import { Download } from 'lucide-react';

const starters = [
  {
    backend: 'OpenGL 3',
    requirement: 'CMake, C compiler, OpenGL 3.2-capable GPU and driver',
    file: 'nanovg-opengl-starter.zip',
  },
  {
    backend: 'Direct3D 11',
    requirement: 'Windows, CMake, C compiler, Windows SDK, Direct3D 11-capable GPU',
    file: 'nanovg-d3d11-starter.zip',
  },
  {
    backend: 'Vulkan',
    requirement: 'CMake, C compiler, Vulkan SDK, Vulkan 1.3-capable GPU and driver',
    file: 'nanovg-vulkan-starter.zip',
  },
  {
    backend: 'WebGPU',
    requirement: 'CMake, C compiler, Dawn or wgpu-native CMake target, compatible GPU',
    file: 'nanovg-webgpu-starter.zip',
  },
] as const;

function downloadUrl(file: string) {
  const basePath = process.env.NEXT_PUBLIC_DOCS_BASE_PATH ?? '';
  return `${basePath}/downloads/${file}`;
}

export function StarterDownloads() {
  return (
    <div className="not-prose my-6 overflow-hidden rounded-xl border bg-fd-card text-fd-card-foreground shadow-sm">
      <div className="divide-y">
        {starters.map(({ backend, requirement, file }) => (
        <article key={file} className="flex items-center justify-between gap-4 px-5 py-3 transition-colors">
          <div className="min-w-0">
            <h3 className="m-0 text-base font-semibold tracking-tight">{backend}</h3>
            <p className="mt-1 text-xs leading-5 text-fd-muted-foreground">
              <span className="font-medium text-fd-foreground/80">Requires</span>
              <span aria-hidden="true"> · </span>
              {requirement}
            </p>
          </div>
          <div className="shrink-0">
            <a
              href={downloadUrl(file)}
              download={file}
              className="inline-flex items-center gap-1.5 rounded-md border bg-fd-background px-2.5 py-1.5 text-xs font-medium text-fd-foreground transition-colors hover:bg-fd-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-fd-ring"
              aria-label={`Download the ${backend} NanoVG starter`}
              title={`Download ${backend} starter`}
            >
              <Download aria-hidden="true" className="size-3.5" />
              Download
            </a>
          </div>
        </article>
      ))}
      </div>
    </div>
  );
}

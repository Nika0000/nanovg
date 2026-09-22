import Link from 'next/link';
import {
  Blend,
  Cpu,
  Frame,
  Image,
  Layers3,
  Move3d,
  Palette,
  PenTool,
  Sparkles,
  Type,
} from 'lucide-react';

const sections = [
  ['Context and frames', 'Frame lifecycle and compositing', '/reference/context', Frame],
  ['State stack', 'Save, restore, clipping, and global alpha', '/reference/state', Layers3],
  ['Paths', 'Path construction, fills, and strokes', '/reference/paths', PenTool],
  ['Paints and colors', 'Colors, gradients, patterns, and stroke style', '/reference/paints', Palette],
  ['Transforms', 'Current transform and standalone matrix helpers', '/reference/transforms', Move3d],
  ['Images', 'Image creation, update, query, and deletion', '/reference/images', Image],
  ['Text and fonts', 'Font loading, text drawing, and measurement', '/reference/text', Type],
  ['Image filters', 'CPU filtering and filtered image creation', '/reference/image-filters', Sparkles],
  ['Framebuffer utilities', 'OpenGL and Metal offscreen targets', '/reference/framebuffer-utilities', Blend],
  ['Renderer creation', 'Backend constructors and destructors', '/reference/renderer-creation', Cpu],
] as const;

export function ApiReferenceGrid() {
  return (
    <nav className="not-prose my-6 grid gap-3 sm:grid-cols-2" aria-label="API reference sections">
      {sections.map(([title, description, href, Icon]) => (
        <Link
          key={href}
          href={href}
          className="group rounded-lg border bg-fd-card px-4 py-3.5 text-fd-card-foreground transition-colors hover:bg-fd-accent"
        >
          <span className="flex items-start gap-3">
            <Icon aria-hidden="true" className="mt-0.5 size-5 shrink-0 text-fd-muted-foreground group-hover:text-fd-primary" />
            <span>
              <span className="block font-medium group-hover:text-fd-primary">{title}</span>
              <span className="mt-1 block text-sm leading-5 text-fd-muted-foreground">{description}</span>
            </span>
          </span>
        </Link>
      ))}
    </nav>
  );
}

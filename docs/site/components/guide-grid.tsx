import Link from 'next/link';
import { Frame, Images, Paintbrush, Shapes, Sparkles, Type } from 'lucide-react';

const guides = [
  ['Drawing shapes', 'Draw primitives and construct custom paths from lines and curves.', '/guides/shapes', Shapes],
  ['Strokes and fills', 'Style paths with colors, gradients, image paints, caps, and joins.', '/guides/strokes-and-fills', Paintbrush],
  ['Text and fonts', 'Load fonts, align text, wrap paragraphs, and measure layouts.', '/guides/text-and-fonts', Type],
  ['Images and patterns', 'Load, draw, update, and release encoded or RGBA images.', '/guides/images', Images],
  ['Image filters', 'Process RGBA8 pixels with filters and upload the results.', '/guides/image-filters', Sparkles],
  ['Framebuffers', 'Render into offscreen OpenGL or Metal targets and reuse the result.', '/guides/framebuffers', Frame],
] as const;

export function GuideGrid() {
  return (
    <nav className="not-prose my-6 grid gap-3 sm:grid-cols-2" aria-label="NanoVG guides">
      {guides.map(([title, description, href, Icon]) => (
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

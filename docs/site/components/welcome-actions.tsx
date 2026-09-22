import Link from 'next/link';
import { Boxes, Frame, PackagePlus, Shapes } from 'lucide-react';

const actions = [
  {
    title: 'Install NanoVG',
    description: 'Add NanoVG to an existing CMake project.',
    href: '/getting-started/installation',
    icon: PackagePlus,
  },
  {
    title: 'Draw your first frame',
    description: 'Learn the frame lifecycle and render your first shape.',
    href: '/getting-started/first-frame',
    icon: Frame,
  },
  {
    title: 'Choose a renderer',
    description: 'Find the renderer for your graphics API and platform.',
    href: '/getting-started/choosing-a-renderer',
    icon: Shapes,
  },
  {
    title: 'Build the examples',
    description: 'Verify your development environment with the included examples.',
    href: '/getting-started/examples',
    icon: Boxes,
  },
] as const;

export function WelcomeActions() {
  return (
    <div className="not-prose my-6 grid gap-4 sm:grid-cols-2">
      {actions.map(({ title, description, href, icon: Icon }) => (
        <Link
          key={href}
          href={href}
          className="group flex min-h-36 flex-col rounded-xl border bg-fd-card p-5 text-fd-card-foreground shadow-sm transition-colors hover:bg-fd-accent"
        >
          <Icon aria-hidden="true" className="mb-4 size-6 text-fd-muted-foreground transition-colors group-hover:text-fd-primary" />
          <span className="font-semibold">{title}</span>
          <span className="mt-1 text-sm leading-6 text-fd-muted-foreground">{description}</span>
        </Link>
      ))}
    </div>
  );
}

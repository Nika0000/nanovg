import type { Metadata } from 'next';
import type { ReactNode } from 'react';
import { RootProvider } from 'fumadocs-ui/provider/next';
import { DocsLayout } from 'fumadocs-ui/layouts/docs';
import { source } from '@/lib/source';
import './global.css';

export const metadata: Metadata = {
  title: { default: 'NanoVG', template: '%s | NanoVG' },
  description: 'Documentation for NanoVG.',
};

export default function Layout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body className="flex min-h-screen flex-col">
        <RootProvider search={{ options: { type: 'static' } }}>
          <DocsLayout
            tree={source.pageTree}
            nav={{ title: 'NanoVG', url: '/' }}
            searchToggle={{ enabled: true }}
          >
            {children}
          </DocsLayout>
        </RootProvider>
      </body>
    </html>
  );
}

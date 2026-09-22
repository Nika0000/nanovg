import { notFound } from 'next/navigation';
import {
  DocsBody,
  DocsDescription,
  DocsPage,
  DocsTitle,
} from 'fumadocs-ui/page';
import { MarkdownCopyButton, ViewOptionsPopover } from 'fumadocs-ui/layouts/docs/page';
import { Step, Steps } from 'fumadocs-ui/components/steps';
import defaultMdxComponents from 'fumadocs-ui/mdx';
import { ApiReferenceGrid } from '@/components/api-reference-grid';
import { GuideGrid } from '@/components/guide-grid';
import { LiveExample } from '@/components/live-example';
import { WelcomeActions } from '@/components/welcome-actions';
import { getPageMarkdownUrl } from '@/lib/shared';
import { source } from '@/lib/source';

type Props = { params: Promise<{ slug?: string[] }> };

export const dynamicParams = false;

export function generateStaticParams() {
  return source.generateParams();
}

export async function generateMetadata({ params }: Props) {
  const { slug } = await params;
  const page = source.getPage(slug);
  if (!page) notFound();
  return { title: page.data.title, description: page.data.description };
}

export default async function Page({ params }: Props) {
  const { slug } = await params;
  const page = source.getPage(slug);
  if (!page) notFound();
  const MDX = page.data.body;
  const markdownUrl = getPageMarkdownUrl(page);
  const githubUrl = `https://github.com/Nika0000/nanovg/blob/main/docs/site/content/docs/${page.path}`;

  return (
    <DocsPage toc={page.data.toc} tableOfContent={{ enabled: false }}>
      <DocsTitle>{page.data.title}</DocsTitle>
      <DocsDescription>{page.data.description}</DocsDescription>
      <div className="flex flex-row items-center gap-2 border-b pb-6 pt-2">
        <MarkdownCopyButton markdownUrl={markdownUrl} />
        <ViewOptionsPopover markdownUrl={markdownUrl} githubUrl={githubUrl} />
      </div>
      <DocsBody>
        <MDX components={{ ...defaultMdxComponents, ApiReferenceGrid, GuideGrid, LiveExample, Step, Steps, WelcomeActions }} />
      </DocsBody>
    </DocsPage>
  );
}

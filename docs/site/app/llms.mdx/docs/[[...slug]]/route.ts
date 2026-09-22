import { notFound } from 'next/navigation';
import { docsLlms, source } from '@/lib/source';

type Props = { params: Promise<{ slug?: string[] }> };

export const revalidate = false;

export async function GET(_request: Request, { params }: Props) {
  const { slug } = await params;
  const slugs = slug?.slice(0, -1) ?? [];
  if (slugs.at(-1) === 'index') slugs.pop();

  const page = source.getPage(slugs);
  if (!page) notFound();

  const basePath = process.env.DOCS_BASE_PATH ?? '';
  const content = (await docsLlms.page(page)).replaceAll('](/', `](${basePath}/`);

  return new Response(content, {
    headers: {
      'Content-Type': 'text/markdown; charset=utf-8',
    },
  });
}

export function generateStaticParams() {
  return source.generateParams().map(({ slug }) => ({
    slug: [...(slug ?? []), 'content.md'],
  }));
}

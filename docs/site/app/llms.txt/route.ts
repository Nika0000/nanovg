import { docsLlms } from '@/lib/source';

export const revalidate = false;

export async function GET() {
  const basePath = process.env.DOCS_BASE_PATH ?? '';
  const content = (await docsLlms.index()).replaceAll('](/', `](${basePath}/`);

  return new Response(content, {
    headers: {
      'Content-Type': 'text/plain; charset=utf-8',
    },
  });
}

import { createGetUrl } from 'fumadocs-core/source';

export const docsContentRoute = '/llms.mdx/docs';

const getContentUrl = createGetUrl(docsContentRoute);

export function getPageMarkdownUrl(page: { slugs: string[]; locale?: string }) {
  return getContentUrl([...page.slugs, 'content.md'], page.locale);
}

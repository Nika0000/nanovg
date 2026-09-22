import { createMDX } from 'fumadocs-mdx/next';

const withMDX = createMDX();

export default withMDX({
  output: 'export',
  env: { NEXT_PUBLIC_DOCS_BASE_PATH: process.env.DOCS_BASE_PATH ?? '' },
  basePath: process.env.DOCS_BASE_PATH ?? '',
  trailingSlash: true,
  images: { unoptimized: true },
});

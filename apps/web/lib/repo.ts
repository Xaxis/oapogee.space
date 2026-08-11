import { join } from 'node:path'

// content/ and data/ are the product. The site is one renderer of them, and it
// lives in a workspace two levels down, so every read resolves from here rather
// than from process.cwd(), which differs between `next dev`, `next build`, and
// the Vercel build container.
export const REPO_ROOT = join(process.cwd(), '..', '..')

export const CONTENT_DIR = join(REPO_ROOT, 'content')
export const DATA_DIR = join(REPO_ROOT, 'data')
export const DOCS_DIR = join(REPO_ROOT, 'docs')

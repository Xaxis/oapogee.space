import { readFileSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import matter from 'gray-matter'
import { unified } from 'unified'
import remarkParse from 'remark-parse'
import remarkGfm from 'remark-gfm'
import remarkRehype from 'remark-rehype'
import rehypeSlug from 'rehype-slug'
import rehypeStringify from 'rehype-stringify'
import { CONTENT_DIR, DOCS_DIR, REPO_ROOT } from './repo'
import { getGlossary, type Term } from './data'
import { linkGlossaryTerms } from './glossary-links'

export type PageStatus = 'draft' | 'needs-review' | 'verified'

export type Frontmatter = {
  title: string
  description: string
  tier: string
  difficulty: string
  time_estimate: string | null
  updated: string
  status: PageStatus
}

export type Page = {
  frontmatter: Frontmatter
  html: string
  markers: MarkerCount
}

export type MarkerCount = {
  verify: number
  confirmOnHardware: number
  confirm: number
  photo: number
  total: number
}

// YAML 1.1, which gray-matter parses frontmatter with, turns an unquoted
// `updated: 2026-08-11` into a Date. Everything downstream wants the ISO string
// it was written as, and a Date reaching JSX is a render error rather than a
// visible bug, so it is normalised here at the boundary.
export function isoDate(value: unknown): string {
  if (value instanceof Date) return value.toISOString().slice(0, 10)
  return String(value ?? '')
}

export function countMarkers(markdown: string): MarkerCount {
  const count = (kind: string) =>
    (markdown.match(new RegExp(`TODO\\(${kind}\\)`, 'g')) ?? []).length
  const verify = count('verify')
  const confirmOnHardware = count('confirm-on-hardware')
  // TODO(confirm) and TODO(confirm-on-hardware) share a prefix, so the plain
  // form has to be counted by excluding the longer one rather than by substring.
  const confirm = (markdown.match(/TODO\(confirm\)/g) ?? []).length
  const photo = count('photo')
  return {
    verify,
    confirmOnHardware,
    confirm,
    photo,
    total: verify + confirmOnHardware + confirm + photo,
  }
}

// Verification markers are written inline in the prose because that is where
// they are useful to whoever is editing. On the rendered page they become a
// visible callout: a reader deserves to see exactly where a number is missing,
// rather than reading around a gap they cannot detect.
const MARKER_LABELS: Record<string, string> = {
  verify: 'Unverified',
  'confirm-on-hardware': 'Needs hardware',
  confirm: 'Needs a decision',
  photo: 'Photo needed',
}

function renderMarkers(html: string): string {
  // A marker runs from its introducer to the end of the block that contains it,
  // which after rendering means the closing tag of the paragraph, list item, or
  // cell. Matching the closing tag rather than a sentence boundary keeps the
  // callout inside valid markup regardless of how long the note is.
  return html.replace(
    /TODO\((verify|confirm-on-hardware|confirm|photo)\):?\s*([\s\S]*?)(?=<\/p>|<\/li>|<\/td>)/g,
    (_match, kind: string, body: string) =>
      `<span class="marker marker-${kind}" role="note">` +
      `<span class="marker-label">${MARKER_LABELS[kind]}</span>` +
      `<span class="marker-body">${body.trim()}</span></span>`
  )
}

// The glossary page itself is excluded, and so is anything that would link a
// term to its own definition from inside that definition.
function rehypeGlossary(terms: Term[]) {
  return (tree: unknown) => {
    linkGlossaryTerms(tree as Parameters<typeof linkGlossaryTerms>[0], terms)
  }
}

async function toHtml(markdown: string, terms?: Term[]): Promise<string> {
  const processor = unified().use(remarkParse).use(remarkGfm).use(remarkRehype).use(rehypeSlug)

  if (terms) processor.use(() => rehypeGlossary(terms))

  const file = await processor.use(rehypeStringify).process(markdown)
  return String(file)
}

export async function getPage(slug: string): Promise<Page | null> {
  const path = join(CONTENT_DIR, `${slug}.md`)
  if (!existsSync(path)) return null

  const raw = readFileSync(path, 'utf8')
  const { data, content } = matter(raw)
  const markers = countMarkers(content)

  // Strip the leading H1: the layout renders the title from frontmatter, and
  // two of them is a duplicate heading in the outline.
  // gray-matter leaves a newline where the frontmatter block was, so an anchor
  // at the start of the string does not reach the H1. The layout renders the
  // title from frontmatter, and leaving this in produces a duplicate heading in
  // the document outline.
  const body = content.replace(/^\s*#\s+.+\n/, '')

  return {
    frontmatter: { ...(data as Frontmatter), updated: isoDate(data.updated) },
    html: renderMarkers(await toHtml(body, getGlossary().terms)),
    markers,
  }
}

// CHANGELOG.md lives at the repository root because that is where a contributor
// looks for it. It renders here too because errata for a flight computer are
// safety information: somebody flying an older firmware needs to be able to
// find out that a detection threshold changed, without cloning anything.
export async function getRepoDoc(name: string): Promise<string | null> {
  const path = join(REPO_ROOT, `${name}.md`)
  if (!existsSync(path)) return null
  const markdown = readFileSync(path, 'utf8').replace(/^\s*#\s+.+\n/, '')
  return renderMarkers(await toHtml(markdown, getGlossary().terms))
}

// The two wire format specifications live in docs/spec/ because they are
// repository deliverables that firmware implementers read from a checkout. They
// also need stable citable URLs, since third parties are invited to implement
// against them, so they render here from the same file rather than from a copy.
export async function getSpec(name: string): Promise<Page | null> {
  const path = join(DOCS_DIR, 'spec', `${name}.md`)
  if (!existsSync(path)) return null

  const { data, content } = matter(readFileSync(path, 'utf8'))
  const markers = countMarkers(content)
  // gray-matter leaves a newline where the frontmatter block was, so an anchor
  // at the start of the string does not reach the H1. The layout renders the
  // title from frontmatter, and leaving this in produces a duplicate heading in
  // the document outline.
  const body = content.replace(/^\s*#\s+.+\n/, '')

  return {
    frontmatter: { ...(data as Frontmatter), updated: isoDate(data.updated) },
    html: renderMarkers(await toHtml(body, getGlossary().terms)),
    markers,
  }
}

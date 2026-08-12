import { readFileSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import matter from 'gray-matter'
import { CONTENT_DIR, DOCS_DIR } from './repo'
import { CONTENT_ROUTES } from './routes'
import { getGlossary, getTroubleshooting, getBom } from './data'

/**
 * The search index, built when the site builds.
 *
 * Indexed by hand rather than by crawling the rendered prose, because what
 * people search a site like this for is not sentences. It is a symptom they
 * just saw, a part number they are holding, a term they do not know, or the
 * name of a page. Those four things are small, high signal, and each knows
 * exactly which anchor to send somebody to.
 *
 * A full-text index over every paragraph would be an order of magnitude larger
 * and would bury "my altitude reads negative" under the twelve paragraphs that
 * happen to contain the word altitude.
 */

export type SearchDoc = {
  /** Shown as the result title. */
  title: string
  /** Shown under it, for disambiguation. */
  context: string
  href: string
  kind: 'page' | 'section' | 'symptom' | 'term' | 'part'
  /** Extra text matched against but not displayed, such as aliases. */
  keywords?: string
}

const slugify = (s: string) =>
  s
    .toLowerCase()
    .trim()
    .replace(/[^\p{L}\p{N}\s-]/gu, '')
    .replace(/\s+/g, '-')

function headings(markdown: string): { text: string; id: string }[] {
  const out: { text: string; id: string }[] = []
  const seen = new Map<string, number>()
  let inFence = false

  for (const line of markdown.split('\n')) {
    if (/^\s*```/.test(line)) {
      inFence = !inFence
      continue
    }
    if (inFence) continue
    const m = /^(#{2,3})\s+(.+?)\s*$/.exec(line)
    if (!m) continue
    const text = m[2].replace(/`([^`]*)`/g, '$1').replace(/\*\*?([^*]*)\*\*?/g, '$1')
    const base = slugify(text)
    const n = seen.get(base) ?? 0
    seen.set(base, n + 1)
    out.push({ text, id: n === 0 ? base : `${base}-${n}` })
  }
  return out
}

export function buildSearchIndex(): SearchDoc[] {
  const docs: SearchDoc[] = []

  const addMarkdown = (path: string, href: string, fallbackTitle: string) => {
    if (!existsSync(path)) return
    const { data, content } = matter(readFileSync(path, 'utf8'))
    const title = (data.title as string) ?? fallbackTitle
    docs.push({
      title,
      context: (data.description as string) ?? '',
      href,
      kind: 'page',
    })
    for (const h of headings(content)) {
      docs.push({ title: h.text, context: title, href: `${href}#${h.id}`, kind: 'section' })
    }
  }

  for (const route of CONTENT_ROUTES) {
    addMarkdown(join(CONTENT_DIR, `${route.file}.md`), `/${route.slug}`, route.nav)
  }
  addMarkdown(
    join(DOCS_DIR, 'spec/telemetry-packet.md'),
    '/reference/telemetry-packet',
    'Telemetry packet specification'
  )
  addMarkdown(
    join(DOCS_DIR, 'spec/log-format.md'),
    '/reference/log-format',
    'Onboard log format specification'
  )

  // Symptoms are the highest-value thing in here. Somebody with a problem knows
  // what they saw, and this is the only route from those words to the answer.
  const trouble = getTroubleshooting()
  const categories = new Map(trouble.categories.map((c) => [c.id, c.title]))
  for (const entry of trouble.entries) {
    docs.push({
      title: entry.symptom,
      context: categories.get(entry.category) ?? 'Troubleshooting',
      href: `/troubleshooting#${entry.id}`,
      kind: 'symptom',
      keywords: entry.checks.map((c) => c.do).join(' '),
    })
  }

  for (const term of getGlossary().terms) {
    docs.push({
      title: term.term,
      context: term.short,
      href: `/glossary#${term.id}`,
      kind: 'term',
      keywords: (term.aliases ?? []).join(' '),
    })
  }

  // Part numbers, because somebody holding a chip and wondering whether it is
  // the right one is a real search.
  for (const part of getBom().parts) {
    docs.push({
      title: part.name,
      context: [part.manufacturer, part.mpn].filter(Boolean).join(' ') || 'Bill of materials',
      href: '/bom',
      kind: 'part',
      keywords: [part.mpn, ...(part.substitutes ?? []).map((s) => s.mpn)].filter(Boolean).join(' '),
    })
  }

  return docs
}

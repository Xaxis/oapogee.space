import type { Term } from './data'

// Links the first mention of each glossary term on a page to its definition.
//
// The style rule is that jargon gets a one-clause gloss on first use and the
// full definition lives in the glossary. That only works if the link exists,
// and doing it by hand across twenty pages guarantees it is done inconsistently
// and then rots as pages are edited. So it happens at render time from
// data/glossary.yaml, which is already the single source for those definitions.
//
// This operates on the hast tree rather than on the rendered HTML string.
// Regex over HTML would eventually match inside an attribute, an existing link,
// or a code sample, and produce broken markup that nothing catches.

type HastNode = {
  type: string
  tagName?: string
  value?: string
  properties?: Record<string, unknown>
  children?: HastNode[]
}

// Never link inside these. A term inside a heading is a section title, inside
// code it is an identifier, and inside a link it would nest an anchor, which is
// invalid HTML.
const SKIP_TAGS = new Set(['a', 'code', 'pre', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6'])

type Matcher = { id: string; pattern: RegExp }

export function buildMatchers(terms: Term[]): Matcher[] {
  const entries: { id: string; text: string }[] = []
  for (const term of terms) {
    entries.push({ id: term.id, text: term.term })
    for (const alias of term.aliases ?? []) entries.push({ id: term.id, text: alias })
  }

  // Longest first, so "centre of pressure" wins over "pressure" when both could
  // match at the same position.
  entries.sort((a, b) => b.text.length - a.text.length)

  return entries.map(({ id, text }) => ({
    id,
    // Word boundaries so "IMU" does not match inside "IMUs" oddly, and so
    // "apogee" does not match inside "oApogee".
    pattern: new RegExp(`\\b${text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`, 'i'),
  }))
}

export function linkGlossaryTerms(tree: HastNode, terms: Term[]): void {
  const matchers = buildMatchers(terms)
  const linked = new Set<string>()

  function walk(node: HastNode, insideSkip: boolean): void {
    if (!node.children) return

    const out: HastNode[] = []
    for (const child of node.children) {
      const skip = insideSkip || (child.tagName ? SKIP_TAGS.has(child.tagName) : false)

      if (child.type === 'element') {
        walk(child, skip)
        out.push(child)
        continue
      }

      if (child.type !== 'text' || skip || typeof child.value !== 'string') {
        out.push(child)
        continue
      }

      out.push(...splitText(child.value))
    }
    node.children = out
  }

  function splitText(value: string): HastNode[] {
    for (const { id, pattern } of matchers) {
      if (linked.has(id)) continue
      const match = pattern.exec(value)
      if (!match) continue

      linked.add(id)
      const before = value.slice(0, match.index)
      const matched = match[0]
      const after = value.slice(match.index + matched.length)

      const link: HastNode = {
        type: 'element',
        tagName: 'a',
        properties: {
          href: `/glossary#${id}`,
          className: ['glossary-link'],
          title: 'Definition in the glossary',
        },
        children: [{ type: 'text', value: matched }],
      }

      // Only the text after the match is rescanned. The text before it was
      // already scanned against every matcher by this point in the loop, and
      // rescanning it would let a shorter term match ahead of a longer one.
      return [
        ...(before ? [{ type: 'text', value: before }] : []),
        link,
        ...(after ? splitText(after) : []),
      ]
    }
    return [{ type: 'text', value }]
  }

  walk(tree, false)
}

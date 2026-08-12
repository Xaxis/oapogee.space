import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs'
import { join, extname, relative, sep } from 'node:path'
import matter from 'gray-matter'
import { countMarkers, isoDate, type Frontmatter, type MarkerCount } from './content'
import { CONTENT_DIR, DATA_DIR, DOCS_DIR, REPO_ROOT } from './repo'
import { CONTENT_ROUTES, GENERATED_ROUTES } from './routes'

// Every page the site claims to have, with where its content comes from. Built
// from the same route table the navigation uses, so a page cannot be reachable
// and missing from this list, or listed here and unreachable.

type Planned = {
  route: string
  title: string
  source: { kind: 'content'; file: string } | { kind: 'spec'; file: string } | { kind: 'generated'; from: string }
}

const SPEC_PAGES: Planned[] = [
  {
    route: '/reference/telemetry-packet',
    title: 'Telemetry packet spec',
    source: { kind: 'spec', file: 'telemetry-packet' },
  },
  {
    route: '/reference/log-format',
    title: 'Log format spec',
    source: { kind: 'spec', file: 'log-format' },
  },
]

const GENERATED_SOURCES: Record<string, string> = {
  '/': 'data/tiers.yaml, data/flight-phases.yaml',
  '/bom': 'data/bom.yaml, data/system.yaml',
  '/preflight': 'data/preflight.yaml',
  '/troubleshooting': 'data/troubleshooting.yaml',
  '/flights': 'data/flights.yaml',
  '/reference/schematic': 'hardware/oapogee.tsx',
  '/glossary': 'data/glossary.yaml',
  '/changelog': 'CHANGELOG.md',
  '/status': 'every page in this table',
}

export const PLANNED: Planned[] = [
  { route: '/', title: 'Home', source: { kind: 'generated', from: GENERATED_SOURCES['/'] } },
  ...CONTENT_ROUTES.map((r) => ({
    route: `/${r.slug}`,
    title: r.nav,
    source: { kind: 'content' as const, file: r.file },
  })),
  ...GENERATED_ROUTES.filter((r) => r.href in GENERATED_SOURCES).map((r) => ({
    route: r.href,
    title: r.nav,
    source: { kind: 'generated' as const, from: GENERATED_SOURCES[r.href] },
  })),
  ...SPEC_PAGES,
]

export type PageStatusRow = {
  route: string
  title: string
  status: string
  source: string
  updated: string | null
  markers: MarkerCount | null
}

function readFrontmatter(path: string) {
  if (!existsSync(path)) return null
  const { data, content } = matter(readFileSync(path, 'utf8'))
  return { fm: data as Frontmatter, markers: countMarkers(content) }
}

export function pageStatuses(): PageStatusRow[] {
  return PLANNED.map((p) => {
    if (p.source.kind === 'generated') {
      return {
        route: p.route,
        title: p.title,
        status: 'generated',
        source: p.source.from,
        updated: null,
        markers: null,
      }
    }

    const path =
      p.source.kind === 'spec'
        ? join(DOCS_DIR, 'spec', `${p.source.file}.md`)
        : join(CONTENT_DIR, `${p.source.file}.md`)

    const read = readFrontmatter(path)
    if (!read) {
      return {
        route: p.route,
        title: p.title,
        status: 'not written',
        source: path.slice(path.lastIndexOf('/') + 1),
        updated: null,
        markers: null,
      }
    }

    return {
      route: p.route,
      title: p.title,
      status: read.fm.status,
      source:
        p.source.kind === 'spec'
          ? `docs/spec/${p.source.file}.md`
          : `content/${p.source.file}.md`,
      updated: isoDate(read.fm.updated),
      markers: read.markers,
    }
  }).sort((a, b) => a.route.localeCompare(b.route))
}

function walk(dir: string, acc: string[] = []): string[] {
  if (!existsSync(dir)) return acc
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry)
    if (statSync(full).isDirectory()) walk(full, acc)
    else if (['.md', '.yaml', '.yml'].includes(extname(full))) acc.push(full)
  }
  return acc
}

const MARKER_SOURCES = () => [
  ...walk(CONTENT_DIR),
  ...walk(DATA_DIR),
  ...walk(join(DOCS_DIR, 'spec')),
]

export function repoMarkerTotals(): MarkerCount {
  const totals: MarkerCount = { verify: 0, confirmOnHardware: 0, confirm: 0, photo: 0, total: 0 }
  for (const file of MARKER_SOURCES()) {
    const m = countMarkers(readFileSync(file, 'utf8'))
    totals.verify += m.verify
    totals.confirmOnHardware += m.confirmOnHardware
    totals.confirm += m.confirm
    totals.photo += m.photo
    totals.total += m.total
  }
  return totals
}

export type Marker = { kind: string; file: string; line: number; text: string }

const KINDS = ['verify', 'confirm-on-hardware', 'confirm', 'photo'] as const

/**
 * Every unverified claim in the repository, read from the sources at build time.
 *
 * This used to be a generated Markdown file at the repository root, checked in
 * and validated by CI so it could not go stale. Reading the sources directly is
 * strictly better: it cannot go stale by construction, it needs no check, and it
 * removes a generated artifact from the root of a repository that people are
 * meant to be able to read.
 *
 * A marker's text runs from its introducer to the end of its block. In YAML that
 * is a folded scalar continuing until dedent or the next key; in Markdown it is
 * the rest of the paragraph. One heuristic covers both.
 */
export function repoMarkers(): Marker[] {
  const out: Marker[] = []

  for (const path of MARKER_SOURCES()) {
    // Relative to the known repository root rather than by searching the
    // absolute path for a directory name. A CI checkout sits at
    // /home/runner/work/oapogee.space/oapogee.space, where searching finds the
    // first of the two and produces a path with the repository name doubled
    // into it, and a clone under any other name produced nothing usable at all.
    // These are interpolated into a GitHub blob URL, so separators are
    // normalised for the Windows case.
    const file = relative(REPO_ROOT, path).split(sep).join('/')
    const lines = readFileSync(path, 'utf8').split('\n')

    for (let i = 0; i < lines.length; i++) {
      const kind = KINDS.find((k) => lines[i].includes(`TODO(${k})`))
      if (!kind) continue

      // Captured before the inner loop advances i past the continuation lines,
      // which would otherwise report the end of the block and send a reader
      // following the link to a line below the marker.
      const startLine = i + 1

      const marker = `TODO(${kind})`
      const parts = [lines[i].slice(lines[i].indexOf(marker) + marker.length)]

      for (let j = i + 1; j < lines.length; j++) {
        const next = lines[j]
        if (!next.trim()) break
        if (/^\s*#/.test(next)) break
        if (/^\s*-?\s*[A-Za-z_][\w-]*:(\s|$)/.test(next)) break
        if (/^\s*[-*]\s/.test(next)) break
        if (/^#{1,6}\s/.test(next)) break
        if (KINDS.some((k) => next.includes(`TODO(${k})`))) break
        parts.push(next)
        i = j
      }

      out.push({
        kind,
        file,
        line: startLine,
        text: parts.join(' ').replace(/^[:\s>]+/, '').replace(/\s+/g, ' ').replace(/["']$/, '').trim(),
      })
    }
  }
  return out
}

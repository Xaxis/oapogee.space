import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs'
import { join, extname } from 'node:path'
import matter from 'gray-matter'
import { countMarkers, isoDate, type Frontmatter, type MarkerCount } from './content'
import { CONTENT_DIR, DATA_DIR, DOCS_DIR } from './repo'
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
  '/glossary': 'data/glossary.yaml',
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

export function repoMarkerTotals(): MarkerCount {
  const totals: MarkerCount = { verify: 0, confirmOnHardware: 0, confirm: 0, photo: 0, total: 0 }
  for (const file of [...walk(CONTENT_DIR), ...walk(DATA_DIR), ...walk(join(DOCS_DIR, 'spec'))]) {
    const m = countMarkers(readFileSync(file, 'utf8'))
    totals.verify += m.verify
    totals.confirmOnHardware += m.confirmOnHardware
    totals.confirm += m.confirm
    totals.photo += m.photo
    totals.total += m.total
  }
  return totals
}

import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs'
import { join, extname } from 'node:path'
import matter from 'gray-matter'
import { countMarkers, isoDate, type Frontmatter, type MarkerCount } from './content'
import { CONTENT_DIR, DATA_DIR } from './repo'

// The page map in docs/page-map.md is the plan. This is the machine-readable
// half of it, so /status can show what exists against what was promised rather
// than only listing what happens to have been written.
export const PLANNED: { route: string; title: string; slug?: string; note?: string }[] = [
  { route: '/', title: 'Home' },
  { route: '/start', title: 'Start here', slug: 'start-here' },
  { route: '/bom', title: 'Bill of materials', note: 'Renders from data/bom.yaml' },
  { route: '/build', title: 'Build guide', slug: 'build' },
  { route: '/firmware', title: 'Firmware and flashing', slug: 'firmware' },
  { route: '/mounting', title: 'Mounting', slug: 'mounting' },
  { route: '/preflight', title: 'Preflight checklist', note: 'Renders from data/preflight.yaml' },
  { route: '/ground-station', title: 'Ground station', slug: 'ground-station' },
  { route: '/data', title: 'Reading your data', slug: 'reading-your-data' },
  { route: '/flights', title: 'Flight log', note: 'Your flights only in v1' },
  { route: '/troubleshooting', title: 'Troubleshooting', note: 'Renders from data/troubleshooting.yaml' },
  { route: '/reference', title: 'Reference', slug: 'reference' },
  { route: '/reference/telemetry-packet', title: 'Telemetry packet spec', note: 'docs/spec/' },
  { route: '/reference/log-format', title: 'Log format spec', note: 'docs/spec/' },
  { route: '/safety', title: 'Safety and rules', slug: 'safety' },
  { route: '/faq', title: 'FAQ', slug: 'faq' },
  { route: '/about', title: 'About and license', slug: 'about' },
  { route: '/glossary', title: 'Glossary', note: 'Renders from data/glossary.yaml' },
  { route: '/status', title: 'Status', note: 'This page' },
]

export type PageStatusRow = {
  route: string
  title: string
  exists: boolean
  status: string
  updated: string | null
  markers: MarkerCount | null
  note?: string
}

export function pageStatuses(): PageStatusRow[] {
  return PLANNED.map((p) => {
    const path = p.slug ? join(CONTENT_DIR, `${p.slug}.md`) : null
    if (!path || !existsSync(path)) {
      // Pages with no Markdown source are either generated from data or not
      // written yet. The distinction is whether anything renders at the route.
      const generated = ['/bom', '/glossary', '/status', '/'].includes(p.route)
      return {
        route: p.route,
        title: p.title,
        exists: generated,
        status: generated ? 'draft' : 'not written',
        updated: null,
        markers: null,
        note: p.note,
      }
    }
    const { data, content } = matter(readFileSync(path, 'utf8'))
    const fm = data as Frontmatter
    return {
      route: p.route,
      title: p.title,
      exists: true,
      status: fm.status,
      updated: isoDate(fm.updated),
      markers: countMarkers(content),
      note: p.note,
    }
  })
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
  for (const file of [...walk(CONTENT_DIR), ...walk(DATA_DIR)]) {
    const m = countMarkers(readFileSync(file, 'utf8'))
    totals.verify += m.verify
    totals.confirmOnHardware += m.confirmOnHardware
    totals.confirm += m.confirm
    totals.photo += m.photo
    totals.total += m.total
  }
  return totals
}

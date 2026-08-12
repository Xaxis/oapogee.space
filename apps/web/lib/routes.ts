// Routes are shorter than filenames on purpose. `/data` is what somebody types
// and links to; `reading-your-data.md` is what makes sense in a directory
// listing. This map is the one place the two are reconciled, and both the
// dynamic content route and the status page read it, so a page cannot exist in
// one and not the other.

export type ContentRoute = {
  slug: string // URL segment
  file: string // filename in content/, without .md
  group: 'Build' | 'Fly' | 'Reference'
  nav: string // short label for navigation
}

export const CONTENT_ROUTES: ContentRoute[] = [
  { slug: 'start', file: 'start-here', group: 'Build', nav: 'Start here' },
  { slug: 'build', file: 'build', group: 'Build', nav: 'Build guide' },
  { slug: 'firmware', file: 'firmware', group: 'Build', nav: 'Firmware' },
  { slug: 'mounting', file: 'mounting', group: 'Build', nav: 'Mounting' },
  { slug: 'ground-station', file: 'ground-station', group: 'Build', nav: 'Ground station' },
  { slug: 'data', file: 'reading-your-data', group: 'Fly', nav: 'Reading your data' },
  { slug: 'safety', file: 'safety', group: 'Fly', nav: 'Safety and rules' },
  { slug: 'reference', file: 'reference', group: 'Reference', nav: 'Reference' },
  { slug: 'faq', file: 'faq', group: 'Reference', nav: 'FAQ' },
  { slug: 'about', file: 'about', group: 'Reference', nav: 'About' },
]

export const routeForSlug = (slug: string) => CONTENT_ROUTES.find((r) => r.slug === slug)

// Pages generated from data or from docs/spec rather than from content/.
export const GENERATED_ROUTES = [
  { href: '/bom', group: 'Build', nav: 'Bill of materials' },
  { href: '/preflight', group: 'Fly', nav: 'Preflight checklist' },
  { href: '/troubleshooting', group: 'Fly', nav: 'Troubleshooting' },
  { href: '/flights', group: 'Fly', nav: 'Flight log' },
  { href: '/reference/telemetry-packet', group: 'Reference', nav: 'Packet format' },
  { href: '/reference/log-format', group: 'Reference', nav: 'Log format' },
  { href: '/glossary', group: 'Reference', nav: 'Glossary' },
  { href: '/changelog', group: 'Reference', nav: 'Changelog' },
  { href: '/status', group: 'Reference', nav: 'Status' },
] as const

// Navigation order within each group is deliberate: the order somebody does
// things, not alphabetical.
export const NAV_GROUPS = ['Build', 'Fly', 'Reference'] as const

export function navFor(group: (typeof NAV_GROUPS)[number]) {
  const fromContent = CONTENT_ROUTES.filter((r) => r.group === group).map((r) => ({
    href: `/${r.slug}`,
    label: r.nav,
  }))
  const fromGenerated = GENERATED_ROUTES.filter((r) => r.group === group).map((r) => ({
    href: r.href,
    label: r.nav,
  }))

  const ORDER: Record<string, string[]> = {
    Build: [
      '/start',
      '/bom',
      '/build',
      '/firmware',
      '/mounting',
      '/ground-station',
    ],
    Fly: ['/preflight', '/safety', '/data', '/troubleshooting', '/flights'],
    Reference: [
      '/reference',
      '/reference/telemetry-packet',
      '/reference/log-format',
      '/glossary',
      '/faq',
      '/changelog',
      '/status',
      '/about',
    ],
  }

  const all = [...fromContent, ...fromGenerated]
  return ORDER[group]
    .map((href) => all.find((item) => item.href === href))
    .filter((x): x is { href: string; label: string } => Boolean(x))
}

export type NavItem = { href: string; label: string; group: string }

// The whole site in reading order, which is also the sidebar order and the
// order the previous/next links walk. One list means those three cannot
// disagree, which they did on every documentation site any of us has used.
export const FLAT_NAV: NavItem[] = NAV_GROUPS.flatMap((group) =>
  navFor(group).map((item) => ({ ...item, group }))
)

export function findNav(pathname: string): NavItem | undefined {
  return FLAT_NAV.find((item) => item.href === pathname)
}

/**
 * The page before and after this one in reading order.
 *
 * Reading order deliberately crosses group boundaries: somebody who finishes
 * the last page of Build is ready for the first page of Fly, and stopping them
 * at the edge of a section would be an artefact of how the sidebar is grouped
 * rather than anything to do with the material.
 */
export function neighbours(pathname: string): { prev?: NavItem; next?: NavItem } {
  const i = FLAT_NAV.findIndex((item) => item.href === pathname)
  if (i === -1) return {}
  return { prev: FLAT_NAV[i - 1], next: FLAT_NAV[i + 1] }
}

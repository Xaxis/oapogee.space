import type { MetadataRoute } from 'next'
import { pageStatuses } from '@/lib/status'

const SITE_URL = process.env.NEXT_PUBLIC_SITE_URL ?? 'https://oapogee.space'

// Only routes that actually render. Listing planned pages would put 404s in the
// sitemap, which is both useless to a crawler and a small lie about how
// finished the site is.
export default function sitemap(): MetadataRoute.Sitemap {
  const pages = pageStatuses()
    .filter((p) => p.exists)
    .map((p) => ({
      url: `${SITE_URL}${p.route}`,
      lastModified: p.updated ? new Date(p.updated) : new Date(),
      priority: p.route === '/' ? 1 : 0.7,
    }))

  return [...pages, { url: `${SITE_URL}/roadmap`, lastModified: new Date(), priority: 0.3 }]
}

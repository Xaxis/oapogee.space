import type { MetadataRoute } from 'next'
import { CONTENT_ROUTES, GENERATED_ROUTES } from '@/lib/routes'

const SITE_URL = process.env.NEXT_PUBLIC_SITE_URL ?? 'https://oapogee.space'

/**
 * Every route the site actually renders.
 *
 * Built from the route table rather than from a page-status model, which is
 * what this used to read. That model existed to drive a status page which is
 * gone, and a sitemap is a list of URLs: it does not need to know how finished
 * any of them is.
 */
export default function sitemap(): MetadataRoute.Sitemap {
  const routes = ['/', ...CONTENT_ROUTES.map((r) => `/${r.slug}`), ...GENERATED_ROUTES.map((r) => r.href)]
  return [...new Set(routes)].map((route) => ({
    url: `${SITE_URL}${route}`,
    lastModified: new Date(),
    priority: route === '/' ? 1 : 0.7,
  }))
}

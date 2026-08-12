import { notFound } from 'next/navigation'
import type { Metadata } from 'next'
import { getPage } from '@/lib/content'
import { DocPage } from '@/components/DocPage'
import { CONTENT_ROUTES, routeForSlug } from '@/lib/routes'

// One route for every prose page. Pages generated from data (bill of materials,
// preflight, troubleshooting, flights, glossary, status) have their own
// directories, and an explicit route wins over this dynamic one.

export function generateStaticParams() {
  return CONTENT_ROUTES.map((r) => ({ slug: r.slug }))
}

export const dynamicParams = false

export async function generateMetadata({
  params,
}: {
  params: Promise<{ slug: string }>
}): Promise<Metadata> {
  const { slug } = await params
  const route = routeForSlug(slug)
  if (!route) return {}
  const page = await getPage(route.file)
  if (!page) return {}
  return { title: page.frontmatter.title, description: page.frontmatter.description }
}

export default async function ContentPage({ params }: { params: Promise<{ slug: string }> }) {
  const { slug } = await params
  const route = routeForSlug(slug)
  if (!route) notFound()

  const page = await getPage(route.file)
  if (!page) notFound()

  return <DocPage {...page} />
}

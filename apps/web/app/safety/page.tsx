import { notFound } from 'next/navigation'
import type { Metadata } from 'next'
import { getPage } from '@/lib/content'
import { DocPage } from '@/components/DocPage'

export async function generateMetadata(): Promise<Metadata> {
  const page = await getPage('safety')
  if (!page) return {}
  return { title: page.frontmatter.title, description: page.frontmatter.description }
}

export default async function Safety() {
  const page = await getPage('safety')
  if (!page) notFound()
  return <DocPage {...page} />
}

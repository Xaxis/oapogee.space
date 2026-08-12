import { notFound } from 'next/navigation'
import type { Metadata } from 'next'
import { getSpec } from '@/lib/content'
import { DocPage } from '@/components/DocPage'

export async function generateMetadata(): Promise<Metadata> {
  const page = await getSpec('log-format')
  if (!page) return {}
  return { title: page.frontmatter.title, description: page.frontmatter.description }
}

export default async function LogFormatSpec() {
  const page = await getSpec('log-format')
  if (!page) notFound()
  return <DocPage {...page} />
}

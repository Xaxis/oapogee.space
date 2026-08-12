import { notFound } from 'next/navigation'
import type { Metadata } from 'next'
import { getSpec } from '@/lib/content'
import { DocPage } from '@/components/DocPage'
import { PacketDecoder } from '@/components/PacketDecoder'

export async function generateMetadata(): Promise<Metadata> {
  const page = await getSpec('telemetry-packet')
  if (!page) return {}
  return { title: page.frontmatter.title, description: page.frontmatter.description }
}

export default async function TelemetryPacketSpec() {
  const page = await getSpec('telemetry-packet')
  if (!page) notFound()
  return (
    <>
      <DocPage {...page} />
      {/* Below the specification rather than above it: somebody who has just
          read the field tables is exactly who wants to check bytes against
          them. */}
      <PacketDecoder />
    </>
  )
}

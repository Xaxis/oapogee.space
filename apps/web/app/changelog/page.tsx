import type { Metadata } from 'next'
import { notFound } from 'next/navigation'
import { getRepoDoc } from '@/lib/content'

export const metadata: Metadata = {
  title: 'Changelog',
  description:
    'Notable changes to oApogee, hardware, firmware and documentation together, including errata.',
}

export default async function Changelog() {
  const html = await getRepoDoc('CHANGELOG')
  if (!html) notFound()

  return (
    <article>
      <header className="mb-10 max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Changelog</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Hardware, firmware and documentation move together, so they are versioned together.
        </p>
        <p className="mt-4 text-sm text-[var(--color-muted)]">
          Errata live here too, because for a flight computer they are safety information. If a
          firmware release changes an apogee detection threshold, somebody still flying the previous
          build needs a way to find that out.
        </p>
      </header>
      <div className="prose" dangerouslySetInnerHTML={{ __html: html }} />
    </article>
  )
}

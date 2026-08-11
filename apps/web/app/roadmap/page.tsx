import type { Metadata } from 'next'
import { notFound } from 'next/navigation'
import { getDoc } from '@/lib/content'

export const metadata: Metadata = {
  title: 'Page map',
  description: 'The planned information architecture for the oApogee documentation, in the open.',
}

// docs/page-map.md is a working document awaiting approval, and it renders here
// rather than only living in the repository so it can be read and argued with
// on a phone. It is the same file, not a copy.
export default async function Roadmap() {
  const doc = await getDoc('page-map')
  if (!doc) notFound()

  return (
    <article>
      <header className="mb-10 max-w-[46rem]">
        <span className="chip chip-draft">proposal, awaiting approval</span>
        <p className="mt-4 text-[var(--color-muted)]">
          This is the working plan for the site, rendered from the same file that lives in the
          repository. It is published while it is still a draft because a documentation project that
          hides its own outline is not much of an open project.
        </p>
      </header>
      <div className="prose" dangerouslySetInnerHTML={{ __html: doc.html }} />
    </article>
  )
}

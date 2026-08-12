import type { Metadata } from 'next'
import { getGlossary } from '@/lib/data'

export const metadata: Metadata = {
  title: 'Glossary',
  description:
    'Model rocketry and telemetry terms used across the oApogee documentation, defined once.',
}

export default function GlossaryPage() {
  const { terms, updated } = getGlossary()
  const sorted = [...terms].sort((a, b) => a.term.localeCompare(b.term))

  return (
    <div>
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Glossary</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Every term the rest of the site assumes you know. Defined once, here, so that no page has
          to choose between explaining itself and getting to the point.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">updated {updated}</span>
          <span className="chip">{terms.length} terms</span>
        </div>
      </header>

      <nav className="mt-10 flex flex-wrap gap-x-4 gap-y-2 border-y border-[var(--color-line)] py-4 text-sm">
        {sorted.map((t) => (
          <a key={t.id} href={`#${t.id}`} className="!text-[var(--color-muted)] !no-underline hover:!text-white">
            {t.term}
          </a>
        ))}
      </nav>

      <dl className="mt-10 flex max-w-[46rem] flex-col gap-10">
        {sorted.map((term) => (
          <div key={term.id} id={term.id} className="scroll-mt-24">
            <dt className="flex flex-wrap items-baseline gap-3">
              <span className="text-xl font-semibold text-white">{term.term}</span>
              <span className="text-sm text-[var(--color-hivis)]">{term.short}</span>
            </dt>
            <dd className="mt-3 text-[var(--color-muted)]">{term.long}</dd>
            {(term.see_also?.length || term.source) && (
              <dd className="mt-3 flex flex-wrap items-center gap-x-4 gap-y-1 text-sm">
                {term.see_also?.map((ref) => {
                  const target = terms.find((t) => t.id === ref)
                  return target ? (
                    <a key={ref} href={`#${ref}`} className="text-[var(--color-dim)]">
                      {target.term}
                    </a>
                  ) : null
                })}
                {term.source && (
                  <a href={term.source} className="font-mono text-xs">
                    primary source
                  </a>
                )}
              </dd>
            )}
          </div>
        ))}
      </dl>
    </div>
  )
}

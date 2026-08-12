import type { Metadata } from 'next'
import { getTroubleshooting } from '@/lib/data'

export const metadata: Metadata = {
  title: 'Troubleshooting',
  description:
    'Symptom-first troubleshooting for oApogee. Find what you saw, not what you think broke.',
}

export default function Troubleshooting() {
  const { categories, entries, updated } = getTroubleshooting()

  return (
    <div>
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">
          Troubleshooting
        </h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Organised by what you saw, not by which component failed. When you have a problem you know
          the symptom, and a page sorted by subsystem asks you to diagnose before you can look
          anything up.
        </p>
        <p className="mt-4 text-[var(--color-muted)]">
          Within each entry, checks are ordered by cost: the free thing to try first is first, and
          the thing that involves desoldering is last. Most of these end at check one or two.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">updated {updated}</span>
          <span className="chip">{entries.length} symptoms</span>
        </div>
      </header>

      <nav className="mt-10 border-y border-[var(--color-line)] py-5">
        <div className="grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
          {categories.map((cat) => (
            <div key={cat.id}>
              <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                {cat.title}
              </div>
              <ul className="mt-2 flex flex-col gap-1 text-sm">
                {entries
                  .filter((e) => e.category === cat.id)
                  .map((e) => (
                    <li key={e.id}>
                      <a
                        href={`#${e.id}`}
                        className="!text-[var(--color-muted)] !no-underline hover:!text-white"
                      >
                        {e.symptom}
                      </a>
                    </li>
                  ))}
              </ul>
            </div>
          ))}
        </div>
      </nav>

      <div className="mt-12 flex flex-col gap-14">
        {categories.map((cat) => (
          <section key={cat.id}>
            <h2 className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
              {cat.title}
            </h2>

            <div className="mt-6 flex flex-col gap-10">
              {entries
                .filter((e) => e.category === cat.id)
                .map((entry) => (
                  <article key={entry.id} id={entry.id} className="max-w-[46rem] scroll-mt-24">
                    <h3 className="text-xl font-semibold text-white">
                      &ldquo;{entry.symptom}&rdquo;
                    </h3>
                    {entry.tiers.length < 3 && (
                      <span className="chip mt-2 inline-flex">
                        {entry.tiers.join(', ')} only
                      </span>
                    )}

                    <ol className="mt-4 flex flex-col gap-4">
                      {entry.checks.map((check, i) => (
                        <li key={check.do} className="flex gap-4">
                          <span className="mt-0.5 font-mono text-xs text-[var(--color-dim)]">
                            {String(i + 1).padStart(2, '0')}
                          </span>
                          <div>
                            <div className="flex flex-wrap items-baseline gap-2">
                              <span className="font-medium text-[var(--color-body)]">
                                {check.do}
                              </span>
                              {check.critical && (
                                <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-alert)]">
                                  critical
                                </span>
                              )}
                            </div>
                            {check.detail && (
                              <p className="mt-1 text-sm text-[var(--color-muted)]">
                                {check.detail}
                              </p>
                            )}
                          </div>
                        </li>
                      ))}
                    </ol>

                    {entry.note && (
                      <p className="mt-4 border-l-2 border-[var(--color-line-bright)] pl-4 text-sm text-[var(--color-muted)]">
                        {entry.note}
                      </p>
                    )}

                    {entry.see_also && entry.see_also.length > 0 && (
                      <p className="mt-4 text-sm text-[var(--color-dim)]">
                        See also{' '}
                        {entry.see_also.map((ref, i) => {
                          const target = entries.find((e) => e.id === ref)
                          if (!target) return null
                          return (
                            <span key={ref}>
                              {i > 0 && ', '}
                              <a href={`#${ref}`}>{target.symptom.toLowerCase()}</a>
                            </span>
                          )
                        })}
                      </p>
                    )}
                  </article>
                ))}
            </div>
          </section>
        ))}
      </div>
    </div>
  )
}

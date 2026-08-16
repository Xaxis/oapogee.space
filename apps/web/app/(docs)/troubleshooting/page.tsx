import type { Metadata } from 'next'
import { getTroubleshooting, type TroubleEntry } from '@/lib/data'
import { DocTabs } from '@/components/DocTabs'

export const metadata: Metadata = {
  title: 'Troubleshooting',
  description:
    'Symptom-first troubleshooting for oApogee. Find what you saw, not what you think broke.',
}

// Panelled by category rather than stacked. Somebody chasing a radio symptom
// should not scroll through the power symptoms to reach it, and these are six
// unrelated lists rather than one argument.
//
// Build checkpoints link straight to a symptom anchor, and those anchors live
// inside panels that are closed by default. DocTabs resolves a hash to the
// panel containing it, so those links keep working.

function Entry({ entry, all }: { entry: TroubleEntry; all: TroubleEntry[] }) {
  return (
    <article id={entry.id} className="max-w-[46rem] scroll-mt-24">
      <h3 className="text-xl font-semibold text-white">&ldquo;{entry.symptom}&rdquo;</h3>
      {entry.tiers.length < 3 && (
        <span className="chip mt-2 mr-2 inline-flex">{entry.tiers.join(', ')} only</span>
      )}
      {/* Path gating was in the data and never on the page, so an entry about
          a part that only exists on the custom board was shown to Modules
          builders with nothing to say it did not apply to them. */}
      {entry.paths && entry.paths.length === 1 && (
        <span className="chip mt-2 inline-flex">{entry.paths[0]} path only</span>
      )}

      <ol className="mt-4 flex flex-col gap-4">
        {entry.checks.map((check, i) => (
          <li key={check.do} className="flex gap-4">
            <span className="mt-0.5 font-mono text-xs text-[var(--color-dim)]">
              {String(i + 1).padStart(2, '0')}
            </span>
            <div>
              <div className="flex flex-wrap items-baseline gap-2">
                <span className="font-medium text-[var(--color-body)]">{check.do}</span>
                {check.critical && (
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-alert)]">
                    critical
                  </span>
                )}
              </div>
              {check.detail && (
                <p className="mt-1 text-sm text-[var(--color-muted)]">{check.detail}</p>
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
            const target = all.find((e) => e.id === ref)
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
  )
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
          Within each entry the checks are ordered by cost: the free thing to try first is first,
          and the thing that involves desoldering is last. Most end at check one or two.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">updated {updated}</span>
          <span className="chip">{entries.length} symptoms</span>
        </div>
      </header>

      <div className="mt-10">
        <DocTabs
          label="Symptom category"
          tabs={categories.map((cat) => {
            const mine = entries.filter((e) => e.category === cat.id)
            return {
              id: cat.id,
              label: cat.title,
              hint: `${mine.length} symptom${mine.length === 1 ? '' : 's'}.`,
              content: (
                <div className="flex flex-col gap-12">
                  {mine.map((entry) => (
                    <Entry key={entry.id} entry={entry} all={entries} />
                  ))}
                </div>
              ),
            }
          })}
        />
      </div>
    </div>
  )
}

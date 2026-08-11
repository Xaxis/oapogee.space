import type { Metadata } from 'next'
import Link from 'next/link'
import { pageStatuses, repoMarkerTotals } from '@/lib/status'

export const metadata: Metadata = {
  title: 'Status',
  description:
    'What is written, what is reviewed, and what is still unmeasured. The oApogee accuracy rule, made visible.',
}

const CHIP: Record<string, string> = {
  verified: 'chip-verified',
  'needs-review': 'chip-draft',
  draft: 'chip-draft',
  'not written': 'chip-blocked',
}

export default function Status() {
  const rows = pageStatuses()
  const totals = repoMarkerTotals()
  const written = rows.filter((r) => r.exists).length

  return (
    <div className="flex flex-col gap-14">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Status</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          What is written, what has been reviewed, and how much of this site is still unmeasured.
        </p>
        <p className="mt-4 text-[var(--color-muted)]">
          oApogee never publishes a number it has not measured or sourced. That rule only means
          something if the gaps are countable, so this page counts them. Every figure below comes
          from the content files themselves, not from a hand-maintained list.
        </p>
      </header>

      <section className="grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
        {[
          { label: 'Pages live', value: `${written} of ${rows.length}` },
          { label: 'Needs a source', value: String(totals.verify) },
          { label: 'Needs hardware', value: String(totals.confirmOnHardware) },
          { label: 'Needs a decision', value: String(totals.confirm) },
        ].map((s) => (
          <div
            key={s.label}
            className="rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5"
          >
            <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
              {s.label}
            </div>
            <div className="mt-2 text-2xl font-semibold text-[var(--color-hivis)]">{s.value}</div>
          </div>
        ))}
      </section>

      <section>
        <h2 className="text-xl font-semibold text-white">What each status means</h2>
        <dl className="mt-4 grid max-w-3xl gap-4 sm:grid-cols-2">
          {[
            ['draft', 'Written. Nobody has checked it. Read the primary sources it links to.'],
            ['needs-review', 'The author believes it is right and wants a second opinion.'],
            [
              'verified',
              'A human with the relevant expertise checked every number and every step on real hardware. Nothing reaches this while it still has open markers.',
            ],
            ['not written', 'Planned in the page map. Does not exist yet.'],
          ].map(([k, v]) => (
            <div key={k}>
              <dt>
                <span className={`chip ${CHIP[k]}`}>{k}</span>
              </dt>
              <dd className="mt-2 text-sm text-[var(--color-muted)]">{v}</dd>
            </div>
          ))}
        </dl>
      </section>

      <section>
        <h2 className="text-xl font-semibold text-white">Every page</h2>
        <div className="table-scroll mt-5">
          <table className="data">
            <thead>
              <tr>
                <th>Page</th>
                <th>Status</th>
                <th className="text-center">Open markers</th>
                <th>Updated</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((row) => (
                <tr key={row.route}>
                  <td>
                    {row.exists ? (
                      <Link href={row.route} className="font-medium">
                        {row.title}
                      </Link>
                    ) : (
                      <span className="font-medium text-[var(--color-dim)]">{row.title}</span>
                    )}
                    <div className="mt-1 font-mono text-xs text-[var(--color-dim)]">
                      {row.route}
                      {row.note ? ` . ${row.note}` : ''}
                    </div>
                  </td>
                  <td>
                    <span className={`chip ${CHIP[row.status] ?? ''}`}>{row.status}</span>
                  </td>
                  <td className="text-center">
                    {row.markers ? (
                      row.markers.total || <span className="text-[var(--color-dim)]">0</span>
                    ) : (
                      <span className="text-[var(--color-dim)]">.</span>
                    )}
                  </td>
                  <td className="whitespace-nowrap text-[var(--color-muted)]">
                    {row.updated ?? <span className="text-[var(--color-dim)]">.</span>}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section className="max-w-[46rem]">
        <h2 className="text-xl font-semibold text-white">Why so much is missing</h2>
        <p className="mt-3 text-[var(--color-muted)]">
          The hardware is a design on paper. Nothing has been fabricated, assembled, weighed, priced,
          or flown. Every one of the markers counted above is a place where a specific number was
          deliberately left out rather than guessed at, and each one records what evidence would
          close it.
        </p>
        <p className="mt-3 text-[var(--color-muted)]">
          The full list lives in{' '}
          <a href="https://github.com/Xaxis/oapogee.space/blob/main/TODO-VERIFY.md">
            TODO-VERIFY.md
          </a>
          , which is generated from the content files and checked in the build, so it cannot drift
          out of date. The plan for the rest of the site is the{' '}
          <Link href="/roadmap">page map</Link>.
        </p>
      </section>
    </div>
  )
}

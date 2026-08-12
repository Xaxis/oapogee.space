import type { Metadata } from 'next'
import Link from 'next/link'
import { pageStatuses, repoMarkerTotals, repoMarkers } from '@/lib/status'

// Ordered by how close each kind is to being closeable: sourcing is desk work,
// hardware needs a fabricated board, photos need a finished one.
const MARKER_GROUPS = [
  {
    kind: 'verify',
    heading: 'Needs a source or a measurement',
    blurb:
      'A number, price, link, or claim that is absent rather than guessed. Each says what evidence would close it.',
  },
  {
    kind: 'confirm-on-hardware',
    heading: 'Needs the physical hardware',
    blurb:
      'Procedures written out but never performed on a real board. No page containing one can be marked verified.',
  },
  {
    kind: 'confirm',
    heading: 'Needs a decision',
    blurb: 'Open design questions, waiting on a maintainer call rather than on evidence.',
  },
  {
    kind: 'photo',
    heading: 'Needs a photograph',
    blurb: 'Image slots, each describing what the photograph has to show.',
  },
]

export const metadata: Metadata = {
  title: 'Status',
  description:
    'What is written, what is reviewed, and what is still unmeasured. The oApogee accuracy rule, made visible.',
}

const CHIP: Record<string, string> = {
  verified: 'chip-verified',
  'needs-review': 'chip-draft',
  draft: 'chip-draft',
  generated: '',
  'not written': 'chip-blocked',
}

export default function Status() {
  const rows = pageStatuses()
  const totals = repoMarkerTotals()
  const markers = repoMarkers()
  const written = rows.filter((r) => r.status !== 'not written').length
  const verified = rows.filter((r) => r.status === 'verified').length

  return (
    <div className="space-y-14">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Status</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          What is written, what has been reviewed, and how much of this site is still unmeasured.
        </p>
        <p className="mt-4 text-[var(--color-muted)]">
          oApogee never publishes a number it has not measured or sourced. That rule only means
          something if the gaps are countable, so this page counts them, from the content files
          rather than from a list somebody maintains by hand.
        </p>
      </header>

      <section className="grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
        {[
          { label: 'Pages live', value: `${written} of ${rows.length}` },
          { label: 'Pages verified', value: `${verified}` },
          { label: 'Needs a source', value: String(totals.verify) },
          { label: 'Needs hardware', value: String(totals.confirmOnHardware) },
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
            [
              'generated',
              'Rendered from structured data rather than written prose. Its accuracy is the data file’s, and the build fails if the data stops resolving.',
            ],
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
                    {row.status === 'not written' ? (
                      <span className="font-medium text-[var(--color-dim)]">{row.title}</span>
                    ) : (
                      <Link href={row.route} className="font-medium">
                        {row.title}
                      </Link>
                    )}
                    <div className="mt-1 font-mono text-xs text-[var(--color-dim)]">
                      {row.route} . {row.source}
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
          The hardware is a design on paper. Nothing has been fabricated, assembled, weighed,
          priced, or flown. Each of the {totals.total} markers below is a place where a specific
          number was deliberately left out rather than guessed at, and each records what evidence
          would close it.
        </p>
        <p className="mt-3 text-[var(--color-muted)]">
          This list is read from the content files when the site is built, so it cannot go stale.
          Closing one means doing the measurement, not editing an index.
        </p>
      </section>

      <section>
        <h2 className="text-xl font-semibold text-white">Every open question</h2>
        <div className="mt-6 flex flex-col gap-10">
          {MARKER_GROUPS.map((group) => {
            const mine = markers.filter((m) => m.kind === group.kind)
            if (!mine.length) return null

            const byFile = new Map<string, typeof mine>()
            for (const m of mine) {
              if (!byFile.has(m.file)) byFile.set(m.file, [])
              byFile.get(m.file)!.push(m)
            }

            return (
              <div key={group.kind} id={group.kind} className="scroll-mt-24">
                <h3 className="text-lg font-semibold text-white">{group.heading}</h3>
                <p className="mt-1 max-w-2xl text-sm text-[var(--color-muted)]">{group.blurb}</p>

                <div className="mt-5 flex flex-col gap-6">
                  {[...byFile.entries()].sort().map(([file, entries]) => (
                    <div key={file}>
                      <a
                        href={`https://github.com/Xaxis/oapogee.space/blob/main/${file}`}
                        className="font-mono text-xs !text-[var(--color-dim)] hover:!text-white"
                      >
                        {file}
                      </a>
                      <ul className="mt-2 flex max-w-3xl flex-col gap-2">
                        {entries.map((m) => (
                          <li key={`${m.file}:${m.line}`} className="flex gap-3 text-sm">
                            <span className="shrink-0 font-mono text-xs text-[var(--color-dim)]">
                              L{m.line}
                            </span>
                            <span className="text-[var(--color-muted)]">{m.text}</span>
                          </li>
                        ))}
                      </ul>
                    </div>
                  ))}
                </div>
              </div>
            )
          })}
        </div>
      </section>
    </div>
  )
}

import type { Metadata } from 'next'
import Link from 'next/link'
import { getFlights } from '@/lib/data'

export const metadata: Metadata = {
  title: 'Flight log',
  description:
    'Real oApogee flights, and the submission format the archive will accept. Currently empty, because nothing has flown.',
}

export default function FlightLog() {
  const { flights, submissions_open, submissions_note, submission, archive_rules, updated } =
    getFlights()

  return (
    <div className="space-y-14">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Flight log</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Real flights, with the data behind them. Comparable only because every entry carries the
          motor, the airframe and the liftoff mass alongside the number.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">updated {updated}</span>
          <span className="chip">{flights.length} flights</span>
        </div>
      </header>

      {flights.length === 0 && (
        <section className="rounded-lg border border-dashed border-[var(--color-line-bright)] bg-[var(--color-surface)] p-8">
          <h2 className="text-xl font-semibold text-white">No flights yet</h2>
          <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
            Nothing has been built, so nothing has flown. This page stays empty until there is a
            real flight to put in it, rather than being filled with a simulation to look
            established.
          </p>
        </section>
      )}

      <section className="max-w-[46rem]">
        <h2 className="text-2xl font-semibold text-white">Submitting a flight</h2>
        <p className="mt-3 text-[var(--color-muted)]">
          {submissions_open ? 'Submissions are open.' : submissions_note}
        </p>
        <p className="mt-3 text-[var(--color-muted)]">
          The format is specified now anyway, so that opening it later is a mechanism change rather
          than a redesign, and so that anyone recording flights today records the fields that will
          be needed. The principle: anything the payload already knows is taken from the log rather
          than typed in, because a field a human retypes is a field a human gets wrong.
        </p>
      </section>

      <section>
        <h3 className="text-lg font-semibold text-white">Files</h3>
        <div className="mt-4 flex max-w-[46rem] flex-col gap-4">
          {submission.required_files.map((file) => (
            <div key={file.name} className="rounded-lg border border-[var(--color-line)] p-4">
              <div className="flex flex-wrap items-baseline gap-2">
                <code className="font-mono text-sm text-[var(--color-hivis)]">{file.name}</code>
                <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                  {file.optional ? 'if present' : 'required'}
                </span>
              </div>
              {file.why && <p className="mt-2 text-sm text-[var(--color-muted)]">{file.why}</p>}
            </div>
          ))}
        </div>
      </section>

      <section>
        <h3 className="text-lg font-semibold text-white">Taken from the log</h3>
        <p className="mt-2 max-w-2xl text-sm text-[var(--color-muted)]">
          Read out of the files above. Never typed in.
        </p>
        <div className="mt-4 flex flex-wrap gap-2">
          {submission.derived_fields.map((f) => (
            <code
              key={f.field}
              className="rounded border border-[var(--color-line)] px-2 py-1 font-mono text-xs text-[var(--color-muted)]"
            >
              {f.field}
            </code>
          ))}
        </div>
      </section>

      <section>
        <h3 className="text-lg font-semibold text-white">What you tell us</h3>
        <p className="mt-2 max-w-2xl text-sm text-[var(--color-muted)]">
          Everything the payload cannot know. Kept short on purpose: a long form is a form nobody
          completes.
        </p>
        <div className="table-scroll mt-4 max-w-3xl">
          <table className="data">
            <thead>
              <tr>
                <th>Field</th>
                <th>Required</th>
                <th>Why</th>
              </tr>
            </thead>
            <tbody>
              {submission.declared_fields.map((f) => (
                <tr key={f.field}>
                  <td>
                    <code className="font-mono text-xs text-[var(--color-body)]">{f.field}</code>
                    {f.example && (
                      <div className="mt-1 text-xs text-[var(--color-dim)]">e.g. {f.example}</div>
                    )}
                    {f.options && (
                      <div className="mt-1 font-mono text-xs text-[var(--color-dim)]">
                        {f.options.join(' | ')}
                      </div>
                    )}
                  </td>
                  <td className="whitespace-nowrap text-[var(--color-muted)]">
                    {f.required ? 'yes' : 'optional'}
                  </td>
                  <td className="text-[var(--color-muted)]">{f.why ?? ''}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section className="max-w-[46rem]">
        <h3 className="text-lg font-semibold text-white">What is never collected</h3>
        <div className="mt-4 flex flex-col gap-4">
          {submission.never_collected.map((f) => (
            <div key={f.field} className="border-l-2 border-[var(--color-alert)] pl-4">
              <div className="font-medium text-[var(--color-body)]">{f.field}</div>
              <p className="mt-1 text-sm text-[var(--color-muted)]">{f.why}</p>
            </div>
          ))}
        </div>
      </section>

      <section className="max-w-[46rem]">
        <h3 className="text-lg font-semibold text-white">Rules this archive holds itself to</h3>
        <ol className="mt-4 flex flex-col gap-4">
          {archive_rules.map((rule, i) => (
            <li key={rule.id} className="flex gap-4">
              <span className="mt-0.5 font-mono text-xs text-[var(--color-dim)]">
                {String(i + 1).padStart(2, '0')}
              </span>
              <p className="text-[var(--color-muted)]">{rule.rule}</p>
            </li>
          ))}
        </ol>
      </section>
    </div>
  )
}

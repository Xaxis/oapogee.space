import Link from 'next/link'
import { getTiers } from '@/lib/data'

// The brief asks the homepage to lead with a real annotated flight graph rather
// than a hero illustration, and that is the right instinct. It is also not
// currently possible: no board exists, so no flight exists. A project whose
// entire pitch is honest data cannot open with fabricated data, however
// attractively labelled. So the hero states where the project actually is, and
// the graph replaces it the day there is a flight to graph.

function Stat({ label, value, note }: { label: string; value: string; note: string }) {
  return (
    <div className="rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5">
      <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
        {label}
      </div>
      <div className="mt-2 text-2xl font-semibold text-[var(--color-hivis)]">{value}</div>
      <div className="mt-1 text-sm text-[var(--color-muted)]">{note}</div>
    </div>
  )
}

export default function Home() {
  const { tiers, paths, scope, envelope, upgrade_promise } = getTiers()

  return (
    <div className="flex flex-col gap-20">
      <section>
        <p className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
          Open source rocket telemetry
        </p>
        <h1 className="mt-4 max-w-3xl text-4xl font-semibold leading-[1.1] text-white sm:text-5xl">
          A small, cheap sensor package that tells you exactly what your model rocket did.
        </h1>
        <p className="mt-6 max-w-2xl text-lg text-[var(--color-muted)]">
          oApogee records altitude, acceleration and orientation through the whole flight, logs it
          onboard, and can send it live to a receiver on the ground. It attaches to a rocket you
          already own. The documentation is the product: everything you need to build one is on this
          site.
        </p>

        <div className="mt-8 flex flex-wrap gap-3">
          <Link
            href="/bom"
            className="rounded-md border border-[var(--color-hivis)] px-4 py-2 text-sm font-medium !text-[var(--color-hivis)] !no-underline hover:bg-[var(--color-hivis)] hover:!text-black"
          >
            See what it costs
          </Link>
          <Link
            href="/safety"
            className="rounded-md border border-[var(--color-line-bright)] px-4 py-2 text-sm font-medium !text-[var(--color-body)] !no-underline hover:border-[var(--color-body)]"
          >
            Safety and rules
          </Link>
        </div>
      </section>

      {/* Where the flight graph goes. Saying so is better than filling it. */}
      <section className="rounded-lg border border-dashed border-[var(--color-line-bright)] bg-[var(--color-surface)] p-8">
        <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-orange)]">
          Project status, read this first
        </div>
        <h2 className="mt-3 max-w-2xl text-2xl font-semibold text-white">
          oApogee is a design on paper. Nothing has been built and nothing has flown.
        </h2>
        <p className="mt-4 max-w-2xl text-[var(--color-muted)]">
          This space is reserved for an annotated graph of a real flight, and it will stay empty
          until there is a real flight to put in it. Every cost, mass, range and battery figure on
          this site is missing for the same reason: none of them have been measured, and inventing
          them would make everything else here worthless.
        </p>
        <p className="mt-4 max-w-2xl text-[var(--color-muted)]">
          What is here now is the design, the parts list, and the safety and regulatory groundwork.
          You can read all of it, and you can tell us where it is wrong.
        </p>
        <Link href="/status" className="mt-5 inline-block text-sm">
          See exactly what is verified and what is not
        </Link>
      </section>

      <section>
        <h2 className="text-xl font-semibold text-white">The numbers, when we have them</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          These are the four figures that decide whether this project is worth your evening. They
          are targets, not measurements, and the site will say so until somebody has weighed one.
        </p>
        <div className="mt-6 grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
          <Stat label="Cost" value="Not priced" note="Target: under $60 for the full build" />
          <Stat label="Flying mass" value="Not weighed" note="Target: under 25 g with battery" />
          <Stat label="Build time" value="Not timed" note="Target: an evening, if you can solder" />
          <Stat label="Data" value="Full profile" note="Altitude, acceleration, orientation" />
        </div>
      </section>

      <section>
        <h2 className="text-xl font-semibold text-white">Three tiers, one board</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">{upgrade_promise}</p>

        <div className="mt-6 grid gap-4 lg:grid-cols-3">
          {tiers.map((tier) => (
            <div
              key={tier.id}
              className="flex flex-col rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-6"
            >
              <h3 className="text-lg font-semibold text-white">{tier.name}</h3>
              <p className="mt-1 text-sm text-[var(--color-hivis)]">{tier.tagline}</p>
              <p className="mt-4 text-sm text-[var(--color-muted)]">{tier.purpose}</p>
              <dl className="mt-5 flex flex-col gap-3 border-t border-[var(--color-line)] pt-4 text-sm">
                <div>
                  <dt className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                    Good for
                  </dt>
                  <dd className="mt-1 text-[var(--color-muted)]">{tier.good_for}</dd>
                </div>
                <div>
                  <dt className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                    Not good for
                  </dt>
                  <dd className="mt-1 text-[var(--color-muted)]">{tier.not_good_for}</dd>
                </div>
              </dl>
              {tier.requires_note && (
                <p className="mt-4 border-l-2 border-[var(--color-orange)] pl-3 text-sm text-[var(--color-muted)]">
                  {tier.requires_note}
                </p>
              )}
            </div>
          ))}
        </div>

        <div className="mt-6 grid gap-4 sm:grid-cols-2">
          {paths.map((path) => (
            <div
              key={path.id}
              className="rounded-lg border border-[var(--color-line)] p-5 text-sm"
            >
              <div className="flex items-center gap-3">
                <span className="font-semibold text-white">{path.name} path</span>
                <span className={`chip ${path.available ? 'chip-verified' : 'chip-blocked'}`}>
                  {path.available ? 'buildable now' : 'not yet fabricated'}
                </span>
              </div>
              <p className="mt-2 text-[var(--color-muted)]">{path.one_liner}</p>
            </div>
          ))}
        </div>
      </section>

      <section className="grid gap-8 lg:grid-cols-2">
        <div>
          <h2 className="text-xl font-semibold text-white">What it does</h2>
          <ul className="mt-4 flex flex-col gap-2 text-[var(--color-muted)]">
            {scope.does.map((item) => (
              <li key={item} className="flex gap-3">
                <span aria-hidden className="text-[var(--color-ok)]">+</span>
                <span>{item}</span>
              </li>
            ))}
          </ul>
        </div>
        <div>
          <h2 className="text-xl font-semibold text-white">What it does not do</h2>
          <ul className="mt-4 flex flex-col gap-2 text-[var(--color-muted)]">
            {scope.does_not.map((item) => (
              <li key={item} className="flex gap-3">
                <span aria-hidden className="text-[var(--color-alert)]">-</span>
                <span>{item}</span>
              </li>
            ))}
          </ul>
          <p className="mt-5 border-l-2 border-[var(--color-alert)] pl-4 text-sm text-[var(--color-muted)]">
            {scope.boundary_statement}
          </p>
        </div>
      </section>

      <section className="rounded-lg border border-[var(--color-line)] p-6">
        <h2 className="text-xl font-semibold text-white">What it is built for</h2>
        <p className="mt-2 text-[var(--color-muted)]">
          <strong className="text-[var(--color-body)]">{envelope.target}.</strong>{' '}
          {envelope.headroom}
        </p>
        <p className="mt-3 text-sm text-[var(--color-muted)]">{envelope.regulatory_note}</p>
      </section>
    </div>
  )
}

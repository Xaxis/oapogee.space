import Link from 'next/link'
import Image from 'next/image'
import { getTiers, getFlightPhases } from '@/lib/data'
import { Hero } from '@/components/hero/Hero'

// The homepage answers one question: what can I do here today.
//
// An earlier version led with what had not been built yet and repeated it in
// three separate places, which is a strange thing for a documentation project
// to do. The hardware being unbuilt is a fact about a date, not about the
// site's usefulness: the design, the parts list, the wire formats, the safety
// groundwork and the schematic all exist and are all buildable from.
//
// The accuracy rule still applies, so the state of the numbers is stated. Once,
// in a line, with a link, rather than as a section with a heading.

const WHATS_HERE = [
  {
    href: '/build',
    title: 'Build guide',
    what: 'Every step, with an observable checkpoint and a link to the failure branch when a checkpoint does not pass.',
  },
  {
    href: '/bom',
    title: 'Bill of materials',
    what: 'Every part, by tier and by build path, with why each was chosen and what substitutes for it.',
  },
  {
    href: '/reference/schematic',
    title: 'Schematic and KiCad',
    what: 'The circuit as code. Drawing, netlist, circuit JSON and a KiCad schematic, all from one source.',
  },
  {
    href: '/reference/telemetry-packet',
    title: 'Wire formats',
    what: 'The packet and log formats, specified well enough to write your own receiver against.',
  },
  {
    href: '/preflight',
    title: 'Preflight checklist',
    what: 'Printable, one action per line, ordered by when it happens rather than by subsystem.',
  },
  {
    href: '/troubleshooting',
    title: 'Troubleshooting',
    what: 'Organised by what you saw, not by which component failed, because that is what you know.',
  },
]

export default function Home() {
  const { tiers, paths, scope, upgrade_promise } = getTiers()
  const { phases } = getFlightPhases()

  return (
    <div className="mx-auto max-w-6xl space-y-24 px-5 py-12">
      <Hero phases={phases} />

      <section>
        <h2 className="text-2xl font-semibold text-white">Everything you need is here</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          The hardware is commodity. The documentation is the product, and it is written to be
          followed start to finish by somebody who has soldered before and never written firmware.
        </p>

        <div className="mt-6 grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {WHATS_HERE.map((item) => (
            <Link
              key={item.href}
              href={item.href}
              className="group rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5 !no-underline transition-colors hover:border-[var(--color-hivis)]"
            >
              <div className="font-medium !text-white group-hover:!text-[var(--color-hivis)]">
                {item.title}
              </div>
              <p className="mt-2 text-sm text-[var(--color-muted)]">{item.what}</p>
            </Link>
          ))}
        </div>

        <p className="mt-6 max-w-2xl text-sm text-[var(--color-dim)]">
          Nothing has been fabricated or flown yet, so the site carries no prices, masses or ranges:
          those are measurements and they have not been taken.{' '}
          <Link href="/status">Exactly what is verified and what is not</Link>.
        </p>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">The whole payload, one sheet</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          Four sensors onto one microcontroller, the log written to flash that is soldered down
          rather than to a card that can shake loose, and one connector for power, charging,
          configuration and offload.{' '}
          <Link href="/reference/schematic">The circuit, in full</Link>.
        </p>
        <div className="mt-6 overflow-x-auto rounded-lg border border-[var(--color-line)]">
          <Image
            src="/schematic/system.svg"
            alt="oApogee system block diagram. Barometer, IMU, high-g accelerometer and GNSS connect to an RP2350 over I2C, SPI and UART. The RP2350 writes to QSPI flash, drives a LoRa radio to a ground station, drives a buzzer and LED, and offloads over USB-C. Power runs USB-C to charger to a 1S LiPo cell to a 3V3 buck-boost rail."
            width={1168}
            height={616}
            className="min-w-[900px] max-w-none"
          />
        </div>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Three tiers, one board</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">{upgrade_promise}</p>

        <div className="mt-6 grid gap-4 lg:grid-cols-3">
          {tiers.map((tier) => (
            <div
              key={tier.id}
              className="flex flex-col rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-6"
            >
              <h3 className="text-lg font-semibold text-white">{tier.name}</h3>
              <p className="mt-1 text-sm text-[var(--color-hivis)]">{tier.tagline}</p>
              <p className="mt-4 text-sm text-[var(--color-muted)]">{tier.good_for}</p>
              {tier.requires_note && (
                <p className="mt-auto pt-4 text-sm text-[var(--color-dim)]">{tier.requires_note}</p>
              )}
            </div>
          ))}
        </div>

        <div className="mt-4 flex flex-wrap gap-x-6 gap-y-2 text-sm text-[var(--color-muted)]">
          {paths.map((path) => (
            <span key={path.id} className="flex items-center gap-2">
              <span className={`chip ${path.available ? 'chip-verified' : 'chip-blocked'}`}>
                {path.name}
              </span>
              {path.one_liner}
            </span>
          ))}
        </div>
      </section>

      <section className="rounded-lg border-l-3 border-[var(--color-alert)] bg-[var(--color-surface)] p-6">
        <h2 className="text-xl font-semibold text-white">What it will not do</h2>
        <p className="mt-3 max-w-2xl text-[var(--color-muted)]">{scope.boundary_statement}</p>
        <Link href="/safety" className="mt-4 inline-block text-sm">
          Safety and rules
        </Link>
      </section>
    </div>
  )
}

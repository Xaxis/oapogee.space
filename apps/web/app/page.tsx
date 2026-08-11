import Link from 'next/link'
import Image from 'next/image'
import { getTiers, getFlightPhases } from '@/lib/data'
import { Hero } from '@/components/hero/Hero'

// Four sections, in the order somebody decides with: what it does, what it is,
// which one to build, and what it will not do. The project's honesty about
// unmeasured numbers is stated once, at the end, rather than repeated on every
// block until it reads as an apology.

export default function Home() {
  const { tiers, paths, scope, upgrade_promise } = getTiers()
  const { phases } = getFlightPhases()

  return (
    <div className="flex flex-col gap-24">
      <Hero phases={phases} />

      <section>
        <h2 className="text-2xl font-semibold text-white">The whole payload, one sheet</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          Four sensors onto one microcontroller, the log written to flash that is soldered down
          rather than to a card that can shake loose, and one connector for power, charging,
          configuration and offload. Generated from the same parts list the{' '}
          <Link href="/bom">bill of materials</Link> renders, so it cannot quietly stop matching the
          hardware.
        </p>
        <div className="mt-6 overflow-x-auto rounded-lg border border-[var(--color-line)]">
          <Image
            src="/schematic/system.svg"
            alt="oApogee system block diagram. Barometer, IMU, high-g accelerometer and GNSS connect to an RP2350 over I2C, SPI and UART. The RP2350 writes to QSPI flash, drives a LoRa radio to a ground station, drives a buzzer and LED, and offloads over USB-C. Power runs USB-C to charger to a 1S LiPo cell to a 3V3 buck-boost rail."
            width={1168}
            height={616}
            className="min-w-[900px] max-w-none"
            priority={false}
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

        <div className="mt-4 flex flex-wrap gap-4 text-sm text-[var(--color-muted)]">
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

      <section className="max-w-2xl border-t border-[var(--color-line)] pt-8">
        <h2 className="text-xl font-semibold text-white">
          Nothing here has been built, and nothing has flown.
        </h2>
        <p className="mt-3 text-[var(--color-muted)]">
          So there are no prices, no masses, no ranges and no battery figures anywhere on this site.
          Not estimates, not placeholders: they are absent, and each gap records what evidence would
          close it. A page full of confident invented specifications is worse than an empty one when
          people spend money and fly hardware over other people&rsquo;s heads based on it.
        </p>
        <Link href="/status" className="mt-4 inline-block text-sm">
          See exactly what is verified and what is not
        </Link>
      </section>
    </div>
  )
}

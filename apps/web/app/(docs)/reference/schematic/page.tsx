import type { Metadata } from 'next'
import Image from 'next/image'
import { readFileSync, statSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { REPO_ROOT } from '@/lib/repo'
import { DocTabs } from '@/components/DocTabs'

export const metadata: Metadata = {
  title: 'Schematic',
  description:
    'The oApogee circuit: schematic, netlist, KiCad project and circuit JSON, all rendered from one tscircuit source.',
}

const HW = join(REPO_ROOT, 'apps/web/public/hardware')

const DOWNLOADS = [
  {
    file: 'oapogee-netlist.txt',
    label: 'Netlist',
    what: 'The connectivity as text. Diffable, reviewable, and the thing to read first.',
  },
  {
    file: 'oapogee-schematic.svg',
    label: 'Schematic',
    what: 'The same connectivity, drawn.',
  },
  {
    file: 'oapogee.kicad_sch',
    label: 'KiCad schematic',
    what: 'For anyone who wants to take the design further in KiCad.',
  },
  {
    file: 'oapogee-circuit.json',
    label: 'Circuit JSON',
    what: 'Machine readable, for anyone writing their own tooling against the design.',
  },
]

function sizeOf(file: string) {
  const path = join(HW, file)
  if (!existsSync(path)) return null
  return `${Math.round(statSync(path).size / 1024)} kB`
}

export default function Schematic() {
  const netlist = existsSync(join(HW, 'oapogee-netlist.txt'))
    ? readFileSync(join(HW, 'oapogee-netlist.txt'), 'utf8')
    : null

  return (
    <div className="space-y-10">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">Schematic</h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          The circuit, written as code and rendered four ways from one source.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">generated from hardware/oapogee.tsx</span>
        </div>
      </header>

      {/* The single most important thing on this page. Somebody who takes these
          pin numbers for datasheet pin numbers will build a board that cannot
          work, and the drawing looks authoritative enough that they might. */}
      <section className="max-w-[46rem] rounded-lg border-l-3 border-[var(--color-alert)] bg-[var(--color-surface)] p-6">
        <h2 className="text-lg font-semibold text-white">This is a netlist, not a pinout</h2>
        <p className="mt-3 text-[var(--color-muted)]">
          It says what connects to what, which is the design decision worth reviewing. It does not
          say which physical pin of any package a signal lands on. The pin numbers below are
          structural placeholders, assigned in the order the labels were written, not read from a
          datasheet.
        </p>
        <p className="mt-3 text-[var(--color-muted)]">
          That is deliberate. This project never publishes a number it has not measured or sourced,
          and a datasheet pin number recalled from memory is exactly the kind of confident,
          plausible, wrong figure that rule exists to prevent. Mapping these functional pins onto
          real packages is a separate step, done against datasheets, and it has not been done.
        </p>
        <p className="mt-3 font-medium text-[var(--color-body)]">
          Do not lay a board out from this, and do not send anything here to a fabricator.
        </p>
      </section>

      <DocTabs
        label="Circuit views"
        tabs={[
          {
            id: 'drawing',
            label: 'Drawing',
            hint: 'Power along the bottom left, the microcontroller in the middle with every bus leaving its right side, sensors and radio in a column on the right. Arranged to read the same way the system block diagram does.',
            content: (
              <div className="overflow-x-auto rounded-lg border border-[var(--color-line)] bg-white p-4">
                <Image
                  src="/hardware/oapogee-schematic.svg"
                  alt="oApogee schematic: USB-C into a charge controller and a single lithium cell feeding a buck-boost 3V3 rail, an RP2350 with QSPI flash, a BMP390 barometer on I2C, an ICM-42688-P IMU and an ADXL375 high-g accelerometer on SPI, an SX1262 radio, a MAX-M10S GNSS receiver, and a buzzer and RGB LED."
                  width={1600}
                  height={1100}
                  className="min-w-[900px] max-w-none"
                />
              </div>
            ),
          },
          {
            id: 'netlist',
            label: 'Netlist',
            hint: 'The same information as text. This is the form to review a change in, because it diffs.',
            content: netlist ? (
              <pre className="max-h-[70vh] overflow-auto rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5 font-mono text-xs leading-relaxed text-[var(--color-muted)]">
                {netlist}
              </pre>
            ) : (
              <p className="text-[var(--color-muted)]">
                Not built. Run <code>make hw</code>.
              </p>
            ),
          },
          {
            id: 'files',
            label: 'Files',
            hint: 'All four artifacts, rendered from hardware/oapogee.tsx by make hw and verified against it in the build.',
            content: (
              <div className="flex max-w-2xl flex-col gap-3">
                {DOWNLOADS.map((d) => (
                  <a
                    key={d.file}
                    href={`/hardware/${d.file}`}
                    download
                    className="rounded-lg border border-[var(--color-line)] p-4 !no-underline hover:border-[var(--color-line-bright)]"
                  >
                    <div className="flex flex-wrap items-baseline gap-2">
                      <span className="font-medium !text-white">{d.label}</span>
                      <code className="font-mono text-xs text-[var(--color-hivis)]">{d.file}</code>
                      <span className="ml-auto font-mono text-xs text-[var(--color-dim)]">
                        {sizeOf(d.file) ?? 'not built'}
                      </span>
                    </div>
                    <p className="mt-1 text-sm text-[var(--color-muted)]">{d.what}</p>
                  </a>
                ))}
              </div>
            ),
          },
        ]}
      />

      <section className="max-w-[46rem]">
        <h2 className="text-xl font-semibold text-white">Why there is no PCB here</h2>
        <p className="mt-3 text-[var(--color-muted)]">
          A layout needs real footprints, real footprints need the datasheet pin mapping this design
          has not done, and an autorouted board built on placeholder pins would look far more
          finished than it is. That is the one thing this project must not ship, so the PCB and 3D
          outputs are deliberately not produced rather than produced with a caveat nobody reads.
        </p>
        <h2 className="mt-8 text-xl font-semibold text-white">How to change it</h2>
        <p className="mt-3 text-[var(--color-muted)]">
          Edit <code>hardware/oapogee.tsx</code> and run <code>make hw</code>. The build fails if the
          committed drawing stops matching the source, so the schematic cannot drift from the design
          the way a hand-drawn one does. See{' '}
          <a href="https://github.com/Xaxis/oapogee.space/blob/main/hardware/README.md">
            hardware/README.md
          </a>
          .
        </p>
      </section>
    </div>
  )
}

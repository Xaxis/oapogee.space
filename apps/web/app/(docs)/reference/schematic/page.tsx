import type { Metadata } from 'next'
import { SchematicViewer } from '@/components/SchematicViewer'
import { readSvg } from '@/lib/svg'
import { readFileSync, statSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { REPO_ROOT } from '@/lib/repo'
import { DocTabs } from '@/components/DocTabs'
import { getPcbStatus } from '@/lib/data'

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
    file: 'oapogee-pcb.svg',
    label: 'PCB layout',
    what: 'Footprints and placement on the 22 by 60 mm board.',
  },
  {
    file: 'oapogee-assembly.svg',
    label: 'Assembly drawing',
    what: 'Where each part goes, for hand assembly.',
  },
  {
    file: 'oapogee-gerbers.zip',
    label: 'Gerbers and drill',
    what: 'What a fab needs: both copper layers, mask, paste, silkscreen, edge cuts, drill files, bill of materials and pick and place.',
  },
  {
    file: 'oapogee.kicad_pcb',
    label: 'KiCad PCB',
    what: 'The layout, for anyone who wants to take it further in KiCad.',
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
  const drawing = readSvg('hardware/oapogee-schematic.svg')
  const pcb = readSvg('hardware/oapogee-pcb.svg')
  const assembly = readSvg('hardware/oapogee-assembly.svg')
  const status = getPcbStatus()
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
            content: drawing ? (
                <SchematicViewer
                  svg={drawing.svg}
                  naturalWidth={drawing.width}
                  naturalHeight={drawing.height}
                  label="oApogee schematic"
                  description="USB-C into a charge controller and a single lithium cell feeding a buck-boost 3V3 rail, an RP2350 with QSPI flash, a BMP390 barometer on I2C, an ICM-42688-P IMU and an ADXL375 high-g accelerometer on SPI, an SX1262 radio, a MAX-M10S GNSS receiver, and a buzzer and RGB LED."
                />
              ) : (
                <p className="text-[var(--color-muted)]">
                  Not built. Run <code>make hw</code>.
                </p>
              ),
          },
          {
            id: 'pcb',
            label: 'PCB',
            hint: 'Every part on the board it will be fabricated as. USB and the power chain at one end, the microcontroller central, the radio and its antenna at the far end away from the digital section.',
            content: pcb ? (
              <SchematicViewer
                svg={pcb.svg}
                naturalWidth={pcb.width}
                naturalHeight={pcb.height}
                label="oApogee PCB layout"
                description="Component placement and footprints on the 22 by 60 mm board. No copper is routed yet: the fabrication blockers below say why."
              />
            ) : (
              <p className="text-[var(--color-muted)]">
                Not built. Run <code>make hw</code>.
              </p>
            ),
          },
          {
            id: 'assembly',
            label: 'Assembly',
            hint: 'The view to build from: outlines and designators, without the copper.',
            content: assembly ? (
              <SchematicViewer
                svg={assembly.svg}
                naturalWidth={assembly.width}
                naturalHeight={assembly.height}
                label="oApogee assembly drawing"
                description="Part outlines and reference designators, for placing components by hand."
              />
            ) : (
              <p className="text-[var(--color-muted)]">
                Not built. Run <code>make hw</code>.
              </p>
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
        <div className="flex flex-wrap items-center gap-3">
          <h2 className="text-xl font-semibold text-white">Can this be fabricated</h2>
          <span className={`chip ${status.fab_ready ? 'chip-verified' : 'chip-blocked'}`}>
            {status.fab_ready ? 'yes' : 'not yet'}
          </span>
        </div>
        <p className="mt-3 text-[var(--color-muted)]">
          {status.components} components, {status.nets} nets, {status.routed_traces} routed traces
          and {status.vias} vias. Everything below is generated by{' '}
          <code>tools/check-pcb.mjs</code> on every build, because a hand-written paragraph about
          the state of a board goes stale the day somebody fixes something.
        </p>

        {status.blockers.length > 0 && (
          <>
            <h3 className="mt-6 font-mono text-xs uppercase tracking-widest text-[var(--color-orange)]">
              Fabrication blockers
            </h3>
            <p className="mt-1 text-sm text-[var(--color-muted)]">
              Gerbers are not published while any of these are open. They are the artifact somebody
              spends money on.
            </p>
            <ul className="mt-3">
              {status.blockers.map((b) => (
                <li
                  key={b.id}
                  className="border-t border-[var(--color-line)] py-3 first:border-t-0 text-sm text-[var(--color-muted)]"
                >
                  <code className="text-xs text-[var(--color-orange)]">{b.id}</code>{' '}
                  {b.what}
                </li>
              ))}
            </ul>
          </>
        )}

        <h3 className="mt-6 font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
          Advisory
        </h3>
        <ul className="mt-2 flex flex-col gap-1 text-sm text-[var(--color-muted)]">
          {status.advisories.map((a) => (
            <li key={a}>{a}</li>
          ))}
        </ul>

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

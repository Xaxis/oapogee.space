import { getMechanical, getPartSizes, type MechProvenance } from '@/lib/data'
import { Marked } from '@/components/Marked'

/**
 * The printed parts, and every dimension they were built from.
 *
 * The provenance table is the point of this component. No oApogee board has
 * been fabricated, so the most important input to both parts, the board
 * outline, is a guess. A guess buried in geometry looks exactly like a
 * measurement buried in geometry, and somebody downloading an STL cannot tell
 * them apart. So the dimensions are published beside the models with where each
 * one came from, and the provisional ones say what evidence would retire them.
 */

const SOURCE_BASE = 'https://github.com/Xaxis/oapogee.space/blob/main/hardware/mechanical'
const FILE_BASE = '/hardware/mechanical'

const BADGE: Record<MechProvenance, string> = {
  standard: 'chip-verified',
  derived: 'chip',
  practice: 'chip',
  provisional: 'chip-blocked',
}

export function PrintedParts() {
  const mech = getMechanical()
  const sizes = getPartSizes()
  const provisional = mech.params.filter((p) => p.provenance === 'provisional')

  return (
    <div className="mt-16 space-y-12">
      <section>
        <h2 className="text-2xl font-semibold text-white">Printed parts</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          Parametric source, not just exported STLs, in a format that opens without a commercial
          licence. Change a dimension in <code>data/mechanical.yaml</code>, run{' '}
          <code>make mech</code>, and both the models and this page follow.
        </p>

        <div className="mt-5 rounded-lg border-l-3 border-[var(--color-orange)] bg-[color-mix(in_srgb,var(--color-orange)_6%,transparent)] p-5">
          <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-orange)]">
            Print these to check fit, not to fly
          </div>
          <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
            {provisional.length} of the {mech.params.length} dimensions below are provisional,
            including the board outline, because no board has been fabricated to measure. The
            geometry is real and the numbers it is built from are not yet. A part printed today is a
            fit check.
          </p>
        </div>

        <div className="mt-8 grid gap-6 lg:grid-cols-3">
          {mech.parts.map((part) => {
            const stem = `oapogee-${part.id.replace(/_/g, '-')}`
            const size = sizes[part.id]
            return (
              <div
                key={part.id}
                className="flex flex-col rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5"
              >
                {/* eslint-disable-next-line @next/next/no-img-element */}
                <img
                  src={`${FILE_BASE}/${stem}.png`}
                  alt={`Rendered view of the oApogee ${part.name.toLowerCase()}`}
                  width={1100}
                  height={750}
                  className="w-full rounded border border-[var(--color-line)]"
                />
                <h3 className="mt-4 font-semibold text-white">{part.name}</h3>
                <p className="mt-2 flex-1 text-sm text-[var(--color-muted)]">{part.note}</p>
                {/* The first question anybody printing a part asks is whether
                    it fits on the bed. Measured off the STL, not typed. */}
                {size ? (
                  <p className="mt-3 text-sm text-[var(--color-muted)]">
                    <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                      Overall:{' '}
                    </span>
                    <span className="font-mono">
                      {size.x_mm} &times; {size.y_mm} &times; {size.z_mm} mm
                    </span>
                  </p>
                ) : null}
                <p className="mt-3 text-sm text-[var(--color-muted)]">
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                    Printing:{' '}
                  </span>
                  {part.prints_with}
                </p>
                <div className="mt-4 flex flex-wrap gap-x-4 gap-y-1 border-t border-[var(--color-line)] pt-3 text-sm">
                  <a href={`${FILE_BASE}/${stem}.stl`} download>
                    Download STL
                  </a>
                  <a href={`${SOURCE_BASE}/${part.source}`}>{part.source}</a>
                </div>
              </div>
            )
          })}
        </div>

        <p className="mt-6 max-w-2xl text-sm text-[var(--color-muted)]">
          The pod is parametric on body tube diameter, so an unusual airframe is one number rather
          than a request for a new file:{' '}
          <code>openscad -D tube_od=41.6 -o base.stl pod-base.scad</code>. Measure your own tube.
          Paper stock varies with humidity and between production runs, and the saddle is where that
          variation shows up as a rock.
        </p>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Every dimension, and where it came from</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          Generated from the same file the models are. A dimension marked provisional is not
          measured and not sourced, and says what would close it.
        </p>

        <div className="mt-6 flex flex-col gap-8">
          {mech.groups.map((group) => {
            const params = mech.params.filter((p) => p.group === group.id)
            if (!params.length) return null
            return (
              <div key={group.id}>
                <h3 className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
                  {group.name}
                </h3>
                <p className="mt-1 max-w-2xl text-sm text-[var(--color-muted)]">{group.note}</p>
                <ul className="mt-3">
                  {params.map((p) => (
                    <li
                      key={p.id}
                      className="border-t border-[var(--color-line)] py-3 first:border-t-0"
                    >
                      <div className="flex flex-wrap items-baseline gap-x-3 gap-y-1">
                        <span className="font-mono text-sm text-[var(--color-hivis)]">
                          {p.value}
                          {p.unit && p.unit !== 'count' ? ` ${p.unit}` : ''}
                        </span>
                        <span className="text-white">{p.what}</span>
                        <span className={`chip ${BADGE[p.provenance]}`}>{p.provenance}</span>
                        <code className="text-xs text-[var(--color-dim)]">{p.id}</code>
                      </div>
                      {(p.source ?? p.closes) && (
                        <Marked
                          text={(p.source ?? p.closes) as string}
                          className="mt-1 max-w-3xl text-sm text-[var(--color-muted)]"
                        />
                      )}
                    </li>
                  ))}
                </ul>
              </div>
            )
          })}
        </div>
      </section>
    </div>
  )
}

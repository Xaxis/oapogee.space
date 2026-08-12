import type { Metadata } from 'next'
import Link from 'next/link'
import { SchematicViewer } from '@/components/SchematicViewer'
import { readSvg } from '@/lib/svg'
import { getBom, getTiers, type Part } from '@/lib/data'
import { DocTabs } from '@/components/DocTabs'

export const metadata: Metadata = {
  title: 'Bill of materials',
  description:
    'Every part in an oApogee build, by tier and by build path, with what is confirmed and what is not.',
}

function money(v: number | null) {
  return v === null ? <span className="text-[var(--color-dim)]">not priced</span> : `$${v.toFixed(2)}`
}

function PartRow({ part, tierIds, pathId }: { part: Part; tierIds: string[]; pathId: string }) {
  return (
    <tr>
      <td>
        <div className="font-medium text-white">
          {part.name}
          {part.optional && (
            <span className="ml-2 font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
              optional
            </span>
          )}
        </div>
        {part.mpn ? (
          <div className="mt-1 font-mono text-xs text-[var(--color-muted)]">
            {part.manufacturer} {part.mpn}
            {part.confidence === 'unverified' && (
              <span className="ml-2 text-[var(--color-orange)]">unconfirmed</span>
            )}
          </div>
        ) : (
          <div className="mt-1 font-mono text-xs text-[var(--color-orange)]">part not yet chosen</div>
        )}
      </td>
      {tierIds.map((tier) => {
        const applies = part.applies?.find((a) => a.tier === tier && a.path === pathId)
        return (
          <td key={tier} className="text-center">
            {applies ? (
              <span className="text-[var(--color-hivis)]">{applies.qty}</span>
            ) : (
              <span className="text-[var(--color-dim)]">.</span>
            )}
          </td>
        )
      })}
      <td className="whitespace-nowrap">{money(part.price_usd)}</td>
    </tr>
  )
}

export default function Bom() {
  const bom = getBom()
  const { tiers } = getTiers()
  const tierIds = tiers.map((t) => t.id)
  const system = readSvg('schematic/system.svg')

  return (
    <div className="space-y-16">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">
          Bill of materials
        </h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Every part in an oApogee build, by tier and by build path.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">{bom.status}</span>
          <span className="chip">updated {bom.updated}</span>
        </div>
      </header>

      <section className="rounded-lg border-l-3 border-[var(--color-hivis-dim)] bg-[color-mix(in_srgb,var(--color-hivis)_6%,transparent)] p-6">
        <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
          Why every price says &ldquo;not priced&rdquo;
        </div>
        <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
          Nothing here has been bought, so there is no honest number to put in the column.
          Manufacturer part numbers shown plainly are asserted with confidence; anything marked{' '}
          <span className="text-[var(--color-orange)]">unconfirmed</span> is a candidate that still
          needs checking against current availability.
        </p>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">How it goes together</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          The same parts, wired. The ticks on each block show which tiers populate it.
        </p>
        {system && (
          <SchematicViewer
            className="mt-6"
            svg={system.svg}
            naturalWidth={system.width}
            naturalHeight={system.height}
            label="oApogee system block diagram"
            description="Every part in this bill of materials and how they connect. The ticks on each block show which tiers populate it."
          />
        )}
      </section>

      <DocTabs
        label="Build path"
        tabs={bom.paths.map((path) => ({
          id: path.id,
          label: `${path.name} path`,
          hint: path.summary,
          content: (
            <div>
              <div className="flex flex-wrap items-center gap-3">
                <span className={`chip ${path.available ? 'chip-verified' : 'chip-blocked'}`}>
                  {path.available ? 'buildable now' : 'not yet fabricated'}
                </span>
                <span className="text-sm text-[var(--color-muted)]">
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                    Tradeoff:{' '}
                  </span>
                  {path.tradeoff}
                </span>
              </div>

              <div className="table-scroll mt-6">
                <table className="data">
                  <thead>
                    <tr>
                      <th>Part</th>
                      {tiers.map((t) => (
                        <th key={t.id} className="text-center">
                          {t.name.replace('oApogee ', '')}
                        </th>
                      ))}
                      <th>Price</th>
                    </tr>
                  </thead>
                  <tbody>
                    {bom.parts
                      .filter((p) => p.applies?.some((a) => a.path === path.id))
                      .map((part) => (
                        <PartRow key={part.id} part={part} tierIds={tierIds} pathId={path.id} />
                      ))}
                  </tbody>
                </table>
              </div>
            </div>
          ),
        }))}
      />

      <section>
        <h2 className="text-2xl font-semibold text-white">Why these parts</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          The reasoning behind each choice, including the ones that look like mistakes until you
          know why.
        </p>
        <div className="mt-6 flex flex-col gap-6">
          {bom.parts
            .filter((p) => p.why)
            .map((part) => (
              <div
                key={part.id}
                className="rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-5"
              >
                <h3 className="font-semibold text-white">{part.name}</h3>
                <p className="mt-2 max-w-3xl text-sm text-[var(--color-muted)]">{part.why}</p>
                {part.gotcha && (
                  <p className="mt-3 max-w-3xl border-l-2 border-[var(--color-orange)] pl-3 text-sm text-[var(--color-muted)]">
                    <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-orange)]">
                      Gotcha:{' '}
                    </span>
                    {part.gotcha}
                  </p>
                )}
                {part.region_note && (
                  <p className="mt-3 max-w-3xl text-sm text-[var(--color-muted)]">
                    <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                      Region:{' '}
                    </span>
                    {part.region_note}
                  </p>
                )}
                {part.substitutes?.length > 0 && (
                  <div className="mt-4 border-t border-[var(--color-line)] pt-3">
                    <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                      Substitutes
                    </div>
                    <ul className="mt-2 flex flex-col gap-2 text-sm text-[var(--color-muted)]">
                      {part.substitutes.map((s) => (
                        <li key={s.mpn}>
                          <span className="font-mono text-[var(--color-body)]">{s.mpn}</span>{' '}
                          {s.note}
                        </li>
                      ))}
                    </ul>
                  </div>
                )}
              </div>
            ))}
        </div>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Ground station</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">{bom.ground_station.summary}</p>
        <p className="mt-3 max-w-2xl border-l-2 border-[var(--color-orange)] pl-4 text-sm text-[var(--color-muted)]">
          This is a second build with its own bill of materials. oApogee Link and oApogee Track are
          not usable without one, so any price you see quoted for those tiers is incomplete until
          this is added to it.
        </p>
        <div className="table-scroll mt-6 max-w-2xl">
          <table className="data">
            <thead>
              <tr>
                <th>Part</th>
                <th className="text-center">Qty</th>
              </tr>
            </thead>
            <tbody>
              {bom.ground_station.parts.map((p) => (
                <tr key={p.id}>
                  <td className="text-white">{p.name}</td>
                  <td className="text-center text-[var(--color-hivis)]">{p.qty}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Tools</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          What you need that is not a part. Ordered by whether you can start without it, because a
          long undifferentiated tool list is where a beginner decides the project is not for them.
        </p>

        {/* Grouped and ranked, one card style throughout. Three parallel
            columns of bullets made the groups look equally important and forced
            a comparison across columns to answer the only question a reader
            has, which is what they need before they can start. Repeating a
            "required" pill on every card in the required group answered that
            question nine times and added nothing, so the group heading carries
            it once.

            items-start so a card with no note hugs its content. Stretched to
            the row height they became tall empty boxes that read as missing
            information. */}
        {(
          [
            [
              'required',
              'Required',
              'You cannot finish the build without these.',
              bom.tools.required,
            ],
            [
              'recommended',
              'Recommended',
              'Not strictly needed, and each one removes a specific frustration.',
              bom.tools.recommended,
            ],
            [
              'fabrication',
              'For the printed parts',
              'The sled and the pod are printed, and there is no non-printed option.',
              bom.tools.fabrication,
            ],
          ] as const
        ).map(([kind, label, blurb, list]) => (
          <div key={kind} className="mt-8">
            <h3 className="text-lg font-semibold text-white">{label}</h3>
            <p className="mt-1 text-sm text-[var(--color-muted)]">{blurb}</p>
            <div className="mt-4 grid items-start gap-3 sm:grid-cols-2 lg:grid-cols-3">
              {list.map((tool) => (
                <div
                  key={tool.name}
                  className={`rounded-lg border bg-[var(--color-surface)] p-4 ${
                    kind === 'required'
                      ? 'border-[var(--color-line-bright)] border-l-3 border-l-[var(--color-hivis)]'
                      : 'border-[var(--color-line)]'
                  }`}
                >
                  <div className="font-medium text-white">{tool.name}</div>
                  {tool.note && (
                    <p className="mt-2 text-sm text-[var(--color-muted)]">{tool.note}</p>
                  )}
                </div>
              ))}
            </div>
          </div>
        ))}

        <p className="mt-4 max-w-2xl text-sm text-[var(--color-dim)]">
          Nothing here is priced, for the same reason nothing else on this page is. Most of it you
          either own already or will keep using long after this build.
        </p>
      </section>

      <p className="text-sm text-[var(--color-muted)]">
        Every open question behind this page is listed on <Link href="/status">the status page</Link>
        .
      </p>
    </div>
  )
}

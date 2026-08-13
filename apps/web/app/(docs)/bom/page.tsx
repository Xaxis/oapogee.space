import type { Metadata } from 'next'
import Link from 'next/link'
import { BomList } from '@/components/BomList'
import { getBom, getSuppliers, getTiers } from '@/lib/data'

export const metadata: Metadata = {
  title: 'Bill of materials',
  description:
    'Pick a tier and a build path, get the parts list with links to buy them, and see what is confirmed and what is not.',
}

export default function Bom() {
  const bom = getBom()
  const { tiers } = getTiers()
  const suppliers = getSuppliers()

  return (
    <div className="space-y-14">
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">
          Bill of materials
        </h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">
          Pick a tier and a build path. The list below is what you buy.
        </p>
        <div className="mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">{bom.status}</span>
          <span className="chip">updated {bom.updated}</span>
        </div>
      </header>

      <BomList
        bom={bom}
        tiers={tiers}
        suppliers={[...suppliers.distributors, ...suppliers.makers]}
      />

      <section className="rounded-lg border-l-3 border-[var(--color-hivis-dim)] bg-[color-mix(in_srgb,var(--color-hivis)_6%,transparent)] p-6">
        <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
          There are no prices here
        </div>
        <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
          Nothing has been bought, so there is no honest number to print. Supplier links are searches
          by part number rather than product pages, because a search does not go out of stock; where
          a specific product is linked, somebody opened the page and the date is next to it. Neither
          is a claim that a part is in stock today.
        </p>
        <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
          See <Link href="/schematic">the schematic</Link> for how these parts connect, and{' '}
          <Link href="/status">the status page</Link> for every open question behind this one.
        </p>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Tools</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          What you need that is not a part, ordered by whether you can start without it.
        </p>

        {(
          [
            ['required', 'Required', 'You cannot finish the build without these.', bom.tools.required],
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
                  {tool.note && <p className="mt-2 text-sm text-[var(--color-muted)]">{tool.note}</p>}
                </div>
              ))}
            </div>
          </div>
        ))}
      </section>
    </div>
  )
}

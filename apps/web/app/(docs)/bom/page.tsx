import type { Metadata } from 'next'
import Link from 'next/link'
import { BomList } from '@/components/BomList'
import { getBom, getSuppliers, getTiers } from '@/lib/data'
import { Marked } from '@/components/Marked'

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
          See <Link href="/reference/schematic">the schematic</Link> for how these parts connect,
          and{' '}
          <Link href="/status">the status page</Link> for every open question behind this one.
        </p>
      </section>

      <section>
        <h2 className="text-2xl font-semibold text-white">Tools</h2>
        <p className="mt-2 max-w-2xl text-[var(--color-muted)]">
          What you need that is not a part, ordered by whether you can start without it.
        </p>

        {/* One row per tool, in the same idiom as the parts list above. These
            were cards in a three-column grid, and eight of the sixteen tools
            have no note, so half the grid was empty boxes of varying height
            with a word in the corner. A tool list answers one question, which
            is whether you can start tonight, and a dense list answers it in a
            glance where a ragged wall of cards does not. */}
        <div className="mt-8 flex flex-col gap-8">
          {(
            [
              ['Before you can start', 'Without these you cannot finish the build.', bom.tools.required],
              [
                'Makes it easier',
                'Each one removes a specific frustration rather than a step.',
                bom.tools.recommended,
              ],
              [
                'For the printed parts',
                'Both form factors are printed, and there is no non-printed option.',
                bom.tools.fabrication,
              ],
            ] as const
          ).map(([label, blurb, list]) => (
            <div key={label}>
              <h3 className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
                {label}
              </h3>
              <p className="mt-1 max-w-2xl text-sm text-[var(--color-muted)]">{blurb}</p>
              <ul className="mt-3">
                {list.map((tool) => (
                  <li
                    key={tool.name}
                    className="border-t border-[var(--color-line)] py-3 first:border-t-0"
                  >
                    <div className="text-white">{tool.name}</div>
                    {tool.note && (
                      <Marked
                        text={tool.note}
                        className="mt-1 max-w-3xl text-sm text-[var(--color-muted)]"
                      />
                    )}
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>
      </section>
    </div>
  )
}

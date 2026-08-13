'use client'

import { useState } from 'react'
import type { Bom, Part, Supplier, Tier } from '@/lib/data'
import { Marked } from '@/components/Marked'

/**
 * The shopping list.
 *
 * The page this replaced put quantities in a table and buying information in a
 * separate stack of cards further down, so ordering one part meant scrolling
 * between two places and matching names by eye. It also carried a price column
 * in which all twenty-six cells read "not priced", which is a column of noise
 * wearing the costume of data.
 *
 * A reader arrives here having already chosen a tier and a build path. That
 * choice is the only control on the page, and everything else follows from it:
 * one row per part you actually need, with the link that actually buys it.
 * Reasoning, substitutes and gotchas are one disclosure away, because they
 * matter when you are deciding and are in the way when you are ordering.
 */

// Roles as they appear in data/bom.yaml, gathered into the groups somebody
// shops in. Anything not listed still renders, under Other, so adding a role to
// the data can never silently drop a part from the list.
const GROUPS: { label: string; roles: string[] }[] = [
  { label: 'Brains', roles: ['mcu', 'storage'] },
  { label: 'Sensors', roles: ['barometer', 'imu', 'high_g_accelerometer', 'gnss'] },
  { label: 'Radio', roles: ['radio', 'antenna'] },
  { label: 'Power', roles: ['battery', 'power', 'arming'] },
  { label: 'Connectors and indicators', roles: ['connector', 'indicator', 'recovery'] },
  { label: 'Passives', roles: ['passive'] },
]

const search = (sup: Supplier, mpn: string) => sup.search.replace('{mpn}', encodeURIComponent(mpn))

function Row({
  part,
  qty,
  path,
  suppliers,
}: {
  part: Part
  qty: number
  path: string
  suppliers: Supplier[]
}) {
  // Which link actually buys this part depends on the path you are on. On the
  // Modules path you want the assembled breakout, and a distributor search for
  // a bare chip is the wrong answer. On the Board path the chip is the answer.
  // Guarded on the url, because a breakout entry can be an open question with a
  // verify note and no product yet; rendering that gave a supplier name
  // attached to nothing and a "page read undefined".
  const breakout = path === 'modules' && part.breakout?.url ? part.breakout : undefined
  // A substitute can be the part to buy rather than a fallback. The IMU is the
  // case: no mainstream ICM-42688-P breakout exists, the only assembled board
  // is a Click that plugs into nothing here, and the data says in terms that it
  // is "not what a first build should be sent after". Linking the breakout
  // anyway sent a beginner to the wrong board and buried the right one inside a
  // collapsed disclosure.
  const pick = part.substitutes?.find((sub) => sub.recommended_for === path && sub.url)
  const primary = suppliers[0]
  const rest = suppliers.slice(1)
  // A passive's value is its specification. Any 100k 0402 is the 100k 0402, so
  // flagging it as "not chosen yet" alongside genuinely undecided parts reads
  // as eight open decisions where there are three.
  const generic = part.role === 'passive'
  const unconfirmed = [part.verify, part.breakout?.verify].filter(Boolean) as string[]
  const hasDetail =
    part.why ||
    part.gotcha ||
    part.availability ||
    part.region_note ||
    part.substitutes?.length ||
    unconfirmed.length

  return (
    <li className="border-t border-[var(--color-line)] py-4 first:border-t-0">
      <div className="flex flex-wrap items-baseline gap-x-3 gap-y-1">
        <span className="font-mono text-sm text-[var(--color-hivis)]">{qty}&times;</span>
        <span className="font-medium text-white">{part.name}</span>
        {part.optional && (
          <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
            optional
          </span>
        )}
      </div>

      <div className="mt-1 flex flex-wrap items-baseline gap-x-3 gap-y-1 font-mono text-xs">
        {part.designators?.length ? (
          <span className="text-[var(--color-dim)]">{part.designators.join(' ')}</span>
        ) : null}
        {part.mpn ? (
          <span className="text-[var(--color-muted)]">
            {part.manufacturer} {part.mpn}
            {part.confidence === 'unverified' && (
              <span className="ml-2 text-[var(--color-orange)]">unconfirmed</span>
            )}
          </span>
        ) : generic ? (
          <span className="text-[var(--color-dim)]">any manufacturer</span>
        ) : (
          <span className="text-[var(--color-orange)]">part not chosen yet</span>
        )}
      </div>

      <div className="mt-2 flex flex-wrap items-center gap-x-4 gap-y-1 text-sm">
        {pick ? (
          <>
            <a href={pick.url}>Buy {pick.mpn} instead</a>
            <span className="text-[var(--color-dim)]">
              the recommended part on this path, not {part.mpn}
            </span>
          </>
        ) : breakout ? (
          <>
            <a href={breakout.url}>
              {breakout.product} &middot; {breakout.supplier}
            </a>
            <span className="text-[var(--color-dim)]">page read {breakout.checked}</span>
          </>
        ) : path === 'modules' && part.mpn ? (
          <>
            <span className="text-[var(--color-orange)]">No assembled board identified yet</span>
            <a href={search(primary, part.mpn)}>Search {primary.name} for the bare chip</a>
          </>
        ) : part.mpn ? (
          <a href={search(primary, part.mpn)}>Search {primary.name}</a>
        ) : null}

        {hasDetail && (
          <details className="w-full">
            <summary className="cursor-pointer text-sm text-[var(--color-dim)] hover:text-[var(--color-body)]">
              Why this part
            </summary>
            <div className="mt-3 flex flex-col gap-3 border-l-2 border-[var(--color-line-bright)] pl-4 text-sm text-[var(--color-muted)]">
              {part.why && <Marked text={part.why} />}
              {part.gotcha && (
                <p>
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-orange)]">
                    Gotcha:{' '}
                  </span>
                  <Marked text={part.gotcha} className="inline" />
                </p>
              )}
              {part.availability && (
                <p>
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-orange)]">
                    Availability:{' '}
                  </span>
                  <Marked text={part.availability} className="inline" />
                </p>
              )}
              {part.region_note && (
                <p>
                  <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                    Region:{' '}
                  </span>
                  {part.region_note}
                </p>
              )}
              {part.substitutes?.length > 0 && (
                <div>
                  <div className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                    Substitutes
                  </div>
                  <ul className="mt-1 flex flex-col gap-1">
                    {part.substitutes.map((s) => (
                      <li key={s.mpn}>
                        {s.url ? (
                          <a href={s.url} className="font-mono">
                            {s.mpn}
                          </a>
                        ) : (
                          <span className="font-mono text-[var(--color-body)]">{s.mpn}</span>
                        )}{' '}
                        <Marked text={s.note} className="inline" />
                      </li>
                    ))}
                  </ul>
                </div>
              )}
              {unconfirmed.map((note, i) => (
                <Marked key={i} text={note} />
              ))}
              {part.mpn && (
                <p className="flex flex-wrap items-center gap-x-3 gap-y-1">
                  <span className="text-[var(--color-dim)]">Also search:</span>
                  {rest.map((sup) => (
                    <a key={sup.id} href={search(sup, part.mpn as string)}>
                      {sup.name}
                    </a>
                  ))}
                </p>
              )}
            </div>
          </details>
        )}
      </div>
    </li>
  )
}

function Choice<T extends { id: string }>({
  label,
  options,
  value,
  onChange,
  render,
}: {
  label: string
  options: T[]
  value: string
  onChange: (id: string) => void
  render: (o: T) => string
}) {
  return (
    <div>
      <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
        {label}
      </div>
      <div
        role="group"
        aria-label={label}
        className="mt-2 inline-flex flex-wrap gap-1 rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-1"
      >
        {options.map((o) => (
          <button
            key={o.id}
            type="button"
            aria-pressed={value === o.id}
            onClick={() => onChange(o.id)}
            className={`rounded-md px-3 py-1.5 text-sm transition-colors ${
              value === o.id
                ? 'bg-[var(--color-hivis)] font-medium text-black'
                : 'text-[var(--color-muted)] hover:text-white'
            }`}
          >
            {render(o)}
          </button>
        ))}
      </div>
    </div>
  )
}

export function BomList({
  bom,
  tiers,
  suppliers,
}: {
  bom: Bom
  tiers: Tier[]
  suppliers: Supplier[]
}) {
  // Track and Modules: the most capable tier on the only path that can be built
  // today, which is what most people arriving here are costing out.
  const [tier, setTier] = useState('track')
  const [path, setPath] = useState('modules')

  const chosenPath = bom.paths.find((p) => p.id === path)
  const rows = bom.parts
    .map((part) => ({ part, qty: part.applies?.find((a) => a.tier === tier && a.path === path)?.qty }))
    .filter((r): r is { part: Part; qty: number } => typeof r.qty === 'number')

  const grouped = GROUPS.map((g) => ({
    label: g.label,
    rows: rows.filter((r) => g.roles.includes(r.part.role)),
  })).filter((g) => g.rows.length)
  const claimed = new Set(GROUPS.flatMap((g) => g.roles))
  const other = rows.filter((r) => !claimed.has(r.part.role))
  if (other.length) grouped.push({ label: 'Other', rows: other })

  const lines = rows.length
  const units = rows.reduce((n, r) => n + r.qty, 0)
  // "Buildable now" sitting next to a column of "part not chosen yet" reads as
  // a contradiction, and a reader is right to distrust it. It means the
  // approach needs no PCB fabrication, not that the shopping list is finished.
  // Saying how many are open turns the apparent lie back into a fact.
  const undecided = rows.filter((r) => !r.part.mpn && r.part.role !== 'passive').length
  const tierName = (id: string) => (tiers.find((t) => t.id === id)?.name ?? id).replace('oApogee ', '')

  return (
    <div>
      <div className="flex flex-wrap gap-8">
        <Choice
          label="Tier"
          options={tiers}
          value={tier}
          onChange={setTier}
          render={(t) => t.name.replace('oApogee ', '')}
        />
        <Choice
          label="Build path"
          options={bom.paths}
          value={path}
          onChange={setPath}
          render={(p) => p.name}
        />
      </div>

      {chosenPath && (
        <p className="mt-5 max-w-2xl text-[var(--color-muted)]">
          <span
            className={`chip ${chosenPath.available ? 'chip-verified' : 'chip-blocked'} mr-2 align-middle`}
          >
            {chosenPath.available ? 'buildable now' : 'not yet fabricated'}
          </span>
          {chosenPath.tradeoff}
        </p>
      )}

      <p className="mt-4 text-sm text-[var(--color-dim)]">
        {lines} line {lines === 1 ? 'item' : 'items'}, {units} {units === 1 ? 'piece' : 'pieces'} for{' '}
        {tierName(tier)} on the {chosenPath?.name} path.
        {undecided > 0 && (
          <>
            {' '}
            <span className="text-[var(--color-orange)]">
              {undecided} still {undecided === 1 ? 'has' : 'have'} no specific product chosen.
            </span>{' '}
            Buildable means the approach needs no board fabricated, not that this list is finished.
          </>
        )}
      </p>

      <div className="mt-8 flex flex-col gap-8">
        {grouped.map((g) => (
          <section key={g.label}>
            <h3 className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
              {g.label}
            </h3>
            <ul className="mt-2">
              {g.rows.map((r) => (
                <Row key={r.part.id} part={r.part} qty={r.qty} path={path} suppliers={suppliers} />
              ))}
            </ul>
          </section>
        ))}
      </div>

      {/* Only shown when the tier needs one. Solo logs to flash and is complete
          on its own; telling a Solo builder to buy a receiver is how a parts
          list talks somebody out of the build they were about to do. */}
      {tier !== 'solo' && (
        <section className="mt-12 rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-6">
          <div className="flex flex-wrap items-center gap-3">
            <h3 className="font-semibold text-white">You also need a ground station</h3>
            {bom.ground_station.status && (
              <span className="chip chip-blocked">{bom.ground_station.status}</span>
            )}
          </div>
          <p className="mt-2 max-w-2xl text-sm text-[var(--color-muted)]">
            {bom.ground_station.summary} {tierName(tier)} is not usable without one, so any price
            quoted for this tier is incomplete until you add it.
          </p>
          <ul className="mt-4 flex flex-col gap-2 text-sm">
            {bom.ground_station.parts.map((p) => (
              <li key={p.id}>
                <div className="flex gap-3">
                  <span className="font-mono text-[var(--color-hivis)]">{p.qty}&times;</span>
                  <span className="text-[var(--color-body)]">{p.name}</span>
                </div>
                {/* The parts carry their own open questions. Rendering the list
                    without them made a proposal look like a decided build. */}
                {p.verify && (
                  <Marked
                    text={p.verify}
                    className="ml-7 mt-1 max-w-2xl text-[var(--color-muted)]"
                  />
                )}
              </li>
            ))}
          </ul>
          {bom.ground_station.approval_note && (
            <Marked
              text={bom.ground_station.approval_note}
              className="mt-4 max-w-2xl border-l-2 border-[var(--color-orange)] pl-4 text-sm text-[var(--color-muted)]"
            />
          )}
        </section>
      )}
    </div>
  )
}

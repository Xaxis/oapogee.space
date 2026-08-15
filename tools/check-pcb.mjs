#!/usr/bin/env node
/**
 * Whether this board could be fabricated, and if not, exactly why.
 *
 * WHY THIS DOES NOT TRUST THE EXPORT
 *
 * `tsci export` prints "Exported to oapogee-pcb.svg!" and exits zero when the
 * autorouter has thrown and laid down no copper at all. The file is produced,
 * it is just empty of traces. That happened here twice for two unrelated
 * reasons, and neither was visible in an exit code, a file size, or a render
 * that a person did not look closely at. So this reads the circuit JSON and
 * counts what is actually in it.
 *
 * (Both causes are worth recording. The first was ES2025 Iterator Helpers,
 * which the autorouter uses and the installed Bun does not implement; see
 * hardware/iterator-helpers-polyfill.js. The second was placement errors, which
 * make tscircuit skip routing entirely and say so only inside the JSON.)
 *
 * WHAT COUNTS AS A BLOCKER
 *
 * Fabrication costs money and takes weeks. A blocker is anything that would
 * produce a board somebody could not build or that would not work: unrouted
 * nets, copper closer than a fab will make, parts whose declared pinout does
 * not match the package they are going into. Everything else is an advisory.
 *
 * The board is NOT currently fabricable and this is expected to say so. That is
 * the honest state of a design nobody has ordered. What it must never do is
 * drift quietly, so the counts are ratcheted against hardware/pcb-baseline.json
 * and any increase fails.
 *
 *   node tools/check-pcb.mjs           report, and fail on regression
 *   node tools/check-pcb.mjs --update  record the current state as the baseline
 *   node tools/check-pcb.mjs --fab     exit non-zero unless it is fab-ready
 */

import { readFileSync, writeFileSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const CIRCUIT = join(ROOT, 'apps/web/public/hardware/oapogee-circuit.json')
const BASELINE = join(ROOT, 'hardware/pcb-baseline.json')
const STATUS = join(ROOT, 'hardware/pcb-status.json')

if (!existsSync(CIRCUIT)) {
  console.error('apps/web/public/hardware/oapogee-circuit.json is missing. Run `make hw`.')
  process.exit(1)
}
const circuit = JSON.parse(readFileSync(CIRCUIT, 'utf8'))
const of = (t) => circuit.filter((e) => e.type === t)
const count = (t) => of(t).length

const blockers = []
const advisories = []

// --- did every part survive being built --------------------------------------
//
// The most fundamental thing that can go wrong, and it went wrong silently.
// Renaming three pins on the microcontroller left a stale entry in its
// schematic pin arrangement, tscircuit could not resolve the old label, and the
// whole chip failed to create. The export still printed success and exited
// zero. The board came out with 34 of its 35 parts and without any of the nets
// that touch the microcontroller, and every other check here was happy:
// nothing was unrouted, because the traces that would have been unrouted no
// longer existed.
//
// Source errors are read first for that reason. A netlist missing half its
// connections is not a board with a routing problem, it is not the board.
{
  for (const [type, id, label] of [
    ['source_failed_to_create_component_error', 'component-failed', 'component(s) failed to be created'],
    ['source_trace_not_connected_error', 'selector-unresolved', 'trace selector(s) name a port that does not exist'],
  ]) {
    const n = count(type)
    if (n > 0) {
      blockers.push({
        id,
        n,
        what: `${n} ${label}. First: ${String(of(type)[0]?.message ?? '').slice(0, 170)}`,
      })
    }
  }

  // Every trace written in the source has to reach the netlist. A selector that
  // resolves to nothing removes a connection without removing a line of source,
  // so counting one against the other is the only way to see it.
  const declared = (readFileSync(join(ROOT, 'hardware/oapogee.tsx'), 'utf8').match(/<trace\s/g) ?? []).length
  const reached = count('source_trace')
  if (declared > 0 && reached < declared) {
    blockers.push({
      id: 'traces-lost',
      n: declared - reached,
      what:
        `hardware/oapogee.tsx declares ${declared} traces and only ${reached} reached the netlist. ` +
        `The missing ones did not fail to route, they were never created.`,
    })
  }
}

// --- routing -----------------------------------------------------------------

const routed = count('pcb_trace')
const netlist = count('source_trace')
const unroutable = circuit.filter((e) => /could not find a route/i.test(String(e.message ?? ''))).length

if (netlist > 0 && routed === 0) {
  // The failure this whole file exists for.
  blockers.push({
    id: 'no-copper',
    n: netlist,
    what:
      `the export produced NO copper at all, with ${netlist} nets in the netlist. ` +
      `Either the autorouter threw, or tscircuit skipped it. Look for pcb_autorouting_error ` +
      `in the circuit JSON, which is the only place it says so.`,
  })
} else if (unroutable > 0) {
  blockers.push({ id: 'unroutable', n: unroutable, what: `${unroutable} connection(s) could not be routed` })
}

// Trace count is deliberately not used as a completeness metric. Several source
// traces terminating on one net become a single routed path, so a fully routed
// board reports fewer pcb_traces than source_traces and a ratio would read as a
// failure on a board that is finished.
const missing = count('pcb_trace_missing_error')
if (missing > 0) {
  blockers.push({ id: 'trace-missing', n: missing, what: `${missing} net(s) have no PCB trace` })
}

// --- placement ---------------------------------------------------------------

for (const [type, id, label] of [
  ['pcb_courtyard_overlap_error', 'courtyard-overlap', 'component courtyards overlap'],
  ['pcb_pad_pad_clearance_error', 'pad-clearance', 'pads are closer than the minimum clearance'],
  ['pcb_port_not_connected_error', 'port-unconnected', 'ports are not connected through a net'],
  // Found by a mistake rather than by foresight: a bad edit moved a footprint's
  // thermal pad instead of the component, and tscircuit reported this type,
  // which nothing here was reading. A part hanging off the edge is milled away.
  ['pcb_component_outside_board_error', 'component-off-board', 'components extend outside the board'],
]) {
  const n = count(type)
  if (n > 0) blockers.push({ id, n, what: `${n} ${label}` })
}

// --- pinout completeness -----------------------------------------------------
//
// The one this board actually fails on, and the one a generic PCB checker would
// not look for. A <chip> may declare fewer pins than the package it is placed
// into has pads: the schematic is then a connectivity diagram, which is useful,
// while the layout built from it is a board whose unmapped pads go nowhere.
// Several logical pins can also land on the same pad, which is where the pad
// clearance errors above come from.

{
  const srcByComp = Object.fromEntries(of('source_component').map((e) => [e.source_component_id, e]))
  const declared = {}
  for (const p of of('source_port')) {
    declared[p.source_component_id] = (declared[p.source_component_id] ?? 0) + 1
  }
  const padsByPcb = {}
  for (const t of ['pcb_smtpad', 'pcb_plated_hole']) {
    for (const p of of(t)) {
      if (!p.pcb_component_id) continue
      padsByPcb[p.pcb_component_id] = (padsByPcb[p.pcb_component_id] ?? 0) + 1
    }
  }

  const gaps = []
  for (const pc of of('pcb_component')) {
    const src = srcByComp[pc.source_component_id]
    if (!src) continue
    const pins = declared[pc.source_component_id] ?? 0
    const pads = padsByPcb[pc.pcb_component_id] ?? 0
    if (pads > pins) gaps.push({ name: src.name, pins, pads })
  }
  gaps.sort((a, b) => b.pads - b.pins - (a.pads - a.pins))
  if (gaps.length) {
    blockers.push({
      id: 'pinout-incomplete',
      n: gaps.length,
      what:
        `${gaps.length} part(s) have footprint pads no declared pin maps onto, so those pads ` +
        `connect to nothing and the mapping of the ones that do is ambiguous: ` +
        gaps.map((g) => `${g.name} ${g.pins}/${g.pads}`).join(', ') +
        `. Each one needs its real pinout transcribed from the part's datasheet before a ` +
        `board is ordered.`,
      detail: gaps,
    })
  }
}

// --- copper clearance, measured rather than trusted --------------------------
//
// tscircuit's own design-rule pass is not the last word: on a sibling project it
// reported clean while two traces sat 0.291 mm apart centre to centre. Geometry
// is cheap to measure and this is the number a fab quotes back.

// Two thresholds, both sourced, because one number was either alarmist or
// useless and the single 0.127 this started with was inherited from a sibling
// project without checking it against where this board would actually be made.
//
// PCBWay quotes 4 mil trace and space as their standard two-layer capability.
// JLCPCB quotes 5 mil for standard two-layer at 1 oz copper. So copper below
// 0.1 mm apart is not manufacturable at either on a standard process and is a
// hard failure; between 0.1 and 0.127 the board is orderable from PCBWay and
// not from JLCPCB, which is a real constraint on the reader and worth saying,
// but is not a reason to refuse to publish fabrication files.
const HARD_MM = 0.1 // PCBWay standard two-layer, the finer of the two
const SOFT_MM = 0.127 // JLCPCB standard two-layer at 1 oz copper

{
  const byTrace = Object.fromEntries(of('source_trace').map((e) => [e.source_trace_id, e]))
  const segs = []
  for (const t of of('pcb_trace')) {
    const net = byTrace[t.source_trace_id]?.subcircuit_connectivity_map_key ?? t.source_trace_id
    const pts = (t.route ?? []).filter((r) => r.route_type === 'wire')
    for (let i = 0; i + 1 < pts.length; i++) {
      if (pts[i].layer !== pts[i + 1].layer) continue
      segs.push({ net, layer: pts[i].layer, a: pts[i], b: pts[i + 1], w: pts[i].width ?? 0.15 })
    }
  }

  const segDist = (p1, p2, p3, p4) => {
    const ptSeg = (p, q, r) => {
      const dx = r.x - q.x
      const dy = r.y - q.y
      const L = dx * dx + dy * dy
      if (L === 0) return Math.hypot(p.x - q.x, p.y - q.y)
      const t = Math.max(0, Math.min(1, ((p.x - q.x) * dx + (p.y - q.y) * dy) / L))
      return Math.hypot(p.x - (q.x + t * dx), p.y - (q.y + t * dy))
    }
    const ccw = (m, n, o) => (o.y - m.y) * (n.x - m.x) > (n.y - m.y) * (o.x - m.x)
    if (ccw(p1, p3, p4) !== ccw(p2, p3, p4) && ccw(p1, p2, p3) !== ccw(p1, p2, p4)) return 0
    return Math.min(ptSeg(p1, p3, p4), ptSeg(p2, p3, p4), ptSeg(p3, p1, p2), ptSeg(p4, p1, p2))
  }

  let hard = 0
  let soft = 0
  let worst = Infinity
  for (let i = 0; i < segs.length; i++) {
    for (let j = i + 1; j < segs.length; j++) {
      const x = segs[i]
      const y = segs[j]
      if (x.net === y.net || x.layer !== y.layer) continue
      const edge = segDist(x.a, x.b, y.a, y.b) - x.w / 2 - y.w / 2
      if (edge < worst) worst = edge
      if (edge < HARD_MM) hard++
      else if (edge < SOFT_MM) soft++
    }
  }
  if (hard > 0) {
    blockers.push({
      id: 'copper-clearance',
      n: hard,
      what:
        `${hard} copper pair(s) on the same layer and different nets closer than ${HARD_MM} mm, ` +
        `closest ${worst.toFixed(3)} mm. A negative figure means they cross, which is a short. ` +
        `tscircuit's own design rule pass reports none of these, and raising the autorouter's ` +
        `traceClearance does not change them; the sequential_trace preset lays no copper at all ` +
        `and a four-layer board did not finish routing in ten minutes. This is the autorouter, ` +
        `not the placement, and it is the one thing between this board and a fab.`,
    })
  } else if (soft > 0) {
    advisories.push(
      `${soft} copper pair(s) under ${SOFT_MM} mm, closest ${worst.toFixed(3)} mm. Orderable from ` +
        `PCBWay, whose standard two-layer process is 4 mil trace and space, and not from JLCPCB, ` +
        `whose standard two-layer at 1 oz copper is 5 mil.`
    )
  } else if (segs.length > 0) {
    advisories.push(`copper clearance clean, closest ${worst.toFixed(3)} mm across ${segs.length} segments`)
  }
}

// --- the outline the enclosure is built from ---------------------------------

{
  const board = of('pcb_board')[0]
  // The outline is checked against data/mechanical.yaml by tools/check-data.mjs,
  // which owns that comparison because the enclosure is built from the same
  // numbers. Repeating it here would mean this file needs a YAML parser that
  // the hardware CI job does not install, to re-assert something already
  // asserted. What this file owns is the geometry inside the outline.
  if (!board) {
    blockers.push({ id: 'no-board', n: 1, what: 'the export contains no pcb_board element' })
  } else {
    // Every pad has to be on the board. A footprint hanging over the edge is
    // milled away, and the render makes it look deliberate.
    const hx = board.width / 2
    const hy = board.height / 2
    let off = 0
    for (const t of ['pcb_smtpad', 'pcb_plated_hole']) {
      for (const p of of(t)) {
        const w = (p.width ?? p.outer_diameter ?? 0) / 2
        const h = (p.height ?? p.outer_diameter ?? 0) / 2
        if (Math.abs(p.x) + w > hx + 1e-6 || Math.abs(p.y) + h > hy + 1e-6) off++
      }
    }
    if (off > 0) {
      blockers.push({ id: 'pad-off-board', n: off, what: `${off} pad(s) extend past the board outline` })
    }

    // Copper to board edge. Being inside the outline is not enough: the router
    // bit that cuts the board has a radius and a tolerance, so copper closer
    // than this gets nicked or exposed. SW1 sat 0.25 mm from the edge here,
    // which looks fine in a render and is under every fab's minimum.
    const EDGE_MM = 0.3
    let tight = 0
    let closest = Infinity
    for (const t of ['pcb_smtpad', 'pcb_plated_hole']) {
      for (const p of of(t)) {
        const w = (p.width ?? p.outer_diameter ?? 0) / 2
        const h = (p.height ?? p.outer_diameter ?? 0) / 2
        const m = Math.min(hx - (Math.abs(p.x) + w), hy - (Math.abs(p.y) + h))
        if (m < closest) closest = m
        if (m < EDGE_MM) tight++
      }
    }
    if (tight > 0) {
      blockers.push({
        id: 'edge-clearance',
        n: tight,
        what: `${tight} pad(s) within ${EDGE_MM} mm of the board edge, closest ${closest.toFixed(3)} mm`,
      })
    } else if (Number.isFinite(closest)) {
      advisories.push(`closest copper to board edge ${closest.toFixed(2)} mm`)
    }
  }
}

// --- advisories --------------------------------------------------------------

// The check nothing can make for you.
//
// A pinLabel maps a signal name onto a footprint pad BY NUMBER, and nothing in
// this repository knows whether pad 1 of a given footprint is the pad the
// part's datasheet calls pin 1. A sibling project found its 40-way header
// numbered the way an IC package is, counter-clockwise, which would have wired
// a GNSS pulse-per-second line to an I2S bit clock. Nothing about that is
// visible in a schematic, a render, or any check here: it shows up when a
// fabricated board does not work. J1 is the live example, since a USB-C
// receptacle's A1 is ground rather than VBUS.
{
  const labelled = of('source_port').filter((p) => (p.port_hints ?? []).some((h) => !/^(pin)?\d+$/.test(h))).length
  advisories.push(
    `${labelled} signal names are assigned to footprint pads by number, and NOTHING HERE ` +
      `CHECKS that the numbering matches the datasheet. Confirm each package by hand.`
  )
}

for (const [type, label] of [
  ['supplier_footprint_mismatch_warning', 'footprint does not match the supplier part'],
  ['source_component_pins_underspecified_warning', 'component pins underspecified'],
  ['source_refdes_convention_warning', 'reference designator convention'],
  ['source_unnamed_trace_warning', 'unnamed trace'],
]) {
  const n = count(type)
  if (n > 0) advisories.push(`${n} x ${label}`)
}

// --- report ------------------------------------------------------------------

const fabReady = blockers.length === 0
const state = Object.fromEntries(blockers.map((b) => [b.id, b.n]))

console.log(`PCB: ${count('pcb_component')} components, ${routed} routed traces, ${count('pcb_via')} vias, ${netlist} nets`)

if (fabReady) {
  console.log('\nFabrication blockers: none. This board can be ordered.')
} else {
  console.log('\nFabrication blockers, which is why no Gerbers are published:')
  for (const b of blockers) console.log(`  [${b.id}] ${b.what}`)
}
if (advisories.length) {
  console.log('\nAdvisory:')
  for (const a of advisories) console.log(`  ${a}`)
}

if (process.argv.includes('--fab')) {
  if (!fabReady) {
    console.error('\nRefusing to publish fabrication files for a board with open blockers.')
    process.exit(1)
  }
  process.exit(0)
}

// --- the ratchet -------------------------------------------------------------

// The site renders this rather than prose, so a page cannot claim the board is
// closer to fabricable than the checker found it. Written on --update and
// verified on every ordinary run, the same way the schematic is.
const status =
  JSON.stringify(
    {
      note: 'Generated by tools/check-pcb.mjs. The site renders this; do not edit by hand.',
      fab_ready: fabReady,
      components: count('pcb_component'),
      nets: netlist,
      routed_traces: routed,
      vias: count('pcb_via'),
      blockers: blockers.map((b) => ({ id: b.id, count: b.n, what: b.what })),
      advisories,
    },
    null,
    2
  ) + '\n'

if (process.argv.includes('--update')) {
  writeFileSync(BASELINE, JSON.stringify({ note: 'Recorded by tools/check-pcb.mjs --update. Counts may fall, never rise.', blockers: state }, null, 2) + '\n')
  writeFileSync(STATUS, status)
  console.log('\nBaseline and status updated.')
  process.exit(0)
}

if (!existsSync(STATUS) || readFileSync(STATUS, 'utf8') !== status) {
  console.error('\nhardware/pcb-status.json is stale. Run `node tools/check-pcb.mjs --update`.')
  process.exit(1)
}

if (!existsSync(BASELINE)) {
  console.error('\nhardware/pcb-baseline.json is missing. Run `node tools/check-pcb.mjs --update`.')
  process.exit(1)
}
const base = JSON.parse(readFileSync(BASELINE, 'utf8')).blockers ?? {}
const worse = []
const better = []
for (const id of new Set([...Object.keys(base), ...Object.keys(state)])) {
  const was = base[id] ?? 0
  const now = state[id] ?? 0
  if (now > was) worse.push(`${id}: ${was} -> ${now}`)
  if (now < was) better.push(`${id}: ${was} -> ${now}`)
}

if (worse.length) {
  console.error(`\nThe board got worse:\n  ${worse.join('\n  ')}`)
  process.exit(1)
}
if (better.length) {
  console.error(
    `\nThe board got better:\n  ${better.join('\n  ')}\n` +
      'Run `node tools/check-pcb.mjs --update` and commit the baseline, so the improvement is\n' +
      'the new floor rather than headroom to regress into.'
  )
  process.exit(1)
}
console.log('\nNo regression against hardware/pcb-baseline.json.')

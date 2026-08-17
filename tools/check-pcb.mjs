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
  // An invalid prop is not a warning. tscircuit reports a footprint it does not
// recognise as source_invalid_component_property_error, exits zero, and builds
// the component with no pads: the part exists on the schematic, has no copper
// on the board, and its nets quietly go unrouted. That is how J5 shipped as a
// GNSS antenna socket that was not connected to anything.
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
// This check used to compare a component's pad count against its port count and
// blocked when there were more pads than ports. It never fired once, because
// tscircuit creates a port for every pad in the footprint whether or not the
// source names it. pads > ports is not reachable, so the check passed on every
// run while the radio had nine of twenty-five pins wired.
//
// What actually matters is how many pads the source NAMED. An unnamed port is
// called pin17, it is connected to nothing, and on a real integrated circuit
// pin17 is a supply rail, an RF port, or a crystal terminal. The board this
// produces has an SX1262 with no reference oscillator and no RF front end, and
// it exports a clean Gerber package, because a pad nobody named looks exactly
// like a pad nobody needed.
//
// Only parts with a manufacturer part number are held to this. A two terminal
// passive placed in a four pad land pattern is a footprint question, not a
// pinout question.

/**
 * Is this pad named by the source, under any of the ways a source can name one?
 *
 * `pinLabels` sets the port's name directly. A hand written footprint names its
 * pads with portHints instead, and those arrive as aliases with the name left as
 * pinN, which is how J1 came to report zero of twenty pads named while every one
 * of its traces resolved by designator. Both are the source naming the pad.
 *
 * A bare number is not a name. tscircuit adds '1', '2' and so on as hints on
 * every port, so counting those would make every pad look named and the check
 * would go quiet the way its predecessor did.
 */
const GENERIC_PIN = /^(?:pin)?\d+$/i

function isNamed(port) {
  if (!GENERIC_PIN.test(String(port.name ?? ''))) return true
  return (port.port_hints ?? []).some((h) => !GENERIC_PIN.test(String(h)))
}

{
  const named = {}
  const generic = {}
  for (const port of of('source_port')) {
    const bucket = isNamed(port) ? named : generic
    bucket[port.source_component_id] = (bucket[port.source_component_id] ?? 0) + 1
  }

  const gaps = []
  for (const src of of('source_component')) {
    if (!src.manufacturer_part_number) continue
    const unnamed = generic[src.source_component_id] ?? 0
    if (unnamed === 0) continue
    const total = unnamed + (named[src.source_component_id] ?? 0)
    gaps.push({ name: src.name, mpn: src.manufacturer_part_number, named: total - unnamed, total })
  }
  gaps.sort((a, b) => b.total - b.named - (a.total - a.named))

  if (gaps.length) {
    blockers.push({
      id: 'pinout-incomplete',
      n: gaps.length,
      what:
        `${gaps.length} part(s) have footprint pads the source never named, so those pads are ` +
        `on the board connected to nothing: ` +
        gaps.map((g) => `${g.name} ${g.mpn} ${g.named}/${g.total}`).join(', ') +
        `. On an integrated circuit the unnamed pads are supply rails, RF ports and crystal ` +
        `terminals. Transcribe each part's pinout from its datasheet into pinLabels and wire ` +
        `what it requires.`,
      detail: gaps,
    })
  }
}

// --- named pins that go nowhere ----------------------------------------------
//
// This one exists because the board shipped two dead-on-arrival defects that
// every other check in this repository passed cleanly.
//
// XIN and XOUT were declared on the microcontroller and connected to nothing.
// The RP2350 runs without a crystal, so nothing complained, and the board would
// have been unflashable: USB needs an accurate reference and USB is the only
// connector it has. RF_IN on the GNSS receiver was declared and connected to
// nothing, which is a Track tier that cannot produce a position, the one thing
// that tier exists for.
//
// Both built, routed, exported a clean Gerber package and reported no
// fabrication blockers, because a pin nobody connects is indistinguishable from
// a pin nobody needed. The difference is intent, so intent has to be written
// down: a named pin is either connected or listed here with the reason.
//
// Only named pins. A pin called pin7 is a footprint pad that no declared pin
// maps onto, which is the pinout-incomplete blocker above and a different
// problem. And spare GPIOs on the microcontroller are spare on purpose: an
// unused GPIO is a header waiting to happen, not a defect.

const INTENTIONALLY_UNCONNECTED = {
  'U1.STAT': 'Charger status output. There is no charge indicator LED on this board: the ' +
    'status LED is driven by the microcontroller, which knows more about what is happening ' +
    'than the charger does.',
  'U8.DIO3': 'Multi-purpose IO, and the supply pin for a temperature compensated ' +
    'oscillator. This design uses a plain crystal, so there is nothing for it to power, and ' +
    'the radio already has an interrupt line on DIO1.',
  'U6.INT1': 'IMU data ready interrupt. The firmware polls the sensors on a fixed cadence ' +
    'rather than reacting to them, because a flight log with an irregular sample interval is ' +
    'harder to reason about than one that occasionally reads the same sample twice.',
  'U5.INT': 'Barometer interrupt output. Unused for the same reason as the IMU and high-g ' +
    'interrupts: the firmware samples on a fixed cadence rather than reacting to the parts. The ' +
    'BMP390 datasheet lists this pin identically for all three interface modes as "host INT input ' +
    'or DNC", so leaving it open is one of the two documented treatments.',
  'SW1.T3': 'The arming switch\'s second throw, left open on purpose. The common goes to the ' +
    'GPIO and one throw to ground, so the internal pull-up defines the pin. Driving this throw to ' +
    '3V3 would give a defined level in both positions and look tidier, but a slide switch is open ' +
    'for a moment as the wiper crosses, so the pin would float briefly on every actuation and the ' +
    'firmware would have to debounce a state it can otherwise trust.',
  'J1.A8': 'Sideband use pins. Unused, which is what a USB 2.0 only design does with them: they ' +
    'carry analogue audio or debug signals in modes this payload has no part in. GCT specifies no ' +
    'board connection for either.',
  'J1.B8': 'See J1.A8.',
  'U9.TIMEPULSE': 'One pulse per second output. Not used: this payload timestamps from arming, ' +
    'not from GNSS time, and it has to work on Solo where there is no receiver at all. Left open ' +
    'per the datasheet, which also warns this pin must have no load that could pull it low at ' +
    'startup, because it shares an internal 1 kilohm path with SAFEBOOT_N.',
  'U9.EXTINT': 'External interrupt input. Left open, which the datasheet permits.',
  'U9.RESET_N': 'Input only with an internal pull-up to V_IO, and the datasheet says to leave it ' +
    'open for normal operation. It also says no capacitor should be placed from this pin to ' +
    'ground, which is a thing somebody would add while tidying up a reset line.',
  'U9.LNA_EN': 'Controls an external low noise amplifier or an active antenna supply, neither of ' +
    'which this design has: the module integrates its own LNA and SAW filter. The datasheet notes ' +
    'this pin also controls the internal LNA and cannot be repurposed.',
  'U9.VCC_RF': 'An OUTPUT, not a supply input: VCC filtered through an internal ferrite, provided ' +
    'to power an external active antenna. Open because the antenna is passive. Wiring it to a ' +
    'rail would be connecting two supplies together.',
  'U9.VIO_SEL': 'Selects the IO voltage. Open selects 3.3 V, which is this board\'s only rail; ' +
    'grounding it would select 1.8 V.',
  'U9.SDA': 'The I2C interface, which the datasheet calls DDC. Unused because the receiver is on ' +
    'a UART: one interface is enough, and a UART needs no pull-ups and no address.',
  'U9.SCL': 'See U9.SDA.',
  'U9.SAFEBOOT_N': 'Pulled low at startup only to enter safeboot, for recovering a module whose ' +
    'firmware will not run. Internal pull-up, left open for normal operation.',
  'U7.INT1': 'High-g accelerometer interrupt outputs, both unused for the same reason as the ' +
    'IMU\'s: the firmware samples on a fixed cadence. The datasheet states no required external ' +
    'connection for either, and they are push-pull outputs, so open is safe.',
  'U7.INT2': 'See U7.INT1.',
  'U7.NC': 'Not internally connected, per Table 5 of the ADXL375 datasheet. There is nothing ' +
    'inside the package to connect to.',
  'U6.INT2': 'The ICM-42688-P multiplexes this pin between a second interrupt, a frame sync ' +
    'input and an external clock input, and PIN9_FUNCTION selects which. Left open deliberately, ' +
    'and NOT grounded, even though the datasheet says to ground it when frame sync is unused: ' +
    'that instruction applies to the frame sync function, and the register resets to 00, which ' +
    'is the second interrupt. So on a board that ties this pin to ground, every power-on has the ' +
    'part driving a push-pull output into a short until firmware reconfigures it. Open is safe ' +
    'under both readings.',
}

{
  const comps = Object.fromEntries(of('source_component').map((e) => [e.source_component_id, e.name]))
  const connected = new Set()
  for (const t of of('source_trace')) {
    for (const id of t.connected_source_port_ids ?? []) connected.add(id)
  }

  const orphans = []
  for (const port of of('source_port')) {
    if (connected.has(port.source_port_id)) continue
    if (!isNamed(port)) continue
    // Report by the most descriptive name the source gave it, which for a hand
    // written footprint is the portHint rather than the port's own name.
    const name =
      (GENERIC_PIN.test(String(port.name ?? ''))
        ? (port.port_hints ?? []).find((h) => !GENERIC_PIN.test(String(h)))
        : port.name) ?? String(port.name ?? '')
    const comp = comps[port.source_component_id] ?? '?'
    if (/^GPIO\d+$/i.test(name)) continue
    const key = `${comp}.${name}`
    if (key in INTENTIONALLY_UNCONNECTED) continue
    orphans.push(key)
  }

  if (orphans.length) {
    blockers.push({
      id: 'pin-unconnected',
      n: orphans.length,
      what:
        `${orphans.length} named pin(s) are declared and connected to nothing: ` +
        orphans.join(', ') +
        `. Either wire them, or add them to INTENTIONALLY_UNCONNECTED in tools/check-pcb.mjs ` +
        `with the reason. A pin left out by accident and a pin left out on purpose look ` +
        `identical in a netlist, and only one of them is a working board.`,
      detail: orphans,
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
// Counted where the clearance is measured, read by the ratchet at the bottom.
let softPairs = 0
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
    softPairs = soft
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
/**
 * What the ratchet watches.
 *
 * Blockers, plus the count of copper pairs too close together for the cheaper
 * fabricator. That second one is not a blocker: the board is orderable with it,
 * from PCBWay rather than JLCPCB, and calling it a blocker would stop a board
 * that can be made. But it is a real number about how much the board costs to
 * make and how likely a run is to come back with shorts, and it moved from 37
 * to 82 in a single commit that added two connectors, with nothing to say so.
 *
 * A number that only ever appears in an advisory list is a number that can
 * double while every check still prints a reassuring summary.
 */
const state = Object.fromEntries(blockers.map((b) => [b.id, b.n]))
state['soft-clearance'] = softPairs

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

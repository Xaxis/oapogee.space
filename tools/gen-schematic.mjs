#!/usr/bin/env node
/**
 * The whole payload as one sheet.
 *
 * Somebody deciding whether they can build this needs to see the shape of it
 * before they read a parts table: four sensors onto one microcontroller, the
 * log going to flash that is soldered down rather than to a card that can
 * unseat, one connector doing power and charging and configuration and offload,
 * and a power chain that goes USB to charger to cell to rail. That is the whole
 * design, and it fits on one sheet.
 *
 * The tier colouring is the other half of the argument. Solo, Link and Track
 * are the same board with different footprints populated, and a diagram that
 * shows the radio and the receiver greyed rather than absent makes the upgrade
 * promise visible instead of asserted.
 *
 * Generated rather than drawn. A diagram maintained by hand disagrees with the
 * hardware within a month, and nobody notices until a builder follows it. Every
 * node names a part in data/bom.yaml, check-data fails when one stops
 * resolving, and CI fails when the committed SVG no longer matches this source.
 *
 *   node tools/gen-schematic.mjs           write the SVG
 *   node tools/gen-schematic.mjs --check   exit 1 if the committed file is stale
 */

import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const system = parse(readFileSync(join(ROOT, 'data/system.yaml'), 'utf8'))
const tiers = parse(readFileSync(join(ROOT, 'data/tiers.yaml'), 'utf8'))

const OUT_DIR = join(ROOT, 'apps/web/public/schematic')
const OUT = join(OUT_DIR, 'system.svg')

// The site is dark and committed to it, so the sheet is drawn once for that
// surface rather than trying to be two drawings badly. Hi-vis yellow carries
// the same meaning here that it does on the board and the pod.
const INK = '#f2f4f7'
const INK2 = '#9aa1ab'
const INK3 = '#6a707a'
const SURFACE = '#101113'
const PANEL = '#17191c'
const LINE = '#2f343b'
const HIVIS = '#ffcf1b'
const ORANGE = '#ff7a1a'
const GREEN = '#3ecf8e'

const KIND_STROKE = {
  bus: '#7f8792',
  data: HIVIS,
  rf: ORANGE,
  power: GREEN,
}
const KIND_DASH = { bus: '', data: '', rf: '5 4', power: '' }

const esc = (s) =>
  String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')

const MONO = 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace'
// Single quotes inside the stack, not double: the whole string goes into an
// XML attribute, and a double quote there ends the attribute and produces an
// SVG that no parser will open.
const SANS = "system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif"

const text = (x, y, s, o = {}) =>
  `<text x="${x}" y="${y}" font-family="${o.mono ? MONO : SANS}" font-size="${o.size ?? 11}" ` +
  `font-weight="${o.weight ?? 400}" fill="${o.fill ?? INK}"` +
  `${o.anchor ? ` text-anchor="${o.anchor}"` : ''}` +
  `${o.spacing ? ` letter-spacing="${o.spacing}"` : ''}>${esc(s)}</text>`

// SVG has no text wrapping and no way to measure a glyph without a layout
// engine, so widths are approximated from the font size. The ratios are
// conservative, which costs a few characters and never overflows.
const charWidth = (size, mono) => size * (mono ? 0.6 : 0.53)

function fit(s, maxWidth, size, mono) {
  const str = String(s)
  const max = Math.floor(maxWidth / charWidth(size, mono))
  return str.length <= max ? str : str.slice(0, Math.max(1, max - 1)).trimEnd() + '…'
}

// --- geometry ---------------------------------------------------------------

const PAD = 32
const HEADER = 76
const BOX_W = 168
const BOX_H = 62
const GAP_X = 66
const GAP_Y = 30
const LEGEND_H = 78

const cols = system.grid.cols
const rows = system.grid.rows

const W = PAD * 2 + cols * BOX_W + (cols - 1) * GAP_X
const BODY_H = rows * BOX_H + (rows - 1) * GAP_Y
const H = HEADER + BODY_H + LEGEND_H + PAD

const nodeById = new Map(system.nodes.map((n) => [n.id, n]))

const xOf = (col) => PAD + col * (BOX_W + GAP_X)
const yOf = (row) => HEADER + row * (BOX_H + GAP_Y)

const box = (n) => ({
  x: xOf(n.col),
  y: yOf(n.row),
  w: BOX_W,
  h: BOX_H,
  cx: xOf(n.col) + BOX_W / 2,
  cy: yOf(n.row) + BOX_H / 2,
})

// --- tier marks -------------------------------------------------------------

const TIER_ORDER = tiers.tiers.map((t) => t.id)
const TIER_INITIAL = Object.fromEntries(
  tiers.tiers.map((t) => [t.id, t.name.replace('oApogee ', '').charAt(0)])
)

/**
 * Three ticks per box, one per tier, lit when that tier populates the part.
 * This is the upgrade promise drawn rather than claimed: a reader can see at a
 * glance that Solo is the same board with two footprints left empty.
 */
function tierMarks(n, b) {
  const size = 11
  const gap = 3
  const total = TIER_ORDER.length * size + (TIER_ORDER.length - 1) * gap
  let x = b.x + b.w - 9 - total
  const y = b.y + b.h - 15
  const out = []
  for (const tier of TIER_ORDER) {
    const on = (n.tiers ?? []).includes(tier)
    out.push(
      `<rect x="${x}" y="${y}" width="${size}" height="${size}" rx="2" ` +
        `fill="${on ? HIVIS : 'none'}" stroke="${on ? HIVIS : LINE}" stroke-width="1"/>`,
      text(x + size / 2, y + size - 2.5, TIER_INITIAL[tier], {
        mono: true,
        size: 8,
        weight: 600,
        anchor: 'middle',
        fill: on ? '#000' : INK3,
      })
    )
    x += size + gap
  }
  return out.join('')
}

// --- edge routing -----------------------------------------------------------

/**
 * Orthogonal routing with a single mid-span jog. Diagonals across a block
 * diagram read as "roughly connected"; right angles read as wiring, which is
 * what this is. The jog sits between the two boxes so the vertical run never
 * crosses a box it does not touch.
 */
function route(a, b, jog, fromPort = 0, toPort = 0) {
  const from = box(a)
  const to = box(b)
  // Port offsets shift where an edge attaches, so that two edges meeting the
  // same box side do not land on the same point and run over each other.
  const fy = from.cy + fromPort
  const ty = to.cy + toPort

  if (a.row === b.row) {
    const goingRight = to.x > from.x
    const x1 = goingRight ? from.x + from.w : from.x
    const x2 = goingRight ? to.x : to.x + to.w
    return { d: `M ${x1} ${fy} L ${x2} ${ty}`, mx: (x1 + x2) / 2, my: fy }
  }

  const goingRight = to.cx > from.cx
  if (a.col === b.col) {
    // Straight vertical, entering top or bottom.
    const goingDown = to.y > from.y
    const y1 = goingDown ? from.y + from.h : from.y
    const y2 = goingDown ? to.y : to.y + to.h
    return { d: `M ${from.cx} ${y1} L ${to.cx} ${y2}`, mx: from.cx, my: (y1 + y2) / 2 }
  }

  const x1 = goingRight ? from.x + from.w : from.x
  const x2 = goingRight ? to.x : to.x + to.w
  // Default to the midpoint. An edge whose vertical run would cross a box it
  // does not touch overrides this with `jog` in data/system.yaml, expressed as
  // a fraction from the source end to the destination end.
  const mid = x1 + (x2 - x1) * (typeof jog === 'number' ? jog : 0.5)
  return {
    d: `M ${x1} ${fy} L ${mid} ${fy} L ${mid} ${ty} L ${x2} ${ty}`,
    mx: mid,
    my: (fy + ty) / 2,
  }
}

// --- render -----------------------------------------------------------------

const parts = []

parts.push(
  `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${W}" height="${H}" ` +
    `role="img" aria-label="${esc(system.title)}: system block diagram">`
)

parts.push(`<defs>`)
for (const [kind, stroke] of Object.entries(KIND_STROKE)) {
  parts.push(
    `<marker id="arrow-${kind}" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="6" ` +
      `markerHeight="6" orient="auto-start-reverse">` +
      `<path d="M 0 1 L 7 4 L 0 7 z" fill="${stroke}"/></marker>`
  )
}
parts.push(`</defs>`)

parts.push(`<rect width="${W}" height="${H}" fill="${SURFACE}"/>`)

// Header
parts.push(text(PAD, 30, system.title, { size: 16, weight: 600 }))
parts.push(text(PAD, 49, system.subtitle, { size: 11.5, fill: INK2 }))
parts.push(
  text(W - PAD, 30, 'oapogee.space', { size: 10, mono: true, fill: INK3, anchor: 'end' })
)
parts.push(
  `<path d="M ${PAD} ${HEADER - 16} L ${W - PAD} ${HEADER - 16}" stroke="${LINE}" stroke-width="1"/>`
)

// Edges first, so boxes sit on top of the lines rather than under them.
for (const edge of system.edges) {
  const a = nodeById.get(edge.from)
  const b = nodeById.get(edge.to)
  if (!a || !b) throw new Error(`edge references unknown node: ${edge.from} -> ${edge.to}`)

  const stroke = KIND_STROKE[edge.kind] ?? INK3
  const dash = KIND_DASH[edge.kind] ?? ''
  const { d, mx, my } = route(a, b, edge.jog, edge.from_port ?? 0, edge.to_port ?? 0)

  parts.push(
    `<path d="${d}" fill="none" stroke="${stroke}" stroke-width="1.4" stroke-opacity="0.75" ` +
      `${dash ? `stroke-dasharray="${dash}" ` : ''}marker-end="url(#arrow-${edge.kind})"/>`
  )

  if (edge.label) {
    const w = edge.label.length * 5.6 + 10
    parts.push(
      `<rect x="${mx - w / 2}" y="${my - 8}" width="${w}" height="15" rx="3" fill="${SURFACE}"/>`,
      text(mx, my + 3, edge.label, { mono: true, size: 9, fill: stroke, anchor: 'middle' })
    )
  }
}

// Boxes
for (const n of system.nodes) {
  const b = box(n)
  const stroke = n.emphasis ? HIVIS : n.external ? INK3 : LINE
  const fill = n.external ? 'none' : PANEL

  parts.push(
    `<rect x="${b.x}" y="${b.y}" width="${b.w}" height="${b.h}" rx="6" fill="${fill}" ` +
      `stroke="${stroke}" stroke-width="${n.emphasis ? 1.6 : 1}"` +
      `${n.external ? ' stroke-dasharray="4 4"' : ''}/>`
  )

  // The tier ticks occupy the bottom right corner, so the note line has to stop
  // short of them. Everything is clipped to what actually fits rather than
  // trusted to be short enough: a caption that overruns its box and lands on an
  // arrow is worse than a truncated one.
  const INSET = 12
  const full = b.w - INSET * 2
  const beside = full - 56

  parts.push(text(b.x + INSET, b.y + 21, fit(n.label, full, 12, false), { size: 12, weight: 600 }))
  if (n.sub) {
    parts.push(
      text(b.x + INSET, b.y + 36, fit(n.sub, full, 9.5, true), {
        size: 9.5,
        fill: INK2,
        mono: true,
      })
    )
  }
  if (n.note) {
    parts.push(
      text(b.x + INSET, b.y + 51, fit(n.note, beside, 9.5, false), { size: 9.5, fill: INK3 })
    )
  }
  // Optional parts get the marker in the header row, right-aligned, where it
  // cannot collide with the note beneath it.
  if (n.optional) {
    parts.push(
      text(b.x + b.w - INSET, b.y + 21, 'optional', {
        size: 9,
        fill: ORANGE,
        mono: true,
        anchor: 'end',
      })
    )
  }
  if (!n.external) parts.push(tierMarks(n, b))
}

// Legend
const legendY = HEADER + BODY_H + 34
let lx = PAD
for (const item of system.legend) {
  const stroke = KIND_STROKE[item.kind] ?? INK3
  const dash = KIND_DASH[item.kind]
  parts.push(
    `<path d="M ${lx} ${legendY} L ${lx + 26} ${legendY}" stroke="${stroke}" stroke-width="1.6" ` +
      `${dash ? `stroke-dasharray="${dash}" ` : ''}/>`,
    text(lx + 33, legendY + 4, item.label, { size: 10, fill: INK2 })
  )
  lx += 33 + item.label.length * 6 + 26
}

// Tier key, so the ticks on each box mean something without a caption.
let tx = lx + 8
parts.push(text(tx, legendY + 4, 'Populated in:', { size: 10, fill: INK3 }))
tx += 78
for (const tier of tiers.tiers) {
  const initial = TIER_INITIAL[tier.id]
  parts.push(
    `<rect x="${tx}" y="${legendY - 8}" width="11" height="11" rx="2" fill="${HIVIS}"/>`,
    text(tx + 5.5, legendY + 1.5, initial, {
      mono: true,
      size: 8,
      weight: 600,
      anchor: 'middle',
      fill: '#000',
    }),
    text(tx + 16, legendY + 4, tier.name.replace('oApogee ', ''), { size: 10, fill: INK2 })
  )
  tx += 16 + tier.name.replace('oApogee ', '').length * 6.4 + 14
}

parts.push(text(PAD, legendY + 28, system.footnote.trim(), { size: 10, fill: INK3 }))

parts.push('</svg>')

const svg = parts.join('\n') + '\n'

if (process.argv.includes('--check')) {
  if (!existsSync(OUT)) {
    console.error('apps/web/public/schematic/system.svg is missing. Run `make schematic`.')
    process.exit(1)
  }
  if (readFileSync(OUT, 'utf8') !== svg) {
    console.error(
      'The committed schematic no longer matches data/system.yaml. Run `make schematic` and commit the result.'
    )
    process.exit(1)
  }
  console.log('Schematic is current.')
} else {
  mkdirSync(OUT_DIR, { recursive: true })
  writeFileSync(OUT, svg)
  console.log(`Wrote ${OUT.slice(ROOT.length)} (${system.nodes.length} nodes, ${system.edges.length} edges).`)
}

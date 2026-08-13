#!/usr/bin/env node
/**
 * Render the tscircuit source into artifacts the site can serve and a reviewer
 * can read.
 *
 * Four outputs, and they are not equally trustworthy, which is the reason this
 * file explains itself rather than just shelling out.
 *
 * The schematic SVG and the readable netlist are derived entirely from the
 * connectivity in hardware/oapogee.tsx. They are exactly as correct as that
 * file is, which is to say the wiring is reviewable and the pin numbers are
 * placeholders. Both are worth publishing.
 *
 * The KiCad export is what makes the openness promise real. The About page says
 * this project publishes the KiCad project rather than a PDF of a schematic,
 * and this is the step that produces one.
 *
 * Circuit JSON is the machine-readable form, for anyone who wants to write
 * their own tooling against the design.
 *
 * There is deliberately no PCB or 3D output. A layout needs real footprints,
 * and real footprints need the datasheet pin mapping that this design has not
 * done yet. A routed board built on placeholder pins would look far more
 * finished than it is, which is the one thing this project must not ship.
 *
 *   node tools/build-hardware.mjs           write the artifacts
 *   node tools/build-hardware.mjs --check   exit 1 if the committed ones are stale
 */

import { execFileSync } from 'node:child_process'
import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const HW = join(ROOT, 'hardware')
const SOURCE = 'oapogee.tsx'
const OUT = join(ROOT, 'apps/web/public/hardware')

const cli = join(HW, 'node_modules/tscircuit/cli.mjs')
const POLYFILL = join(HW, 'iterator-helpers-polyfill.js')
if (!existsSync(cli)) {
  console.error('The tscircuit CLI is not installed. Run `make hw-deps`.')
  process.exit(1)
}

// The CLI runs under bun. Under node it cannot load a .tsx entry point at all.
function haveBun() {
  try {
    execFileSync('bun', ['--version'], { stdio: 'pipe' })
    return true
  } catch {
    return false
  }
}

if (!haveBun()) {
  console.error('bun is required to run the tscircuit CLI. See hardware/README.md.')
  process.exit(1)
}

// Gerbers and drill files are deliberately NOT in this list. They are the
// artifact somebody spends money on, and this board does not pass
// tools/check-pcb.mjs. `make fab` gates them behind that check; publishing them
// beside a board with open blockers would be the most expensive kind of
// confident wrong thing this project can produce.
const FAB_ARTIFACTS = [
  { format: 'gerbers', file: 'oapogee-gerbers.zip' },
  { format: 'kicad_pcb', file: 'oapogee.kicad_pcb' },
]

const ARTIFACTS = [
  { format: 'schematic-svg', file: 'oapogee-schematic.svg' },
  { format: 'readable-netlist', file: 'oapogee-netlist.txt' },
  { format: 'circuit-json', file: 'oapogee-circuit.json' },
  { format: 'pcb-svg', file: 'oapogee-pcb.svg' },
  { format: 'assembly-svg', file: 'oapogee-assembly.svg' },
  { format: 'kicad_sch', file: 'oapogee.kicad_sch' },
]

const check = process.argv.includes('--check')
const tmp = join(HW, '.build')
mkdirSync(tmp, { recursive: true })
mkdirSync(OUT, { recursive: true })

let stale = []


/**
 * Make an exported SVG fit a dark page, and give it a viewBox.
 *
 * Colour first: tscircuit draws on rgb(245,241,237) with black text, which on
 * this site is a bright rectangle in the middle of a dark page. Rewritten to
 * the site's own surface colour rather than themed by CSS, because the file is
 * also downloaded and opened outside the site.
 *
 * Then the viewBox. The exporter emits width="800" height="600" and puts the
 * drawing wherever it lands inside that, so a board 213 units wide sat in the
 * middle of an 800 unit canvas and most of the pan and zoom range was spent on
 * black. Tight to the content, with a small margin.
 */
function recolour(svg) {
  let out = svg
    .replace(/rgb\(245,\s*241,\s*237\)/g, '#0f1214')
    .replace(/fill:\s*#f2f2f2/g, 'fill: #0f1214')
    .replace(/stroke:\s*#000\b/g, 'stroke: #9aa4ab')
    .replace(/stroke:\s*rgb\(0,\s*0,\s*0\)/g, 'stroke: #9aa4ab')
    .replace(/fill:\s*#000\b/g, 'fill: #c8d0d6')

  if (!/viewBox=/.test(out)) {
    // The PCB and assembly exports both carry a boundary rect for the board.
    const b = /<rect[^>]*class="(?:pcb-boundary|assembly-boundary)"[^>]*>/.exec(out)
    const num = (attr, s) => Number(new RegExp(`\\s${attr}="([\\d.-]+)"`).exec(s)?.[1])
    if (b) {
      const x = num('x', b[0])
      const y = num('y', b[0])
      const w = num('width', b[0])
      const h = num('height', b[0])
      if ([x, y, w, h].every(Number.isFinite)) {
        const m = Math.max(w, h) * 0.06
        out = out.replace(
          /<svg([^>]*?)>/,
          `<svg$1 viewBox="${(x - m).toFixed(2)} ${(y - m).toFixed(2)} ${(w + 2 * m).toFixed(2)} ${(h + 2 * m).toFixed(2)}">`
        )
      }
    }
  }
  return out
}

const fab = process.argv.includes('--fab')
for (const { format, file } of fab ? FAB_ARTIFACTS : ARTIFACTS) {
  // The CLI resolves -o itself and fails on an absolute path, so it is given a
  // path relative to hardware/ and the result is read back from there.
  const relative = join('.build', file)
  const scratch = join(HW, relative)
  try {
    // --preload is not optional. tscircuit's default capacity-mesh autorouter
    // calls .map() on the Iterator returned by Map.prototype.entries(), which is
    // ES2025 Iterator Helpers and absent from the Bun this runs on. Without the
    // polyfill the router throws, the export still prints success and exits
    // zero, and the artifact is written with no copper in it at all. That
    // failure is silent in every way a build normally notices, which is why
    // tools/check-pcb.mjs counts the traces rather than trusting this call.
    execFileSync('bun', ['--preload', POLYFILL, cli, 'export', SOURCE, '-f', format, '-o', relative], {
      cwd: HW,
      stdio: 'pipe',
    })
  } catch (e) {
    console.error(`Failed to export ${format}:`)
    console.error(String(e.stderr ?? e.message))
    process.exit(1)
  }

  let produced = readFileSync(scratch)

  // tscircuit exports for a light page and sizes the canvas to a fixed 800x600
  // with the drawing somewhere inside it. Both are wrong here. The site is dark,
  // so a cream background punches a hole in every page it appears on; and a
  // viewer that pans by driving the viewBox needs one that is tight to the
  // drawing, or the reader spends the zoom range on empty canvas.
  if (file.endsWith('.svg')) {
    produced = Buffer.from(recolour(produced.toString('utf8')))
  }
  const target = join(OUT, file)

  if (check) {
    if (!existsSync(target)) {
      stale.push(`${file} is missing`)
      continue
    }
    // Circuit JSON embeds absolute source paths from the machine that built it,
    // so it can never match byte for byte on a second machine.
    if (format === 'circuit-json') continue

    // The KiCad export mints 223 fresh UUIDs on every run, so a byte comparison
    // reports a change that is not one. Normalising them still catches a real
    // design change, because a moved net changes far more than identifiers.
    const normalise = (buf) =>
      format === 'kicad_sch'
        ? buf.toString('utf8').replace(/[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}/g, '<uuid>')
        : buf.toString('utf8')

    if (normalise(readFileSync(target)) !== normalise(produced)) {
      stale.push(`${file} does not match the source`)
    }
  } else {
    writeFileSync(target, produced)
    console.log(`  ${file}  ${(produced.length / 1024).toFixed(0)} kB`)
  }
}

if (check) {
  if (stale.length) {
    for (const s of stale) console.error(`error ${s}`)
    console.error('\nRun `make hw` and commit the result.')
    process.exit(1)
  }
  console.log('Hardware artifacts are current.')
} else {
  console.log(`Wrote ${readdirSync(OUT).length} artifacts to apps/web/public/hardware/.`)
}

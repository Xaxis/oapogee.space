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

const ARTIFACTS = [
  { format: 'schematic-svg', file: 'oapogee-schematic.svg' },
  { format: 'readable-netlist', file: 'oapogee-netlist.txt' },
  { format: 'circuit-json', file: 'oapogee-circuit.json' },
  { format: 'kicad_sch', file: 'oapogee.kicad_sch' },
]

const check = process.argv.includes('--check')
const tmp = join(HW, '.build')
mkdirSync(tmp, { recursive: true })
mkdirSync(OUT, { recursive: true })

let stale = []

for (const { format, file } of ARTIFACTS) {
  // The CLI resolves -o itself and fails on an absolute path, so it is given a
  // path relative to hardware/ and the result is read back from there.
  const relative = join('.build', file)
  const scratch = join(HW, relative)
  try {
    execFileSync('bun', [cli, 'export', SOURCE, '-f', format, '-o', relative], {
      cwd: HW,
      stdio: 'pipe',
    })
  } catch (e) {
    console.error(`Failed to export ${format}:`)
    console.error(String(e.stderr ?? e.message))
    process.exit(1)
  }

  const produced = readFileSync(scratch)
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

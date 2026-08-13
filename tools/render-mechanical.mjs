#!/usr/bin/env node
/**
 * Render the printed parts to STL and to a preview image.
 *
 * Needs OpenSCAD, which CI does not have, so this is a local step and the STLs
 * are committed. What CI can still verify is that they were not left stale:
 * this writes a manifest of the source hashes next to them, and
 * `gen-mechanical.mjs --check` recomputes those hashes with nothing but node.
 * Editing a .scad and forgetting to re-render fails the build the same way
 * editing the schematic source and forgetting to regenerate does.
 *
 * --hardwarnings is not optional. OpenSCAD treats an undefined variable as a
 * warning and carries on, and it will happily export a closed manifold solid
 * built entirely from undefined arithmetic. That happened here: a params.scad
 * that had not been regenerated produced parts that passed every geometric
 * test and were garbage. Somebody would have printed them.
 *
 *   node tools/render-mechanical.mjs
 */

import { execFileSync } from 'node:child_process'
import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync } from 'node:fs'
import { createHash } from 'node:crypto'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const SRC = join(ROOT, 'hardware/mechanical')
const OUT = join(ROOT, 'apps/web/public/hardware/mechanical')
const mech = parse(readFileSync(join(ROOT, 'data/mechanical.yaml'), 'utf8'))

let openscad
try {
  openscad = execFileSync('which', ['openscad'], { encoding: 'utf8' }).trim()
} catch {
  console.error(
    'OpenSCAD is not installed, so the models cannot be rendered.\n' +
      'macOS: brew install --cask openscad\n' +
      'The committed STLs are still valid; this step is only needed after editing a .scad\n' +
      'or a dimension in data/mechanical.yaml.'
  )
  process.exit(1)
}

mkdirSync(OUT, { recursive: true })

/**
 * How many disconnected solids a binary STL contains.
 *
 * This exists because "manifold, genus 2" is not the same as "one object", and
 * I trusted it as though it were. The pod base exported as a pod and a separate
 * loose rectangular ring, and every geometric property OpenSCAD prints was
 * happy about it. Union-find over welded vertices is the cheap way to ask the
 * question that actually matters for something being printed.
 */
function countShells(path) {
  const buf = readFileSync(path)
  const count = buf.readUInt32LE(80)
  const parent = new Map()
  const find = (x) => {
    while (parent.get(x) !== x) {
      parent.set(x, parent.get(parent.get(x)))
      x = parent.get(x)
    }
    return x
  }
  const union = (a, b) => {
    const ra = find(a)
    const rb = find(b)
    if (ra !== rb) parent.set(ra, rb)
  }
  const key = (o) =>
    `${buf.readFloatLE(o).toFixed(4)},${buf.readFloatLE(o + 4).toFixed(4)},` +
    `${buf.readFloatLE(o + 8).toFixed(4)}`

  for (let i = 0; i < count; i++) {
    const base = 84 + i * 50 + 12
    const vs = [key(base), key(base + 12), key(base + 24)]
    for (const v of vs) if (!parent.has(v)) parent.set(v, v)
    union(vs[0], vs[1])
    union(vs[1], vs[2])
  }
  return new Set([...parent.keys()].map(find)).size
}

// Rendered from an isometric angle that shows the cavity and one strap slot,
// because the preview exists to let somebody recognise the part rather than to
// be pretty.
const CAMERA = '0,0,0,62,0,32,0'

for (const part of mech.parts) {
  const scad = join(SRC, part.source)
  const stl = join(OUT, `oapogee-${part.id.replace(/_/g, '-')}.stl`)
  const png = join(OUT, `oapogee-${part.id.replace(/_/g, '-')}.png`)

  for (const [flag, out] of [
    ['--export-format=binstl', stl],
    [null, png],
  ]) {
    const args = ['--hardwarnings', '-o', out]
    if (flag) args.push(flag)
    else args.push('--imgsize=1100,750', '--viewall', '--autocenter', '--colorscheme=Tomorrow Night',
                   `--camera=${CAMERA}`)
    args.push(scad)
    try {
      execFileSync(openscad, args, { stdio: ['ignore', 'ignore', 'pipe'] })
    } catch (e) {
      console.error(`${part.source} failed to render:\n${e.stderr?.toString() ?? e.message}`)
      process.exit(1)
    }
  }
  const shells = countShells(stl)
  if (shells !== 1) {
    console.error(
      `${part.source} exported ${shells} disconnected surfaces. A printed part is one.\n` +
        `Either something is floating free of the body, or a pocket is fully enclosed by\n` +
        `material with no opening. Both are defects and both look fine to every other\n` +
        `check: OpenSCAD calls this manifold and gives it a plausible genus, because\n` +
        `neither of those says anything about whether the pieces touch or the holes open.`
    )
    process.exit(1)
  }
  console.log(`${part.id}: ${part.source} -> ${stl.slice(ROOT.length)} (1 solid)`)
}

// Every .scad in the directory, including the ones that are only included, so
// editing common.scad or params.scad counts as a change.
const sources = readdirSync(SRC)
  .filter((f) => f.endsWith('.scad'))
  .sort()
const manifest = {
  note:
    'Written by tools/render-mechanical.mjs. Verified by tools/gen-mechanical.mjs --check, ' +
    'which needs no OpenSCAD. If this check fails, run `make mech` and commit the result.',
  sources: Object.fromEntries(
    sources.map((f) => [f, createHash('sha256').update(readFileSync(join(SRC, f))).digest('hex')])
  ),
}
writeFileSync(join(SRC, 'rendered.json'), JSON.stringify(manifest, null, 2) + '\n')

const provisional = mech.params.filter((p) => p.provenance === 'provisional')
console.log(
  `\n${mech.parts.length} parts rendered. ${provisional.length} of the ${mech.params.length} ` +
    `dimensions they are built from are provisional:\n  ` +
    provisional.map((p) => p.id).join(', ') +
    `\nA part printed from these is a fit check, not a finished component.`
)

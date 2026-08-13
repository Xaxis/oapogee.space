#!/usr/bin/env node
//
// Enforces the mechanical rules in CONTENT-STYLE.md that a human reviewer will
// not reliably catch on the fortieth page.
//
// Two of these are style. One of them, the name check, is not: shortening
// oApogee to "Apogee" collides with Apogee Components, who own that word in
// model rocketry completely, and a stray "the Apogee board" in a heading is
// both confusing to readers and an invitation to trademark friction. That is
// worth a build failure.

import { readFileSync, readdirSync, statSync } from 'node:fs'
import { join, relative, extname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const SCAN_DIRS = ['content', 'data', 'docs']
const ALSO = ['CONTENT-STYLE.md', 'CONTRIBUTING.md', 'README.md']
const SCAN_EXT = new Set(['.md', '.yaml', '.yml'])
const SKIP_DIRS = new Set(['node_modules', '.git', '.next'])

const RULES = [
  {
    id: 'em-dash',
    // U+2014 em dash and U+2013 en dash used as a sentence break.
    test: (line) => /[—]/.test(line) || /\s[–]\s/.test(line),
    message: 'em dash or spaced en dash. Use a comma, a colon, parentheses, or two sentences.',
  },
  {
    id: 'emoji',
    test: (line) => /\p{Extended_Pictographic}/u.test(line),
    message: 'emoji in content. Not permitted in body content.',
  },
  {
    id: 'name-misspelling',
    test: (line) => /\bOApogee\b|\bOpenApogee\b|\bOpen Apogee\b|\bo-Apogee\b|\bO-Apogee\b/.test(line),
    message: 'misspelled project name. It is always written oApogee.',
  },
  {
    id: 'name-shortened',
    // The bare word is legitimate when it means the flight event, and when it
    // names Apogee Components. It is not legitimate as a stand-in for the
    // project, which in practice means immediately before a project noun.
    test: (line) =>
      /\bApogee\s+(board|firmware|payload|kit|Solo|Link|Track|project|site|docs|team)\b/.test(line),
    message:
      'the project name shortened to "Apogee". That word belongs to Apogee Components in this ' +
      'hobby. There is no short form: write oApogee.',
  },
]

// Fenced code blocks and inline code are exempt: a code sample may legitimately
// contain any of these, and a URL containing a dash is not prose.
function* proseLines(text) {
  let inFence = false
  const lines = text.split('\n')
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    if (/^\s*```/.test(line)) {
      inFence = !inFence
      continue
    }
    if (inFence) continue
    yield [i + 1, line.replace(/`[^`]*`/g, '').replace(/https?:\/\/\S+/g, '')]
  }
}

function walk(dir, acc = []) {
  let entries
  try {
    entries = readdirSync(dir)
  } catch {
    return acc
  }
  for (const entry of entries) {
    if (SKIP_DIRS.has(entry)) continue
    const full = join(dir, entry)
    if (statSync(full).isDirectory()) walk(full, acc)
    else if (SCAN_EXT.has(extname(full))) acc.push(full)
  }
  return acc
}

const files = [...SCAN_DIRS.flatMap((d) => walk(join(ROOT, d))), ...ALSO.map((f) => join(ROOT, f))]

let failures = 0
for (const file of files) {
  let text
  try {
    text = readFileSync(file, 'utf8')
  } catch {
    continue
  }
  // This file describes the forbidden constructs, so it cannot obey them.
  if (file.endsWith('CONTENT-STYLE.md')) continue
  for (const [lineNo, line] of proseLines(text)) {
    for (const rule of RULES) {
      if (rule.test(line)) {
        console.error(`${relative(ROOT, file)}:${lineNo}  [${rule.id}] ${rule.message}`)
        failures++
      }
    }
  }
}

// --- never publish a number that has not been measured or sourced ------------

/**
 * The rule the whole project rests on, which until now was enforced by
 * remembering it. It was not remembered. "Around 16 g" and "anything above a C
 * motor will saturate an IMU" sat on the Reading your data page naming exactly
 * the two figures the bill of materials refuses to state until they are
 * derived, which is the specific failure the rule exists to prevent.
 *
 * A regex cannot tell a sourced number from a guess, so this does what
 * check-firmware does with tuning constants: it flags every physical quantity
 * in published prose and requires each one to be listed below with the reason
 * it is allowed. The list is meant to be argued with, and adding to it should
 * feel like a small act of accountability, because that is the point.
 *
 * Exempt: any TODO(...) paragraph. Those exist to name the figure that is
 * missing and the evidence that would close it, so a target quoted there as a
 * target is the mechanism working rather than a breach of it.
 */
const SOURCED = [
  {
    values: ['902 MHz', '928 MHz', '863 MHz', '870 MHz', '868 MHz'],
    why: 'Radio band edges. Regulatory definitions, with the primary source linked where stated.',
  },
  {
    values: ['70 cm', '2 m'],
    why: 'Amateur band names rather than measurements.',
  },
  {
    values: ['101325 Pa'],
    why:
      'Standard sea level pressure, ISA. A defined constant, not a measurement. It replaced a ' +
      '"95000 to 103000 Pa" plausibility band that this allowlist had waved through: standard ' +
      'atmosphere is already below 95000 Pa at 550 m, so the band called a correct sensor faulty ' +
      'at most of the launch sites in Colorado, New Mexico and Utah. Allowlisting a number is ' +
      'supposed to mean somebody checked it.',
  },
  {
    values: ['24 mm'],
    why: 'Nominal model rocket body tube and coupler size.',
  },
  {
    values: ['$60', '$30'],
    why:
      'Targets from the project brief, quoted on the FAQ explicitly as targets and disclaimed in ' +
      'the same sentence as not measurements. Sourced to the brief, not to a cart.',
  },
  {
    values: ['5 V'],
    why: 'USB bus voltage. A fixed property of the connector standard.',
  },
  {
    values: ['0.4 mm', '3.6 mm', '2.0 mm', '0.8 mm', '0.1 g'],
    why:
      'Sizes of things that are not this payload: a printer nozzle, a cable tie, the JST-PH ' +
      'pitch, solder wire, and the resolution of a scale a builder needs. Standards and tool ' +
      'requirements, not measurements of oApogee.',
  },
  {
    values: ['3 mm'],
    why:
      'Named in data/mechanical.yaml only as the example of a static port diameter that must ' +
      'NOT be defaulted to. It appears in the sentence refusing to publish one.',
  },
  {
    values: ['250 mAh'],
    why:
      'The capacity range the cell is being sourced against, from the project brief. Not a ' +
      'measurement: the verify note on that part requires it be re-derived from measured ' +
      'current draw per tier, and until then this is a shopping constraint rather than a spec.',
  },
  {
    values: ['10 minutes'],
    why: 'Reading time in frontmatter: an editorial estimate, not a measurement of hardware.',
  },
]
const allowed = new Map()
for (const entry of SOURCED) for (const v of entry.values) allowed.set(v, entry.why)

const UNITS =
  'kg|mAh|mA|MHz|kHz|GHz|Hz|dBm|mm|cm|km|Pa|g|m|V|minutes|hours|seconds|metres|feet|ft'
const QUANTITY = new RegExp(String.raw`(?<![\w.])(\d+(?:[.,]\d+)?)\s*(${UNITS})\b`, 'g')
const MONEY = /\$\d+(?:\.\d+)?/g

for (const file of files) {
  // content/ and data/ both render on the site. Scoping this to content/ only
  // is exactly how the glossary kept publishing an IMU full-scale range and a
  // motor class after the prose page that quoted them had been corrected.
  const where = relative(ROOT, file)
  if (!where.startsWith('content/') && !where.startsWith('data/')) continue
  let text
  try {
    text = readFileSync(file, 'utf8')
  } catch {
    continue
  }

  // TODO paragraphs run from the marker to the next blank line.
  const raw = text.split('\n')
  const inTodo = new Array(raw.length).fill(false)
  let open = false
  raw.forEach((line, i) => {
    if (/TODO\((verify|confirm|confirm-on-hardware|photo)\)/.test(line)) open = true
    else if (!line.trim()) open = false
    inTodo[i] = open
  })

  for (const [lineNo, line] of proseLines(text)) {
    if (inTodo[lineNo - 1]) continue
    const hits = [
      ...[...line.matchAll(QUANTITY)].map((m) => `${m[1]} ${m[2]}`),
      ...(line.match(MONEY) ?? []),
    ]
    for (const hit of hits) {
      if (allowed.has(hit)) continue
      console.error(
        `${relative(ROOT, file)}:${lineNo}  [measured-number] "${hit}" is a physical quantity in ` +
          `published prose. If it was measured or sourced, add it to SOURCED in ` +
          `tools/check-prose.mjs with where it came from. If it was not, it belongs in a ` +
          `TODO(verify) saying what evidence would close it.`
      )
      failures++
    }
  }
}


if (failures) {
  console.error(`\n${failures} prose issue${failures === 1 ? '' : 's'}.`)
  process.exit(1)
}
console.log(`Prose rules hold across ${files.length} files.`)

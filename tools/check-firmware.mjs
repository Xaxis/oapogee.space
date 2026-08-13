#!/usr/bin/env node
/**
 * Two claims the firmware makes about itself, checked rather than promised.
 *
 * 1. THE PASSIVE BOUNDARY. oApogee does not fire ejection charges, control
 *    deployment, ignite motors, or command any pyrotechnic device. The whole
 *    project rests on that, and a comment saying so is worth nothing. What is
 *    worth something is that the set of physical outputs the firmware may drive
 *    is written down in one place, in plain text, and that the code cannot
 *    quietly grow a third one.
 *
 * 2. NO INVENTED NUMBERS. Every flight threshold is unmeasured, so it lives in
 *    configuration as unset and the payload refuses to arm without it. A
 *    threshold that appears as a literal in a source file is that rule being
 *    broken silently, and it is the single most likely way this project ships a
 *    guess.
 *
 * The second rule needs judgement, so this makes the judgement explicit rather
 * than pretending a regex can tell a threshold from a field width. It applies
 * only to the modules where a tuning value could plausibly hide, and it allows a
 * documented set of values that are structure rather than tuning. Both lists are
 * below and both are meant to be argued with.
 */

import { readFileSync, readdirSync, existsSync } from 'node:fs'
import { join, basename } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const FW = join(ROOT, 'firmware')

const problems = []
const fail = (msg) => problems.push(msg)

if (!existsSync(FW)) {
  console.error('firmware/ does not exist')
  process.exit(1)
}

// --- 1. the passive boundary ------------------------------------------------

/**
 * Words that must never appear as an identifier anywhere under firmware/.
 *
 * Deliberately checked against identifiers rather than against all text: the
 * safety documentation has to be able to say the word "pyrotechnic" in order to
 * say the firmware does not command one.
 */
const FORBIDDEN = [
  'ejection',
  'eject',
  'igniter',
  'ignite',
  'pyro',
  'deploy',
  'charge_fire',
  'squib',
  'ematch',
]

function sources(dir, acc = []) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === 'build' || entry.name.startsWith('build-') || entry.name === 'vendor') {
      continue
    }
    const full = join(dir, entry.name)
    if (entry.isDirectory()) sources(full, acc)
    else if (/\.(c|h|def)$/.test(entry.name)) acc.push(full)
  }
  return acc
}

const files = sources(FW)

for (const file of files) {
  const rel = file.slice(ROOT.length)
  const lines = readFileSync(file, 'utf8').split('\n')
  let inBlockComment = false

  lines.forEach((line, i) => {
    // Strip comments before looking for identifiers, so prose about the boundary
    // does not trip the check that enforces it.
    let code = line
    if (inBlockComment) {
      const end = code.indexOf('*/')
      if (end === -1) return
      code = code.slice(end + 2)
      inBlockComment = false
    }
    const open = code.indexOf('/*')
    if (open !== -1) {
      const end = code.indexOf('*/', open + 2)
      if (end === -1) {
        code = code.slice(0, open)
        inBlockComment = true
      } else {
        code = code.slice(0, open) + code.slice(end + 2)
      }
    }
    code = code.replace(/\/\/.*$/, '').replace(/"(?:[^"\\]|\\.)*"/g, '""')

    // Compare whole identifier components, not substrings. "rejected" contains
    // "eject" and is an ordinary word about a sample that failed a plausibility
    // check, so substring matching flags most of the barometer module and the
    // rule gets switched off, which is worse than not having it.
    for (const ident of code.match(/[A-Za-z_][A-Za-z0-9_]*/g) ?? []) {
      const parts = ident
        .replace(/([a-z0-9])([A-Z])/g, '$1_$2')
        .toLowerCase()
        .split('_')
        .filter(Boolean)
      const hit = FORBIDDEN.find((w) => parts.includes(w))
      if (hit) {
        fail(
          `${rel}:${i + 1} identifier "${ident}" names "${hit}", which suggests a pyrotechnic ` +
            `output. oApogee is a passive instrumentation payload and there is no code path ` +
            `that may drive one.`
        )
      }
    }
  })
}

// --- the output allowlist ---------------------------------------------------

const OUT_HEADER = join(FW, 'port/include/oapogee_port/oa_out.h')
const ALLOWLIST = join(FW, 'port/outputs.allowlist')

if (!existsSync(OUT_HEADER) || !existsSync(ALLOWLIST)) {
  fail('the output header or its allowlist is missing; the passive boundary is unenforced')
} else {
  const header = readFileSync(OUT_HEADER, 'utf8')
  const listed = readFileSync(ALLOWLIST, 'utf8')
    .split('\n')
    .map((l) => l.replace(/#.*$/, '').trim())
    .filter(Boolean)
    .map((l) => l.split(/\s+/)[0])

  // Every enumerator in the header must be named in the allowlist and vice
  // versa. The allowlist is the artifact a reviewer reads to confirm the
  // boundary without reading any C, so it has to be exhaustive to be worth
  // anything.
  // Only enumerators, taken from the enum body. Matching OA_OUT_ anywhere in
  // the file also catches the include guard, OAPOGEE_PORT_OA_OUT_H, and the
  // count macro, neither of which is a physical output.
  const enumBody = /typedef\s+enum\s*\{([\s\S]*?)\}\s*oa_out_t/.exec(header)?.[1] ?? ''
  const declared = [...enumBody.matchAll(/\b(OA_OUT_[A-Z0-9_]+)\b/g)]
    .map((m) => m[1])
    .filter((n) => n !== 'OA_OUT_COUNT' && n !== 'OA_OUT_NONE')

  if (!enumBody) fail('could not find the oa_out_t enum in oa_out.h; the boundary is unchecked')

  const uniqueDeclared = [...new Set(declared)]
  for (const name of uniqueDeclared) {
    if (!listed.includes(name)) {
      fail(
        `firmware/port/outputs.allowlist does not list ${name}, which oa_out.h declares. ` +
          `A new physical output must be justified in the allowlist before the code may drive it.`
      )
    }
  }
  for (const name of listed) {
    if (!uniqueDeclared.includes(name)) {
      fail(`firmware/port/outputs.allowlist lists ${name}, which oa_out.h does not declare`)
    }
  }
  if (uniqueDeclared.length > 2) {
    fail(
      `oa_out.h declares ${uniqueDeclared.length} outputs (${uniqueDeclared.join(', ')}). ` +
        `The payload drives a buzzer and a status LED, and nothing else. Adding a third is a ` +
        `change to what this project is, not a change to its code.`
    )
  }
}

// --- 2. no invented numbers -------------------------------------------------

/**
 * The modules where a tuning value could plausibly hide. Encoders and packers
 * are full of legitimate constants, which is why they are not listed: the rule
 * is meant to be enforceable rather than broad.
 */
const TUNING_SENSITIVE = ['oa_state.c', 'oa_sched.c', 'oa_fusion.c', 'oa_health.c', 'oa_baro.c']

/**
 * Values that are structure rather than tuning.
 *
 * 0 and 1 are identity and unity. 2 through 8 are axis counts, array sizes and
 * shift amounts. 10, 100 and 1000 are unit conversions between the scaled
 * integers the formats already fix. 1024 and 100000 are fixed-point scales.
 * Anything else in these files has to justify itself, and the way to justify a
 * threshold is to move it into configuration where it can be unset.
 */
const ALLOWED = new Set([
  0, 1, 2, 3, 4, 5, 6, 7, 8, // identity, axis counts, array sizes, shift amounts
  10, 100, 1000, 100000, // unit conversions between the scaled integers the formats fix
  16, 29, 30, 31, 32, 64, // fixed-point shifts, including Q30 and its half-LSB rounding term
  255, 1024,
])

for (const file of files) {
  if (!TUNING_SENSITIVE.includes(basename(file))) continue
  const rel = file.slice(ROOT.length)
  const text = readFileSync(file, 'utf8')

  // Comments and strings stripped: a comment citing the standard atmosphere is
  // exactly the documentation this rule wants to encourage.
  // Block comments are replaced by their own newlines rather than removed, so a
  // reported line number still points at the line it came from. Collapsing them
  // silently shifts every later report, which sends somebody to the wrong line
  // and makes the whole check untrustworthy.
  const code = text
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, ' '))
    .replace(/\/\/.*$/gm, '')
    .replace(/"(?:[^"\\]|\\.)*"/g, '""')

  // A named, commented lookup table is a named constant, and its entries are
  // its contents. The barometer's fixed-point logarithm is a table of log2
  // terms, each annotated with the term it represents; flagging every entry
  // would say the mathematics should have been measured.
  let inTable = false

  code.split('\n').forEach((line, i) => {
    if (inTable) {
      if (/\}\s*;/.test(line)) inTable = false
      return
    }
    if (/\bstatic\s+const\b[^;]*\[\s*\w*\s*\]\s*=\s*\{/.test(line)) {
      // Single-line tables close on the same line.
      if (!/\}\s*;/.test(line)) inTable = true
      return
    }
    // Skip preprocessor lines: a #define is where a named constant belongs, and
    // naming it is the behaviour this rule is trying to produce.
    if (/^\s*#/.test(line)) return

    // Skip static assertions. A _Static_assert comparing a named constant
    // against the value it is supposed to have is the opposite of a hidden
    // number: it is the check that the named constant is right, and it fails
    // the build if somebody edits the definition. The barometer asserts its ISA
    // scale height this way, which is exactly the practice this rule wants.
    if (/_Static_assert/.test(line)) return

    // Hex counts too. The scan matched decimal only, so a threshold written as
    // 0x1F walked past the one rule that exists to stop a tuning constant being
    // buried in a source file.
    for (const m of line.matchAll(/(?<![\w.])(0[xX][0-9a-fA-F]+|\d+)(?![\w.])/g)) {
      const value = Number(m[1])
      if (ALLOWED.has(value)) continue
      fail(
        `${rel}:${i + 1} literal ${value} in a tuning-sensitive module. ` +
          `If it is a threshold, it belongs in configuration as unset, so the payload refuses ` +
          `to arm rather than flying a guess. If it is structure, name it in a #define or add ` +
          `it to the allowlist in tools/check-firmware.mjs with the reason.`
      )
    }
  })
}

// --- 3. contracts that a return type cannot express -------------------------

/**
 * oa_packet_encode_pad_pressure returns a uint16_t, so it has no way to report
 * an error, and its out-parameter is the only channel by which a caller learns
 * the pad reference was clamped. The header says that parameter "may not be
 * NULL". The implementation guards against NULL anyway, because a null
 * dereference on a flight computer is worse than a lost flag, and the two
 * together mean a caller can silently transmit a clamped reference pressure
 * with no fault raised. Neither the compiler nor the type can catch that, so
 * the call sites are checked instead.
 */
for (const file of files) {
  if (!file.endsWith('.c')) continue
  // Tests are exempt, and firmware/test/test_oa_packet.c relies on it: proving
  // the guard holds means calling it with NULL on purpose. The hazard is a
  // caller on the transmit path, not a test exercising the boundary.
  if (file.includes('/test/')) continue
  const rel = file.slice(ROOT.length)
  readFileSync(file, 'utf8')
    .split('\n')
    .forEach((line, i) => {
      const call = /oa_packet_encode_pad_pressure\s*\(([^;]*)\)/.exec(line)
      if (!call || /^uint16_t/.test(line.trim())) return
      if (/,\s*NULL\s*\)?\s*$/.test(call[1]) || /,\s*NULL\s*[,)]/.test(call[1])) {
        fail(
          `${rel}:${i + 1} passes NULL as out_baro_fault. That is the only signal that the pad ` +
            `reference was clamped, and a packet built from a clamped reference with no ` +
            `BARO_FAULT flag is a wrong altitude that looks correct.`
        )
      }
    })
}

// --- report -----------------------------------------------------------------

if (problems.length) {
  for (const p of problems) console.error(`error ${p}`)
  console.error(`\n${problems.length} firmware policy violation${problems.length === 1 ? '' : 's'}.`)
  process.exit(1)
}

console.log(
  `Firmware policy holds: the passive boundary is intact across ${files.length} sources, ` +
    `and no tuning constant is hardcoded.`
)

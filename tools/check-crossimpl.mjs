#!/usr/bin/env node
/**
 * The firmware and the browser must encode the same packet identically.
 *
 * There are two implementations of docs/spec/telemetry-packet.md in this
 * repository: the C one under firmware/, and the TypeScript one in
 * apps/web/lib/packet.ts that the ground station receiver and the decoder on the
 * specification page run on. They were written from the same document by
 * different hands, and neither was written from the other.
 *
 * That makes them worth comparing. Two independent readings that produce
 * identical bytes is the strongest evidence available that the specification is
 * unambiguous, which is the entire point of publishing one. A difference is a
 * defect in the document even when both implementations look correct on their
 * own, because two careful readers reached different conclusions from the same
 * words.
 *
 * A conformance test written against either implementation cannot find this
 * class of defect, because a misreading of the spec is faithfully reproduced by
 * the test that was written from the same misreading.
 *
 *   node tools/check-crossimpl.mjs
 */

import { execFileSync } from 'node:child_process'
import { existsSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))

const BINARY = [
  join(ROOT, 'firmware/build/test/oa_vectors'),
  join(ROOT, 'firmware/build/oa_vectors'),
].find((p) => existsSync(p))

if (!BINARY) {
  console.error('firmware vector dumper not built. Run `make fw-build` first.')
  process.exit(1)
}

const { PacketType, FLAG, NO_FIX, encode, toHex } = await import(
  pathToFileURL(join(ROOT, 'apps/web/lib/packet.ts')).href
)

/**
 * The same inputs the C dumper uses, expressed through the TypeScript API.
 *
 * Kept as a literal table rather than generated from anything, because the whole
 * value of this check is that the two sides were arrived at independently. A
 * shared source of inputs would be one more place a single misreading could hide.
 */
const VECTORS = {
  status_pad: {
    type: PacketType.STATUS,
    seq: 12,
    state: 0,
    // Deliberately non-zero: the spec requires 0 on the air while PAD_IDLE, and
    // an implementation that forwards the uptime differs here rather than
    // somewhere subtler.
    tMs: 987654,
    body: { padPressurePa: 101325, battVolts: 4.08 },
  },
  status_pressure_low: {
    type: PacketType.STATUS,
    seq: 12,
    state: 0,
    body: { padPressurePa: 50000, battVolts: 2.5 },
  },
  status_pressure_high: {
    type: PacketType.STATUS,
    seq: 12,
    state: 0,
    body: { padPressurePa: 115535, battVolts: 5.05 },
  },
  status_pressure_under: {
    type: PacketType.STATUS,
    seq: 12,
    state: 0,
    body: { padPressurePa: 49999, battVolts: 3.5 },
  },
  status_pressure_over: {
    type: PacketType.STATUS,
    seq: 12,
    state: 0,
    body: { padPressurePa: 115536, battVolts: 3.5 },
  },
  flight_boost: {
    type: PacketType.FLIGHT,
    flags: FLAG.HIGH_G,
    seq: 88,
    state: 2,
    tMs: 1430,
    body: { altCm: 8250, velDmS: 1420, accelCg: 2340, battVolts: 3.94 },
  },
  flight_negative_altitude: {
    type: PacketType.FLIGHT,
    flags: FLAG.HIGH_G,
    seq: 88,
    state: 6,
    tMs: 214000,
    body: { altCm: -12345, velDmS: -400, accelCg: -100, battVolts: 3.94 },
  },
  flight_seq_max: {
    type: PacketType.FLIGHT,
    flags: FLAG.HIGH_G,
    seq: 255,
    state: 6,
    tMs: 214000,
    body: { altCm: -12345, velDmS: -400, accelCg: -100, battVolts: 3.94 },
  },
  flight_seq_wrapped: {
    type: PacketType.FLIGHT,
    flags: FLAG.HIGH_G,
    seq: 0,
    state: 6,
    tMs: 214000,
    body: { altCm: -12345, velDmS: -400, accelCg: -100, battVolts: 3.94 },
  },
  // Reserved bit 7 must be masked off by a conforming encoder. Until this
  // vector existed nothing set a flag bit above SIM.
  flight_all_flag_bits: {
    type: PacketType.FLIGHT,
    flags: 0xff,
    seq: 88,
    state: 6,
    tMs: 214000,
    body: { altCm: -12345, velDmS: -400, accelCg: -100, battVolts: 3.94 },
  },
  apogee: {
    type: PacketType.APOGEE,
    seq: 91,
    state: 4,
    tMs: 9180,
    body: { apogeeCm: 40500, tApogeeMs: 9040 },
  },
  beacon_no_fix: {
    type: PacketType.BEACON,
    seq: 3,
    state: 6,
    tMs: 240000,
    body: { latE7: NO_FIX, lonE7: NO_FIX, apogeeCm: 40500 },
  },
  beacon_fix: {
    type: PacketType.BEACON,
    flags: FLAG.GNSS_FIX,
    seq: 3,
    state: 6,
    tMs: 240000,
    body: { latE7: 512345678, lonE7: -1234567, apogeeCm: 40500 },
  },
  // Coordinates handed in with GNSS_FIX clear. The flag governs, so both
  // encoders must emit the sentinel and ignore the body.
  beacon_coords_without_fix: {
    type: PacketType.BEACON,
    flags: 0,
    seq: 3,
    state: 6,
    tMs: 240000,
    body: { latE7: 515000000, lonE7: -1270000000, apogeeCm: 40500 },
  },
  position_fix: {
    type: PacketType.POSITION,
    flags: FLAG.GNSS_FIX,
    seq: 40,
    state: 3,
    tMs: 5000,
    body: { latE7: 512345678, lonE7: -1234567, sats: 9 },
  },
  position_no_fix: {
    type: PacketType.POSITION,
    seq: 40,
    state: 3,
    tMs: 5000,
    body: { latE7: NO_FIX, lonE7: NO_FIX, sats: 0 },
  },
}

const fromC = new Map()
for (const line of execFileSync(BINARY, { encoding: 'utf8' }).trim().split('\n')) {
  const [name, hex] = line.trim().split(/\s+/)
  if (name && hex) fromC.set(name, hex.toLowerCase())
}

const problems = []
let compared = 0

for (const [name, input] of Object.entries(VECTORS)) {
  const c = fromC.get(name)
  if (!c) {
    problems.push(`${name}: the firmware did not emit this vector`)
    continue
  }
  const ts = toHex(encode(input).packet).replace(/ /g, '').toLowerCase()
  compared++

  if (ts !== c) {
    // Name the first differing byte. "these two strings differ" sends somebody
    // diffing 44 characters by eye; an offset points at the field.
    let at = 0
    while (at < Math.min(c.length, ts.length) && c[at] === ts[at]) at++
    const byte = Math.floor(at / 2)
    problems.push(
      `${name}: first differs at byte ${byte}\n` +
        `        firmware   ${c}\n` +
        `        typescript ${ts}\n` +
        `        ${' '.repeat(byte * 2)}^^`
    )
  }
}

for (const name of fromC.keys()) {
  if (!(name in VECTORS)) {
    problems.push(`${name}: the firmware emits this vector and the TypeScript side has no case`)
  }
}

if (problems.length) {
  console.error('The two implementations of the packet specification disagree.\n')
  for (const p of problems) console.error(`error ${p}`)
  console.error(
    '\nBefore changing either implementation, read the specification and decide ' +
      'which reading it actually supports. If both are defensible, the document ' +
      'is ambiguous and that is what needs fixing.'
  )
  process.exit(1)
}

console.log(
  `The firmware and the browser encode ${compared} packets identically, byte for byte.`
)

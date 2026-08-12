/**
 * The oApogee telemetry packet format, in TypeScript.
 *
 * This is a second, independent implementation of
 * docs/spec/telemetry-packet.md. The firmware carries the first, in C. Two
 * implementations written from the same document, by different hands, that agree
 * byte for byte is the strongest evidence available that the specification is
 * unambiguous, which matters because the whole point of publishing a wire format
 * is that somebody else can implement it without asking us anything.
 *
 * It also does real work: the ground station receiver and the decoder on the
 * spec page both run on it.
 *
 * The spec is normative. Where this file and the spec disagree, this file is
 * wrong. Every constant here is an encoding constant taken from the spec, not a
 * tuning value: there are no thresholds in this file and there must never be.
 */

export const SPEC_VERSION = 1

export const PacketType = {
  STATUS: 0x1,
  FLIGHT: 0x2,
  APOGEE: 0x3,
  BEACON: 0x4,
  POSITION: 0x5,
} as const

export type PacketTypeName = keyof typeof PacketType

export const TYPE_NAME: Record<number, PacketTypeName> = {
  1: 'STATUS',
  2: 'FLIGHT',
  3: 'APOGEE',
  4: 'BEACON',
  5: 'POSITION',
}

/** Body length by type, from the packet type table. Total is 8 + body + 2. */
export const BODY_LEN: Record<number, number> = { 1: 3, 2: 9, 3: 8, 4: 12, 5: 9 }

export const HEADER_LEN = 8
export const CRC_LEN = 2

export const totalLen = (type: number) => HEADER_LEN + BODY_LEN[type] + CRC_LEN

export const STATE_NAME = [
  'PAD_IDLE',
  'ARMED',
  'BOOST',
  'COAST',
  'APOGEE',
  'DESCENT',
  'LANDED',
] as const

export const FLAG = {
  GNSS_FIX: 0x01,
  HIGH_G: 0x02,
  BARO_FAULT: 0x04,
  IMU_FAULT: 0x08,
  LOG_FULL: 0x10,
  LOW_BATT: 0x20,
  SIM: 0x40,
} as const

/** The no-fix sentinel for latitude and longitude. Zero is a real coordinate. */
export const NO_FIX = -2147483648 // INT32_MIN

// Encoding constants, all from the spec.
const PRESSURE_OFFSET_PA = 50000
const PRESSURE_MAX_PA = 115535
const BATTERY_OFFSET_V = 2.5
const BATTERY_STEP_V = 0.01

/**
 * CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection, no
 * final XOR. Bitwise rather than table-driven so it is small enough to read
 * against the spec's Python side by side.
 */
export function crc16(data: Uint8Array): number {
  let crc = 0xffff
  for (const byte of data) {
    crc ^= byte << 8
    for (let i = 0; i < 8; i++) {
      crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
    }
  }
  return crc
}

export const batteryVolts = (raw: number) => BATTERY_OFFSET_V + raw * BATTERY_STEP_V

/** Inverse of the transmitted encoding, with both clamps applied by the sender. */
export const padPressurePa = (raw: number) => PRESSURE_OFFSET_PA + raw

export type Decoded = {
  version: number
  type: PacketTypeName
  typeValue: number
  flags: number
  flagNames: string[]
  seq: number
  state: number
  stateName: string
  tMs: number
  fields: { name: string; raw: number | string; value: string }[]
}

export class PacketError extends Error {}

/**
 * Decode one packet.
 *
 * Follows the conformance rules in the spec: reject an unrecognised version
 * rather than guessing at offsets, treat an unknown type as unreadable rather
 * than misparsing it, verify the CRC, and never clamp altitude at zero.
 */
export function decode(packet: Uint8Array): Decoded {
  if (packet.length < HEADER_LEN + CRC_LEN) {
    throw new PacketError(`short packet: ${packet.length} bytes`)
  }

  const version = packet[0] >> 4
  const typeValue = packet[0] & 0x0f

  if (version !== SPEC_VERSION) {
    throw new PacketError(
      `unsupported format version ${version}. Field offsets are not guaranteed stable across versions, so this cannot be decoded.`
    )
  }
  if (!(typeValue in BODY_LEN)) {
    throw new PacketError(`unknown packet type 0x${typeValue.toString(16)}`)
  }

  const expected = totalLen(typeValue)
  if (packet.length !== expected) {
    throw new PacketError(`expected ${expected} bytes for this type, got ${packet.length}`)
  }

  const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength)
  const received = view.getUint16(packet.length - CRC_LEN, true)
  const computed = crc16(packet.subarray(0, packet.length - CRC_LEN))
  if (received !== computed) {
    throw new PacketError(
      `CRC mismatch: packet carries 0x${received.toString(16).padStart(4, '0')}, computed 0x${computed
        .toString(16)
        .padStart(4, '0')}`
    )
  }

  const flags = packet[1]
  const state = packet[3]
  const fields: Decoded['fields'] = []

  const push = (name: string, raw: number | string, value: string) =>
    fields.push({ name, raw, value })

  switch (typeValue) {
    case PacketType.STATUS: {
      const raw = view.getUint16(8, true)
      push('pad_pressure_pa_off', raw, `${padPressurePa(raw)} Pa`)
      push('batt', packet[10], `${batteryVolts(packet[10]).toFixed(2)} V`)
      break
    }
    case PacketType.FLIGHT: {
      const alt = view.getInt32(8, true)
      const vel = view.getInt16(12, true)
      const acc = view.getInt16(14, true)
      push('alt_cm', alt, `${(alt / 100).toFixed(2)} m`)
      push('vel_dm_s', vel, `${(vel / 10).toFixed(1)} m/s`)
      push('accel_cg', acc, `${(acc / 100).toFixed(2)} g`)
      push('batt', packet[16], `${batteryVolts(packet[16]).toFixed(2)} V`)
      break
    }
    case PacketType.APOGEE: {
      const apogee = view.getInt32(8, true)
      const t = view.getUint32(12, true)
      push('apogee_cm', apogee, `${(apogee / 100).toFixed(2)} m`)
      push('t_apogee_ms', t, `${(t / 1000).toFixed(3)} s since arming`)
      break
    }
    case PacketType.BEACON: {
      const lat = view.getInt32(8, true)
      const lon = view.getInt32(12, true)
      const apogee = view.getInt32(16, true)
      push('lat_e7', lat, lat === NO_FIX ? 'no fix' : `${(lat / 1e7).toFixed(7)} deg`)
      push('lon_e7', lon, lon === NO_FIX ? 'no fix' : `${(lon / 1e7).toFixed(7)} deg`)
      push('apogee_cm', apogee, `${(apogee / 100).toFixed(2)} m`)
      break
    }
    case PacketType.POSITION: {
      const lat = view.getInt32(8, true)
      const lon = view.getInt32(12, true)
      push('lat_e7', lat, lat === NO_FIX ? 'no fix' : `${(lat / 1e7).toFixed(7)} deg`)
      push('lon_e7', lon, lon === NO_FIX ? 'no fix' : `${(lon / 1e7).toFixed(7)} deg`)
      push('sats', packet[16], `${packet[16]} satellites`)
      break
    }
  }

  return {
    version,
    type: TYPE_NAME[typeValue],
    typeValue,
    flags,
    flagNames: Object.entries(FLAG)
      .filter(([, mask]) => flags & mask)
      .map(([name]) => name),
    seq: packet[2],
    state,
    stateName: STATE_NAME[state] ?? `reserved (${state})`,
    // Transmitted as 0 while PAD_IDLE, because before arming there is no
    // elapsed time to report.
    tMs: view.getUint32(4, true),
    fields,
  }
}

export type EncodeInput = {
  type: number
  flags?: number
  seq?: number
  state?: number
  tMs?: number
  body?: Partial<{
    padPressurePa: number
    battVolts: number
    altCm: number
    velDmS: number
    accelCg: number
    apogeeCm: number
    tApogeeMs: number
    latE7: number
    lonE7: number
    sats: number
  }>
}

const clamp = (v: number, lo: number, hi: number) => Math.min(Math.max(Math.round(v), lo), hi)

/**
 * Encode one packet.
 *
 * Present so the tests can round-trip and so the site can offer a sample of
 * every type without hand-assembling bytes. Returns the flags actually used,
 * because clamping the pressure raises BARO_FAULT and the caller has to be able
 * to see that happen.
 */
export function encode(input: EncodeInput): { packet: Uint8Array; flags: number } {
  const type = input.type
  if (!(type in BODY_LEN)) throw new PacketError(`unknown packet type ${type}`)

  const packet = new Uint8Array(totalLen(type))
  const view = new DataView(packet.buffer)
  const b = input.body ?? {}
  let flags = input.flags ?? 0

  const state = input.state ?? 0
  packet[0] = (SPEC_VERSION << 4) | type
  packet[2] = (input.seq ?? 0) & 0xff
  packet[3] = state
  // The spec requires 0 on the pad rather than an uptime, which is a firmware
  // implementation detail and is never on the air.
  view.setUint32(4, state === 0 ? 0 : (input.tMs ?? 0) >>> 0, true)

  const battByte = (volts: number) => clamp((volts - BATTERY_OFFSET_V) / BATTERY_STEP_V, 0, 255)

  switch (type) {
    case PacketType.STATUS: {
      const pa = b.padPressurePa ?? PRESSURE_OFFSET_PA
      if (pa < PRESSURE_OFFSET_PA || pa > PRESSURE_MAX_PA) flags |= FLAG.BARO_FAULT
      view.setUint16(8, clamp(pa - PRESSURE_OFFSET_PA, 0, 65535), true)
      packet[10] = battByte(b.battVolts ?? BATTERY_OFFSET_V)
      break
    }
    case PacketType.FLIGHT: {
      view.setInt32(8, b.altCm ?? 0, true)
      view.setInt16(12, clamp(b.velDmS ?? 0, -32768, 32767), true)
      view.setInt16(14, clamp(b.accelCg ?? 0, -32768, 32767), true)
      packet[16] = battByte(b.battVolts ?? BATTERY_OFFSET_V)
      break
    }
    case PacketType.APOGEE: {
      view.setInt32(8, b.apogeeCm ?? 0, true)
      view.setUint32(12, (b.tApogeeMs ?? 0) >>> 0, true)
      break
    }
    case PacketType.BEACON: {
      view.setInt32(8, b.latE7 ?? NO_FIX, true)
      view.setInt32(12, b.lonE7 ?? NO_FIX, true)
      view.setInt32(16, b.apogeeCm ?? 0, true)
      break
    }
    case PacketType.POSITION: {
      const hasFix = (flags & FLAG.GNSS_FIX) !== 0
      view.setInt32(8, hasFix ? (b.latE7 ?? 0) : NO_FIX, true)
      view.setInt32(12, hasFix ? (b.lonE7 ?? 0) : NO_FIX, true)
      packet[16] = hasFix ? clamp(b.sats ?? 0, 0, 255) : 0
      break
    }
  }

  packet[1] = flags
  view.setUint16(packet.length - CRC_LEN, crc16(packet.subarray(0, packet.length - CRC_LEN)), true)
  return { packet, flags }
}

export const toHex = (packet: Uint8Array) =>
  [...packet].map((b) => b.toString(16).padStart(2, '0')).join(' ')

/** Accepts hex with or without separators, and the 0x prefixes people paste. */
export function fromHex(text: string): Uint8Array {
  const cleaned = text.replace(/0x/gi, '').replace(/[^0-9a-f]/gi, '')
  if (cleaned.length === 0) throw new PacketError('no hex digits found')
  if (cleaned.length % 2 !== 0) throw new PacketError('odd number of hex digits')
  const out = new Uint8Array(cleaned.length / 2)
  for (let i = 0; i < out.length; i++) out[i] = parseInt(cleaned.slice(i * 2, i * 2 + 2), 16)
  return out
}

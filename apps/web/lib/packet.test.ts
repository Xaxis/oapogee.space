import { test } from 'node:test'
import assert from 'node:assert/strict'
import {
  BODY_LEN,
  FLAG,
  NO_FIX,
  PacketError,
  PacketType,
  SPEC_VERSION,
  crc16,
  decode,
  encode,
  fromHex,
  totalLen,
} from './packet.ts'

// Conformance tests for docs/spec/telemetry-packet.md.
//
// Each test names the specific claim it checks. A test that only round-trips
// this file's own encoder against its own decoder would pass with both sides
// wrong in the same way, so the byte-level assertions below are written from
// the spec's tables rather than from the code.

test('the packet type table: body and total lengths', () => {
  // Spec, "Packet types": STATUS 3/13, FLIGHT 9/19, APOGEE 8/18, BEACON 12/22,
  // POSITION 9/19.
  assert.deepEqual(BODY_LEN, { 1: 3, 2: 9, 3: 8, 4: 12, 5: 9 })
  assert.equal(totalLen(PacketType.STATUS), 13)
  assert.equal(totalLen(PacketType.FLIGHT), 19)
  assert.equal(totalLen(PacketType.APOGEE), 18)
  assert.equal(totalLen(PacketType.BEACON), 22)
  assert.equal(totalLen(PacketType.POSITION), 19)
})

test('CRC-16/CCITT-FALSE matches its published check value', () => {
  // The standard check vector for this parameterisation: "123456789" gives
  // 0x29B1. If this fails, every packet on the air is wrong.
  assert.equal(crc16(new TextEncoder().encode('123456789')), 0x29b1)
})

test('the header is laid out as the spec says', () => {
  const { packet } = encode({
    type: PacketType.FLIGHT,
    flags: FLAG.HIGH_G,
    seq: 200,
    state: 3,
    tMs: 0x01020304,
  })

  // Spec, "Header, 8 bytes".
  assert.equal(packet[0] >> 4, SPEC_VERSION, 'version in the high nibble')
  assert.equal(packet[0] & 0x0f, PacketType.FLIGHT, 'type in the low nibble')
  assert.equal(packet[0], 0x12, 'version 1 with a FLIGHT packet is 0x12')
  assert.equal(packet[1], FLAG.HIGH_G, 'flags at offset 1')
  assert.equal(packet[2], 200, 'seq at offset 2')
  assert.equal(packet[3], 3, 'state at offset 3')
  // Little-endian, from "Conventions".
  assert.deepEqual([...packet.subarray(4, 8)], [0x04, 0x03, 0x02, 0x01])
})

test('seq wraps at 255 rather than widening', () => {
  const { packet } = encode({ type: PacketType.APOGEE, seq: 256, state: 4 })
  assert.equal(packet[2], 0)
})

test('t_ms is transmitted as 0 while the state is PAD_IDLE', () => {
  // Before arming there is no elapsed time to report, and the uptime counter is
  // a firmware detail that is never on the air.
  const { packet } = encode({ type: PacketType.STATUS, state: 0, tMs: 999999 })
  assert.equal(decode(packet).tMs, 0)
})

test('FLIGHT field offsets and scalings', () => {
  const { packet } = encode({
    type: PacketType.FLIGHT,
    state: 2,
    tMs: 1000,
    body: { altCm: -12345, velDmS: -400, accelCg: 1600, battVolts: 3.7 },
  })
  const view = new DataView(packet.buffer)

  assert.equal(view.getInt32(8, true), -12345, 'alt_cm at 8')
  assert.equal(view.getInt16(12, true), -400, 'vel_dm_s at 12')
  assert.equal(view.getInt16(14, true), 1600, 'accel_cg at 14')
  assert.equal(packet[16], 120, 'batt at 16, (3.7 - 2.5) / 0.01')

  const d = decode(packet)
  // Negative altitude is legitimate and must survive: the pad reference can be
  // above where the rocket lands.
  assert.equal(d.fields[0].value, '-123.45 m')
  assert.equal(d.fields[1].value, '-40.0 m/s')
  assert.equal(d.fields[2].value, '16.00 g')
  assert.equal(d.fields[3].value, '3.70 V')
})

test('the pressure encoding covers exactly 50000 to 115535 Pa', () => {
  // Spec: value is pressure_pa - 50000, and 65536 codes cover the band.
  const low = encode({ type: PacketType.STATUS, body: { padPressurePa: 50000 } })
  assert.equal(new DataView(low.packet.buffer).getUint16(8, true), 0)
  assert.equal(low.flags & FLAG.BARO_FAULT, 0, 'the low endpoint is in range')

  const high = encode({ type: PacketType.STATUS, body: { padPressurePa: 115535 } })
  assert.equal(new DataView(high.packet.buffer).getUint16(8, true), 65535)
  assert.equal(high.flags & FLAG.BARO_FAULT, 0, 'the high endpoint is in range')

  assert.equal(decode(low.packet).fields[0].value, '50000 Pa')
  assert.equal(decode(high.packet).fields[0].value, '115535 Pa')
})

test('a pressure outside the band clamps and raises BARO_FAULT', () => {
  const under = encode({ type: PacketType.STATUS, body: { padPressurePa: 49999 } })
  assert.equal(new DataView(under.packet.buffer).getUint16(8, true), 0)
  assert.ok(under.flags & FLAG.BARO_FAULT)

  const over = encode({ type: PacketType.STATUS, body: { padPressurePa: 115536 } })
  assert.equal(new DataView(over.packet.buffer).getUint16(8, true), 65535)
  assert.ok(over.flags & FLAG.BARO_FAULT)
})

test('the battery byte clamps at both ends of its range', () => {
  const flat = encode({ type: PacketType.FLIGHT, body: { battVolts: 1.0 } })
  assert.equal(flat.packet[16], 0, '2.50 V is the floor')
  const usb = encode({ type: PacketType.FLIGHT, body: { battVolts: 9.9 } })
  assert.equal(usb.packet[16], 255, '5.05 V is the ceiling')
})

test('BEACON transmits INT32_MIN for position when there is no fix', () => {
  // Zero is a real coordinate, which is why the sentinel is not zero.
  const { packet } = encode({ type: PacketType.BEACON, state: 6, body: { apogeeCm: 15000 } })
  const view = new DataView(packet.buffer)
  assert.equal(view.getInt32(8, true), NO_FIX)
  assert.equal(view.getInt32(12, true), NO_FIX)
  assert.equal(view.getInt32(16, true), 15000, 'apogee_cm at 16')

  const d = decode(packet)
  assert.equal(d.fields[0].value, 'no fix')
  assert.equal(d.fields[2].value, '150.00 m')
})

test('POSITION reports no fix and zero satellites when GNSS_FIX is clear', () => {
  const { packet } = encode({
    type: PacketType.POSITION,
    state: 3,
    body: { latE7: 512345678, lonE7: -1234567, sats: 9 },
  })
  const view = new DataView(packet.buffer)
  assert.equal(view.getInt32(8, true), NO_FIX, 'the flag governs, not the supplied value')
  assert.equal(packet[16], 0)
})

test('POSITION carries the fix when GNSS_FIX is set', () => {
  const { packet } = encode({
    type: PacketType.POSITION,
    state: 3,
    flags: FLAG.GNSS_FIX,
    body: { latE7: 512345678, lonE7: -1234567, sats: 9 },
  })
  const d = decode(packet)
  assert.equal(d.fields[0].value, '51.2345678 deg')
  assert.equal(d.fields[1].value, '-0.1234567 deg')
  assert.equal(d.fields[2].value, '9 satellites')
  assert.ok(d.flagNames.includes('GNSS_FIX'))
})

test('every packet type round-trips through an independent read', () => {
  for (const type of Object.values(PacketType)) {
    const { packet } = encode({ type, seq: 7, state: 2, tMs: 4242 })
    const d = decode(packet)
    assert.equal(d.typeValue, type)
    assert.equal(d.seq, 7)
    assert.equal(d.tMs, 4242)
    assert.equal(packet.length, totalLen(type))
  }
})

test('a corrupted byte fails the CRC', () => {
  const { packet } = encode({ type: PacketType.FLIGHT, state: 2, body: { altCm: 1000 } })
  packet[9] ^= 0x01
  assert.throws(() => decode(packet), PacketError)
})

test('an unrecognised version is rejected rather than decoded', () => {
  // Conformance: field offsets are not guaranteed stable across versions.
  const { packet } = encode({ type: PacketType.FLIGHT })
  packet[0] = (2 << 4) | PacketType.FLIGHT
  assert.throws(() => decode(packet), /unsupported format version 2/)
})

test('an unknown packet type is unreadable rather than misparsed', () => {
  const { packet } = encode({ type: PacketType.FLIGHT })
  packet[0] = (SPEC_VERSION << 4) | 0x7
  assert.throws(() => decode(packet), /unknown packet type/)
})

test('an unknown state is reported rather than treated as an error', () => {
  const { packet } = encode({ type: PacketType.FLIGHT, state: 200 })
  assert.equal(decode(packet).stateName, 'reserved (200)')
})

test('hex input tolerates the separators people actually paste', () => {
  const { packet } = encode({ type: PacketType.APOGEE, state: 4, body: { apogeeCm: 12345 } })
  const spaced = [...packet].map((b) => b.toString(16).padStart(2, '0')).join(' ')
  const runOn = spaced.replace(/ /g, '')
  const prefixed = [...packet].map((b) => '0x' + b.toString(16).padStart(2, '0')).join(', ')
  for (const text of [spaced, runOn, prefixed, spaced.toUpperCase()]) {
    assert.deepEqual([...fromHex(text)], [...packet])
  }
})

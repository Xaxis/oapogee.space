import { FLAG, PacketType, decode, encode, fromHex, type Decoded } from './packet.ts'

/**
 * The line protocol between a ground station module and its host.
 *
 * Specified on the ground station page. One line per received radio frame:
 *
 *   OA1 <hex> rssi=<dBm> snr=<dB>
 *
 * A line protocol rather than a binary one so that a serial terminal, a browser
 * and a shell script all read exactly the same thing. That property is why the
 * terminal fallback is a real fallback rather than a degraded mode.
 */

export type Line = {
  hex: string
  rssi?: number
  snr?: number
  /** Everything after the hex, for keys this parser does not know yet. */
  meta: Record<string, string>
}

export type Frame = {
  at: number
  line: Line
  decoded?: Decoded
  /** Why the packet was rejected. Rejected frames are kept: they are link data. */
  error?: string
}

/**
 * Parse one line. Returns null for anything that is not a packet line, because
 * boot banners and diagnostics share the port and must not confuse a reader.
 */
export function parseLine(raw: string): Line | null {
  const text = raw.trim()
  if (!text.startsWith('OA1 ')) return null

  const parts = text.slice(4).trim().split(/\s+/)
  const hex = parts.shift()
  if (!hex) return null

  const meta: Record<string, string> = {}
  for (const part of parts) {
    const eq = part.indexOf('=')
    if (eq > 0) meta[part.slice(0, eq)] = part.slice(eq + 1)
  }

  const num = (key: string) => {
    const v = Number(meta[key])
    return Number.isFinite(v) ? v : undefined
  }

  return { hex, rssi: num('rssi'), snr: num('snr'), meta }
}

export function frameFrom(line: Line, at: number): Frame {
  try {
    return { at, line, decoded: decode(fromHex(line.hex)) }
  } catch (e) {
    // A frame that fails is still evidence about the link, so it is kept and
    // counted rather than dropped. Dropping it would make the link look better
    // than it is.
    return { at, line, error: e instanceof Error ? e.message : String(e) }
  }
}

/** Splits a byte stream into lines, holding a partial line across reads. */
export class LineSplitter {
  private buffer = ''

  push(chunk: string): string[] {
    this.buffer += chunk
    const lines = this.buffer.split(/\r?\n/)
    this.buffer = lines.pop() ?? ''
    return lines
  }
}

export type LinkStats = {
  received: number
  decoded: number
  rejected: number
  /** Gaps inferred from the sequence number, which wraps at 255. */
  lost: number
}

export function accumulate(stats: LinkStats, frame: Frame, lastSeq: number | null): number | null {
  stats.received++
  if (!frame.decoded) {
    stats.rejected++
    return lastSeq
  }
  stats.decoded++
  const seq = frame.decoded.seq
  if (lastSeq !== null) {
    // Unsigned distance around the wrap. A gap of 1 is the next packet.
    const gap = (seq - lastSeq + 256) % 256
    if (gap > 1) stats.lost += gap - 1
  }
  return seq
}

/**
 * A synthetic flight, so the receiver page works before any hardware exists.
 *
 * Every packet it produces sets the SIM flag, which is exactly what that flag
 * is for: a bench run produces complete, plausible packets, and without the flag
 * an archive eventually publishes one as a flight. The receiver shows the flag
 * prominently and refuses to call it a flight.
 *
 * The shape is the flight state machine, not a measurement. No oApogee has
 * flown, so there is nothing to replay. The altitudes here are a curve chosen to
 * exercise every packet type and every state, and the page says so.
 */
export function* simulatedFlight(): Generator<string> {
  const SIM = FLAG.SIM
  let seq = 0
  const next = () => seq++ & 0xff
  const line = (packet: Uint8Array, rssi: number, snr: number) =>
    `OA1 ${[...packet].map((b) => b.toString(16).padStart(2, '0')).join('')} rssi=${rssi} snr=${snr.toFixed(1)}`

  // On the pad. t_ms is 0 throughout, per the packet spec.
  for (let i = 0; i < 4; i++) {
    yield line(
      encode({
        type: PacketType.STATUS,
        state: 0,
        seq: next(),
        flags: SIM,
        body: { padPressurePa: 101_180, battVolts: 4.08 },
      }).packet,
      -61,
      9.5
    )
  }

  // Altitude is accumulated in centimetres, the unit the packet carries, and
  // velocity in decimetres per second, likewise. Mixing the two is the first bug
  // this simulation had: it reported an apogee ten times too low, which looked
  // plausible enough to publish.
  let t = 0
  let altCm = 0
  let velDmS = 0

  const step = (state: number, dtMs: number, accelCg: number, rssi: number) => {
    t += dtMs
    altCm += (velDmS * dtMs) / 100 // dm/s over ms, into cm
    return line(
      encode({
        type: PacketType.FLIGHT,
        state,
        seq: next(),
        tMs: t,
        flags: SIM | FLAG.HIGH_G,
        body: { altCm: Math.round(Math.max(altCm, 0)), velDmS: Math.round(velDmS), accelCg, battVolts: 4.02 },
      }).packet,
      rssi,
      8
    )
  }

  // Boost: about a second and a half of thrust to roughly 90 m/s.
  for (let i = 0; i < 6; i++) {
    velDmS += 150
    yield step(2, 250, 2200 - i * 90, -64 - i)
  }

  // Coast: decelerating under gravity and drag until vertical velocity reaches
  // zero. Most of the altitude on a low power flight is gained here, which is
  // the thing this animation is worth showing.
  while (velDmS > 0) {
    velDmS -= 30
    yield step(3, 250, -120, -70 - Math.min(18, Math.floor(t / 500)))
  }

  const apogeeCm = Math.round(altCm)
  yield line(
    encode({
      type: PacketType.APOGEE,
      state: 4,
      seq: next(),
      tMs: t + 120,
      flags: SIM,
      body: { apogeeCm, tApogeeMs: t },
    }).packet,
    -88,
    4.5
  )

  // Descent under recovery, at a slower packet rate. That rate drop is in the
  // specification rather than a shortcut here: a descent contains far less
  // information per second than boost did, and the battery saved is battery the
  // recovery beacon needs afterwards.
  velDmS = -150
  while (altCm > 0) {
    yield step(5, 1000, 0, -90 + Math.floor(altCm / 4000))
  }

  // One frame arrives corrupted, because that is what a real link does and a
  // receiver that has never shown a rejection has not been tested.
  const bad = encode({
    type: PacketType.FLIGHT,
    state: 5,
    seq: next(),
    tMs: t + 500,
    flags: SIM,
    body: { altCm: 400, velDmS: -150, accelCg: 0, battVolts: 4.0 },
  }).packet
  bad[10] ^= 0x20
  yield line(bad, -97, 1.5)

  for (let i = 0; i < 3; i++) {
    yield line(
      encode({
        type: PacketType.BEACON,
        state: 6,
        seq: next(),
        tMs: t + 20_000 + i * 15_000,
        flags: SIM,
        body: { apogeeCm },
      }).packet,
      -101,
      -2
    )
  }
}

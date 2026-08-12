import { test } from 'node:test'
import assert from 'node:assert/strict'
import { decode, fromHex, FLAG } from './packet.ts'
import {
  LineSplitter,
  accumulate,
  frameFrom,
  parseLine,
  simulatedFlight,
  type LinkStats,
} from './hostlink.ts'

// Conformance tests for the host link protocol specified on the ground station
// page: OA1 <hex> rssi=<dBm> snr=<dB>, one line per received radio frame.

test('a packet line parses into its hex and its metadata', () => {
  const line = parseLine('OA1 1200580200000000 rssi=-92 snr=7.5')
  assert.ok(line)
  assert.equal(line.hex, '1200580200000000')
  assert.equal(line.rssi, -92)
  assert.equal(line.snr, 7.5)
})

test('anything not starting with OA1 is ignored', () => {
  // Boot banners and diagnostics share the port and must not confuse a reader.
  for (const noise of [
    'oApogee ground station v0.1',
    'radio: SX1262 ready',
    '',
    '   ',
    'OA2 deadbeef',
  ]) {
    assert.equal(parseLine(noise), null, `should ignore: ${noise}`)
  }
})

test('unknown metadata keys are kept rather than dropped', () => {
  // So a module that learns to report something new does not need this parser
  // updated before its output is usable.
  const line = parseLine('OA1 abcd rssi=-70 snr=9 freq=915000000 fei=-142')
  assert.equal(line?.meta.freq, '915000000')
  assert.equal(line?.meta.fei, '-142')
})

test('a line with no metadata is still a valid line', () => {
  const line = parseLine('OA1 1200580200000000')
  assert.ok(line)
  assert.equal(line.rssi, undefined)
})

test('a frame that fails to decode is kept, not discarded', () => {
  // A corrupted frame is evidence about the link. Dropping it silently makes
  // the link look better than it is.
  const frame = frameFrom({ hex: 'deadbeef', meta: {} }, 0)
  assert.equal(frame.decoded, undefined)
  assert.ok(frame.error)
  assert.equal(frame.line.hex, 'deadbeef')
})

test('the splitter holds a partial line across reads', () => {
  const s = new LineSplitter()
  assert.deepEqual(s.push('OA1 aa\nOA1 bb\nOA1 c'), ['OA1 aa', 'OA1 bb'])
  assert.deepEqual(s.push('c\n'), ['OA1 cc'])
})

test('the splitter handles carriage returns', () => {
  const s = new LineSplitter()
  assert.deepEqual(s.push('OA1 aa\r\nOA1 bb\r\n'), ['OA1 aa', 'OA1 bb'])
})

test('sequence gaps are counted across the wrap at 255', () => {
  const stats: LinkStats = { received: 0, decoded: 0, rejected: 0, lost: 0 }
  const at = (seq: number) => ({
    at: 0,
    line: { hex: '', meta: {} },
    decoded: { seq } as never,
  })

  let last: number | null = null
  for (const seq of [254, 255, 2]) {
    // 254 then 255 are consecutive; 2 means 0 and 1 were lost, across the wrap.
    last = accumulate(stats, at(seq), last)
  }
  assert.equal(last, 2)
  assert.equal(stats.lost, 2)
  assert.equal(stats.decoded, 3)
})

test('a rejected frame counts as received but not as decoded', () => {
  const stats: LinkStats = { received: 0, decoded: 0, rejected: 0, lost: 0 }
  accumulate(stats, frameFrom({ hex: 'deadbeef', meta: {} }, 0), null)
  assert.deepEqual(stats, { received: 1, decoded: 0, rejected: 1, lost: 0 })
})

test('the simulated flight exercises every packet type and every state', () => {
  const types = new Set<string>()
  const states = new Set<string>()
  let apogeeM: number | null = null
  let maxAltCm = 0
  let rejected = 0

  for (const raw of simulatedFlight()) {
    const line = parseLine(raw)
    assert.ok(line, 'every emitted line must parse')
    const frame = frameFrom(line, 0)
    if (!frame.decoded) {
      rejected++
      continue
    }
    types.add(frame.decoded.type)
    states.add(frame.decoded.stateName)

    // Every simulated packet must carry SIM. Without it an archive eventually
    // publishes a bench run as a flight, which is the whole reason the flag
    // exists.
    assert.ok(
      frame.decoded.flagNames.includes('SIM'),
      `packet ${frame.decoded.type} is missing the SIM flag`
    )

    if (frame.decoded.type === 'APOGEE') {
      apogeeM = Number(frame.decoded.fields[0].raw) / 100
    }
    const alt = frame.decoded.fields.find((f) => f.name === 'alt_cm')
    if (alt) maxAltCm = Math.max(maxAltCm, Number(alt.raw))
  }

  assert.deepEqual([...types].sort(), ['APOGEE', 'BEACON', 'FLIGHT', 'STATUS'])
  assert.deepEqual(
    [...states].sort(),
    ['APOGEE', 'BOOST', 'COAST', 'DESCENT', 'LANDED', 'PAD_IDLE']
  )

  // Exactly one corrupted frame, on purpose: a receiver that has never shown a
  // rejection has not been tested.
  assert.equal(rejected, 1)

  // The reported apogee must match the highest altitude actually flown. A
  // simulation whose summary disagrees with its own samples would be teaching
  // the wrong thing on the page that explains how to read a flight.
  assert.ok(apogeeM !== null)
  assert.equal(Math.round(apogeeM! * 100), maxAltCm)
})

test('the simulated flight is physically coherent', () => {
  // Not a claim about any real rocket. It only has to be a curve that does not
  // teach somebody something false about how a flight looks.
  const alts: number[] = []
  for (const raw of simulatedFlight()) {
    const line = parseLine(raw)!
    try {
      const d = decode(fromHex(line.hex))
      const alt = d.fields.find((f) => f.name === 'alt_cm')
      if (alt && d.stateName !== 'PAD_IDLE') alts.push(Number(alt.raw))
    } catch {
      // the deliberate corrupt frame
    }
  }

  const peak = Math.max(...alts)
  const peakAt = alts.indexOf(peak)
  assert.ok(peak > 10_000, 'should reach a plausible low power altitude')
  assert.ok(peakAt > 0 && peakAt < alts.length - 1, 'apogee is not at either end')

  // Monotonic up to apogee, monotonic down after it.
  for (let i = 1; i <= peakAt; i++) assert.ok(alts[i] >= alts[i - 1], `climb broken at ${i}`)
  for (let i = peakAt + 1; i < alts.length; i++) {
    assert.ok(alts[i] <= alts[i - 1], `descent broken at ${i}`)
  }
})

test('every simulated line survives a round trip through the splitter', () => {
  // The receiver reads a byte stream, not an array of lines, so the two have to
  // agree about where a line ends.
  const s = new LineSplitter()
  const all = [...simulatedFlight()]
  const stream = all.join('\n') + '\n'
  const out: string[] = []
  // Split at an awkward boundary to make sure partial lines are held.
  for (let i = 0; i < stream.length; i += 7) out.push(...s.push(stream.slice(i, i + 7)))
  assert.deepEqual(out, all)
})

test('the flags in every simulated packet are ones the format defines', () => {
  const known = Object.values(FLAG).reduce((a, b) => a | b, 0)
  for (const raw of simulatedFlight()) {
    const line = parseLine(raw)!
    try {
      const d = decode(fromHex(line.hex))
      assert.equal(d.flags & ~known, 0, 'no reserved bit is set')
    } catch {
      // the deliberate corrupt frame
    }
  }
})

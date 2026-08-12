'use client'

import { useMemo, useState } from 'react'
import {
  FLAG,
  PacketType,
  decode,
  encode,
  fromHex,
  toHex,
  type Decoded,
} from '@/lib/packet'

/**
 * A live decoder for the packet format, on the page that specifies it.
 *
 * A wire format that invites third parties to implement it should let them
 * check their work without cloning anything. Paste the bytes your encoder
 * produced and see whether this reads them the way the specification says, or
 * take one of ours and check your decoder against it.
 *
 * It runs the same module the ground station receiver uses, which is a second
 * independent implementation of the specification from the one in the firmware.
 * If the two ever disagree, the specification is ambiguous and that is a defect
 * in the document rather than in either implementation.
 */

const SAMPLES: { label: string; hex: string; note: string }[] = [
  {
    label: 'STATUS on the pad',
    note: 'Waiting to be armed. t_ms is 0 because before arming there is no elapsed time to report.',
    hex: toHex(
      encode({
        type: PacketType.STATUS,
        state: 0,
        seq: 12,
        body: { padPressurePa: 101325, battVolts: 4.05 },
      }).packet
    ),
  },
  {
    label: 'FLIGHT during boost',
    note: 'Climbing hard. The acceleration here is what a high-g part is for.',
    hex: toHex(
      encode({
        type: PacketType.FLIGHT,
        state: 2,
        seq: 88,
        tMs: 1430,
        flags: FLAG.HIGH_G,
        body: { altCm: 8250, velDmS: 1420, accelCg: 2340, battVolts: 3.94 },
      }).packet
    ),
  },
  {
    label: 'APOGEE',
    note: 'Sent the instant apogee is detected, ahead of anything queued. The event time is earlier than the packet time, and that difference is the detection lag.',
    hex: toHex(
      encode({
        type: PacketType.APOGEE,
        state: 4,
        seq: 91,
        tMs: 9180,
        body: { apogeeCm: 31460, tApogeeMs: 9040 },
      }).packet
    ),
  },
  {
    label: 'BEACON, no fix',
    note: 'After landing, on a build with no GNSS. Both coordinates carry the sentinel, because zero is a real place.',
    hex: toHex(
      encode({
        type: PacketType.BEACON,
        state: 6,
        seq: 3,
        tMs: 214000,
        body: { apogeeCm: 31460 },
      }).packet
    ),
  },
  {
    label: 'Corrupted',
    note: 'The same boost packet with one bit flipped. A conforming receiver discards it and counts the discard rather than showing a plausible wrong altitude.',
    hex: (() => {
      const { packet } = encode({
        type: PacketType.FLIGHT,
        state: 2,
        seq: 88,
        tMs: 1430,
        body: { altCm: 8250, velDmS: 1420, accelCg: 2340, battVolts: 3.94 },
      })
      packet[9] ^= 0x08
      return toHex(packet)
    })(),
  },
]

export function PacketDecoder() {
  const [text, setText] = useState(SAMPLES[1].hex)
  const [note, setNote] = useState(SAMPLES[1].note)

  const result = useMemo<{ decoded?: Decoded; error?: string; bytes?: number }>(() => {
    try {
      const bytes = fromHex(text)
      return { decoded: decode(bytes), bytes: bytes.length }
    } catch (e) {
      return { error: e instanceof Error ? e.message : String(e) }
    }
  }, [text])

  return (
    <section className="no-print mt-16 max-w-[46rem]">
      <h2 className="text-2xl font-semibold text-white">Try it</h2>
      <p className="mt-3 text-[var(--color-muted)]">
        Paste bytes your encoder produced and see whether they read the way this document says, or
        take one of these and check your decoder against it. This runs the same module the ground
        station receiver uses, which is a second implementation of this specification, independent
        of the one in the firmware.
      </p>

      <div className="mt-5 flex flex-wrap gap-2">
        {SAMPLES.map((s) => (
          <button
            key={s.label}
            type="button"
            onClick={() => {
              setText(s.hex)
              setNote(s.note)
            }}
            className={`rounded border px-3 py-1.5 text-sm transition-colors ${
              text === s.hex
                ? 'border-[var(--color-hivis)] text-[var(--color-hivis)]'
                : 'border-[var(--color-line-bright)] text-[var(--color-muted)] hover:border-[var(--color-body)] hover:text-white'
            }`}
          >
            {s.label}
          </button>
        ))}
      </div>

      <label className="mt-5 block">
        <span className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
          Packet bytes, hex
        </span>
        <textarea
          value={text}
          onChange={(e) => {
            setText(e.target.value)
            setNote('')
          }}
          spellCheck={false}
          rows={3}
          className="mt-2 w-full rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-3 font-mono text-sm text-[var(--color-body)] outline-none focus:border-[var(--color-hivis)]"
        />
      </label>

      {note && <p className="mt-2 text-sm text-[var(--color-muted)]">{note}</p>}

      {result.error && (
        <div className="mt-5 rounded-lg border-l-3 border-[var(--color-alert)] bg-[var(--color-surface)] p-4">
          <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-alert)]">
            Rejected
          </div>
          <p className="mt-2 text-sm text-[var(--color-body)]">{result.error}</p>
          <p className="mt-2 text-sm text-[var(--color-muted)]">
            That is the correct outcome, not a failure of the tool. A conforming receiver discards
            this packet and counts the discard.
          </p>
        </div>
      )}

      {result.decoded && (
        <div className="mt-5 overflow-hidden rounded-lg border border-[var(--color-line)]">
          <div className="flex flex-wrap items-center gap-2 border-b border-[var(--color-line)] bg-[var(--color-surface)] p-4">
            <span className="chip chip-verified">{result.decoded.type}</span>
            <span className="chip">v{result.decoded.version}</span>
            <span className="chip">{result.bytes} bytes</span>
            <span className="chip">seq {result.decoded.seq}</span>
            <span className="chip">{result.decoded.stateName}</span>
            {result.decoded.flagNames.map((f) => (
              <span key={f} className="chip chip-draft">
                {f}
              </span>
            ))}
          </div>

          <table className="data">
            <thead>
              <tr>
                <th>Field</th>
                <th>Raw</th>
                <th>Value</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td className="font-mono text-xs text-[var(--color-body)]">t_ms</td>
                <td className="font-mono text-xs text-[var(--color-muted)]">
                  {result.decoded.tMs}
                </td>
                <td className="text-[var(--color-muted)]">
                  {result.decoded.stateName === 'PAD_IDLE'
                    ? 'not armed yet'
                    : `${(result.decoded.tMs / 1000).toFixed(3)} s since arming`}
                </td>
              </tr>
              {result.decoded.fields.map((f) => (
                <tr key={f.name}>
                  <td className="font-mono text-xs text-[var(--color-body)]">{f.name}</td>
                  <td className="font-mono text-xs text-[var(--color-muted)]">{f.raw}</td>
                  <td className="text-[var(--color-muted)]">{f.value}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      <p className="mt-4 font-mono text-xs text-[var(--color-dim)]">
        Decoded in your browser. Nothing is uploaded.
      </p>
    </section>
  )
}

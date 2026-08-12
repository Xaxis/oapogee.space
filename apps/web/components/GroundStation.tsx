'use client'

import { useCallback, useEffect, useRef, useState } from 'react'
import {
  LineSplitter,
  accumulate,
  frameFrom,
  parseLine,
  simulatedFlight,
  type Frame,
  type LinkStats,
} from '@/lib/hostlink'

/**
 * The ground station receiver, in the browser.
 *
 * Runs the same packet module as the decoder on the specification page, over
 * the line protocol specified above. Nothing is installed and nothing is
 * uploaded: the serial port is read in the page.
 *
 * It ships with a simulated flight, because no hardware exists and a receiver
 * nobody can run is a receiver nobody reviews. Every simulated packet sets the
 * SIM flag, and the display refuses to call the result a flight while that flag
 * is set. That is exactly what the flag is for.
 */

type Support = 'unknown' | 'yes' | 'no'

const fmt = (n: number | undefined, digits: number, unit: string) =>
  n === undefined ? '.' : `${n.toFixed(digits)} ${unit}`

export function GroundStation() {
  const [support, setSupport] = useState<Support>('unknown')
  const [connected, setConnected] = useState(false)
  const [simulating, setSimulating] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [frames, setFrames] = useState<Frame[]>([])
  const [stats, setStats] = useState<LinkStats>({ received: 0, decoded: 0, rejected: 0, lost: 0 })

  const lastSeq = useRef<number | null>(null)
  const portRef = useRef<{ close: () => Promise<void> } | null>(null)
  const stopSim = useRef<(() => void) | null>(null)

  useEffect(() => {
    setSupport('serial' in navigator ? 'yes' : 'no')
    return () => {
      stopSim.current?.()
      portRef.current?.close().catch(() => {})
    }
  }, [])

  const ingest = useCallback((raw: string) => {
    const line = parseLine(raw)
    if (!line) return
    const frame = frameFrom(line, Date.now())
    setStats((prev) => {
      const next = { ...prev }
      lastSeq.current = accumulate(next, frame, lastSeq.current)
      return next
    })
    // Newest first, and bounded: a long flight would otherwise grow the DOM
    // until the page stutters at exactly the moment somebody is watching it.
    setFrames((prev) => [frame, ...prev].slice(0, 200))
  }, [])

  const reset = () => {
    setFrames([])
    setStats({ received: 0, decoded: 0, rejected: 0, lost: 0 })
    lastSeq.current = null
    setError(null)
  }

  const connect = async () => {
    reset()
    try {
      // Typed loosely: WebSerial is not in the DOM lib everywhere, and adding a
      // dependency for three call sites is not worth it.
      const serial = (navigator as unknown as { serial: { requestPort(): Promise<unknown> } }).serial
      const port = (await serial.requestPort()) as {
        open(o: { baudRate: number }): Promise<void>
        readable: ReadableStream<Uint8Array>
        close(): Promise<void>
      }
      // TODO(verify): the baud rate is a guess until the receiver firmware sets
      // one. It is here rather than in the spec for that reason.
      await port.open({ baudRate: 115200 })
      portRef.current = port
      setConnected(true)

      const splitter = new LineSplitter()
      const decoder = new TextDecoder()
      const reader = port.readable.getReader()

      const pump = async () => {
        try {
          for (;;) {
            const { value, done } = await reader.read()
            if (done) break
            for (const line of splitter.push(decoder.decode(value, { stream: true }))) ingest(line)
          }
        } catch (e) {
          setError(e instanceof Error ? e.message : String(e))
        } finally {
          reader.releaseLock()
          setConnected(false)
        }
      }
      void pump()
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
      setConnected(false)
    }
  }

  const simulate = () => {
    reset()
    setSimulating(true)
    const gen = simulatedFlight()
    // Paced rather than dumped, because the point is to show what watching a
    // flight is like, and a wall of packets shows nothing.
    const timer = setInterval(() => {
      const next = gen.next()
      if (next.done) {
        clearInterval(timer)
        setSimulating(false)
        return
      }
      ingest(next.value)
    }, 220)
    stopSim.current = () => {
      clearInterval(timer)
      setSimulating(false)
    }
  }

  const latest = frames.find((f) => f.decoded)
  const apogee = frames.find(
    (f) => f.decoded?.type === 'APOGEE' || f.decoded?.type === 'BEACON'
  )?.decoded
  const isSim = frames.some((f) => f.decoded?.flagNames.includes('SIM'))

  const altitude = latest?.decoded?.fields.find((f) => f.name === 'alt_cm')
  const apogeeField = apogee?.fields.find((f) => f.name === 'apogee_cm')

  return (
    <section className="no-print mt-16">
      <h2 className="text-2xl font-semibold text-white">Receiver</h2>
      <p className="mt-3 max-w-2xl text-[var(--color-muted)]">
        Connect a ground station over USB and watch the flight here. It runs the same packet module
        as the decoder on the specification page, reads the line protocol above, and uploads
        nothing.
      </p>

      {support === 'no' && (
        <div className="mt-5 max-w-2xl rounded-lg border-l-3 border-[var(--color-orange)] bg-[var(--color-surface)] p-4">
          <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-orange)]">
            Not supported in this browser
          </div>
          <p className="mt-2 text-sm text-[var(--color-muted)]">
            WebSerial is unavailable here, which on iOS is true of every browser because they share
            one engine. Use a Chromium-based browser on a desktop or Android device, or read the
            same lines in any serial terminal. The simulated flight below still works.
          </p>
        </div>
      )}

      <div className="mt-5 flex flex-wrap gap-3">
        <button
          type="button"
          onClick={connect}
          disabled={support !== 'yes' || connected}
          className="rounded-md border border-[var(--color-hivis)] px-4 py-2 text-sm font-medium text-[var(--color-hivis)] transition-colors hover:bg-[var(--color-hivis)] hover:text-black disabled:cursor-not-allowed disabled:opacity-40 disabled:hover:bg-transparent disabled:hover:text-[var(--color-hivis)]"
        >
          {connected ? 'Connected' : 'Connect a ground station'}
        </button>
        <button
          type="button"
          onClick={simulating ? () => stopSim.current?.() : simulate}
          className="rounded-md border border-[var(--color-line-bright)] px-4 py-2 text-sm font-medium text-[var(--color-body)] transition-colors hover:border-[var(--color-body)]"
        >
          {simulating ? 'Stop' : 'Play a simulated flight'}
        </button>
        {frames.length > 0 && (
          <button
            type="button"
            onClick={reset}
            className="rounded-md px-3 py-2 text-sm text-[var(--color-dim)] hover:text-white"
          >
            Clear
          </button>
        )}
      </div>

      {error && (
        <p className="mt-3 max-w-2xl text-sm text-[var(--color-alert)]">
          {error}
        </p>
      )}

      {isSim && (
        <div className="mt-5 max-w-2xl rounded-lg border-l-3 border-[var(--color-alert)] bg-[var(--color-surface)] p-4">
          <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-alert)]">
            Simulated, not a flight
          </div>
          <p className="mt-2 text-sm text-[var(--color-muted)]">
            Every packet here carries the SIM flag. No oApogee has flown, so there is nothing to
            replay: the shape is the flight state machine and the numbers are a curve chosen to
            exercise every packet type. A receiver must never publish one of these as a flight, and
            this one will not.
          </p>
        </div>
      )}

      <div className="mt-6 grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
        {[
          {
            label: 'Altitude',
            value: altitude ? altitude.value : '.',
            note: latest?.decoded?.stateName ?? 'no packets yet',
          },
          {
            label: 'Apogee',
            value: apogeeField ? apogeeField.value : '.',
            note: apogee ? 'reported by the payload' : 'not reached',
          },
          {
            label: 'Packets',
            value: String(stats.decoded),
            note: `${stats.rejected} rejected, ${stats.lost} lost`,
          },
          {
            label: 'Signal',
            value: fmt(frames[0]?.line.rssi, 0, 'dBm'),
            note: `SNR ${fmt(frames[0]?.line.snr, 1, 'dB')}`,
          },
        ].map((s) => (
          <div
            key={s.label}
            className="rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)] p-4"
          >
            <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
              {s.label}
            </div>
            <div className="mt-1 text-xl font-semibold text-[var(--color-hivis)]">{s.value}</div>
            <div className="mt-1 text-xs text-[var(--color-muted)]">{s.note}</div>
          </div>
        ))}
      </div>

      {frames.length > 0 && (
        <div className="table-scroll mt-5">
          <table className="data">
            <thead>
              <tr>
                <th>Type</th>
                <th>State</th>
                <th>Seq</th>
                <th>Value</th>
                <th>RSSI</th>
              </tr>
            </thead>
            <tbody>
              {frames.slice(0, 40).map((f, i) => (
                <tr key={`${f.at}-${i}`}>
                  <td>
                    {f.decoded ? (
                      <span className="font-mono text-xs text-[var(--color-body)]">
                        {f.decoded.type}
                      </span>
                    ) : (
                      <span className="font-mono text-xs text-[var(--color-alert)]">REJECTED</span>
                    )}
                  </td>
                  <td className="font-mono text-xs text-[var(--color-muted)]">
                    {f.decoded?.stateName ?? '.'}
                  </td>
                  <td className="font-mono text-xs text-[var(--color-muted)]">
                    {f.decoded?.seq ?? '.'}
                  </td>
                  <td className="text-sm text-[var(--color-muted)]">
                    {f.decoded
                      ? f.decoded.fields
                          .slice(0, 2)
                          .map((x) => x.value)
                          .join(', ')
                      : f.error}
                  </td>
                  <td className="whitespace-nowrap font-mono text-xs text-[var(--color-dim)]">
                    {fmt(f.line.rssi, 0, 'dBm')}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      <p className="mt-4 max-w-2xl font-mono text-xs text-[var(--color-dim)]">
        Rejected frames are shown rather than hidden. A receiver that never displays a rejection
        makes the link look better than it is, and the rejection count is the honest measure of
        link quality.
      </p>
    </section>
  )
}

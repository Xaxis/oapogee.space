'use client'

import { useState } from 'react'
import dynamic from 'next/dynamic'
import Link from 'next/link'
import type { Phase } from '@/lib/data'

// Dynamically imported so three.js never lands in the first-load bundle. The
// hero is fully readable without it: the scene is an explanation of the state
// machine, and the state machine is also written out in the legend below it.
const FlightScene = dynamic(() => import('./FlightScene'), {
  ssr: false,
  loading: () => null,
})

export function Hero({ phases }: { phases: Phase[] }) {
  const [active, setActive] = useState('ARMED')
  const current = phases.find((p) => p.id === active)

  return (
    <section className="relative min-h-[30rem]">
      {/*
        On a wide viewport the scene occupies the right side and the copy the
        left, so the two never overlap and the trajectory is legible as a
        diagram rather than as texture behind a headline. Below that breakpoint
        it spans the full width, faded well back, because a phone has no room
        for both and the copy has to win.
      */}
      <div
        className="pointer-events-none absolute inset-y-0 right-0 -z-10 w-full opacity-35 lg:w-[56%] lg:opacity-100"
        style={{
          maskImage:
            'linear-gradient(to right, transparent 0%, rgba(0,0,0,0.6) 22%, rgba(0,0,0,1) 48%)',
          WebkitMaskImage:
            'linear-gradient(to right, transparent 0%, rgba(0,0,0,0.6) 22%, rgba(0,0,0,1) 48%)',
        }}
      >
        <FlightScene onPhase={setActive} />
      </div>

      <div className="py-10 sm:py-16">
        <p className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">
          Open source rocket telemetry
        </p>
        <h1 className="mt-4 max-w-xl text-4xl font-semibold leading-[1.1] text-white sm:text-5xl">
          A small, cheap sensor package that tells you exactly what your model rocket did.
        </h1>
        <p className="mt-6 max-w-lg text-lg text-[var(--color-muted)]">
          oApogee records altitude, acceleration and orientation through the whole flight, logs it
          onboard, and can send it live to a receiver on the ground. It attaches to a rocket you
          already own.
        </p>

        <div className="mt-8 flex flex-wrap gap-3">
          <Link
            href="/start"
            className="rounded-md border border-[var(--color-hivis)] bg-[var(--color-hivis)] px-4 py-2 text-sm font-medium !text-black !no-underline hover:opacity-90"
          >
            Start here
          </Link>
          <Link
            href="/reference/schematic"
            className="rounded-md border border-[var(--color-line-bright)] px-4 py-2 text-sm font-medium !text-[var(--color-body)] !no-underline hover:border-[var(--color-body)]"
          >
            See the circuit
          </Link>
        </div>
      </div>

      {/* The legend is the point of the animation. It names what is happening
          and says, unambiguously, that the shape is a diagram rather than a
          recorded flight. */}
      <div className="rounded-lg border border-[var(--color-line)] bg-[var(--color-surface)]/70 p-5 backdrop-blur">
        <div className="flex flex-wrap items-center gap-x-2 gap-y-2">
          {phases.map((phase, i) => (
            <span key={phase.id} className="flex items-center gap-2">
              <button
                type="button"
                onClick={() => setActive(phase.id)}
                className={`rounded px-2 py-1 font-mono text-xs uppercase tracking-wider transition-colors ${
                  phase.id === active
                    ? 'bg-[var(--color-hivis)] text-black'
                    : 'text-[var(--color-dim)] hover:text-[var(--color-body)]'
                }`}
              >
                {phase.id}
              </button>
              {i < phases.length - 1 && (
                <span aria-hidden className="text-[var(--color-line-bright)]">
                  &rsaquo;
                </span>
              )}
            </span>
          ))}
        </div>

        {current && (
          <div className="mt-4 border-t border-[var(--color-line)] pt-4">
            <h2 className="text-sm font-semibold text-white">{current.name}</h2>
            <p className="mt-1 max-w-2xl text-sm text-[var(--color-muted)]">{current.summary}</p>
          </div>
        )}

        <p className="mt-4 font-mono text-xs text-[var(--color-dim)]">
          Diagram of the flight state machine, not recorded data.
        </p>
      </div>
    </section>
  )
}

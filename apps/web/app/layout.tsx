import type { Metadata } from 'next'
import Link from 'next/link'
import { NAV_GROUPS, navFor } from '@/lib/routes'
import '../styles/globals.css'

const SITE_URL = process.env.NEXT_PUBLIC_SITE_URL ?? 'https://oapogee.space'

// Metadata deliberately avoids the bare word "apogee" as a search term. That
// word belongs to Apogee Components in this hobby and is not winnable. These
// are the phrases the project can actually own.
export const metadata: Metadata = {
  metadataBase: new URL(SITE_URL),
  title: {
    default: 'oApogee: open source rocket telemetry',
    template: '%s | oApogee',
  },
  description:
    'An open source model rocket telemetry payload you can build in an evening. Altitude, acceleration and orientation logged onboard, with a live LoRa downlink and GNSS recovery tracking.',
  keywords: [
    'open source rocket telemetry',
    'DIY rocket altimeter',
    'model rocket flight data',
    'LoRa rocket tracker',
    'model rocket telemetry kit',
  ],
  openGraph: {
    type: 'website',
    siteName: 'oApogee',
    url: SITE_URL,
    title: 'oApogee: open source rocket telemetry',
    description:
      'A cheap, light, open source telemetry payload for model rockets, and the documentation that teaches you to build one.',
  },
  robots: { index: true, follow: true },
}

// Three groups, because the three readers arrive with different intent: the
// beginner building one, the flier using one, and the engineer who wants the
// packet format and will skip everything else.
const NAV = NAV_GROUPS.map((group) => ({ group, items: navFor(group) }))

// The header carries the entry point into each group rather than all twenty
// links. The footer carries everything.
const HEADER_LINKS = [
  { href: '/start', label: 'Start here' },
  { href: '/bom', label: 'Bill of materials' },
  { href: '/build', label: 'Build' },
  { href: '/safety', label: 'Safety' },
  { href: '/reference', label: 'Reference' },
]

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        <header className="no-print sticky top-0 z-50 border-b border-[var(--color-line)] bg-[var(--color-ink)]/90 backdrop-blur">
          <div className="mx-auto flex max-w-6xl flex-wrap items-center gap-x-6 gap-y-2 px-5 py-3">
            <Link href="/" className="wordmark text-lg !text-white !no-underline">
              <span className="o">o</span>apogee
            </Link>
            <nav className="flex flex-wrap items-center gap-x-5 gap-y-1 text-sm">
              {HEADER_LINKS.map((item) => (
                <Link
                  key={item.href}
                  href={item.href}
                  className="!text-[var(--color-muted)] !no-underline hover:!text-white"
                >
                  {item.label}
                </Link>
              ))}
            </nav>
            <span className="chip chip-draft ml-auto">v0.1 design stage</span>
          </div>
        </header>

        <main className="mx-auto max-w-6xl px-5 py-12">{children}</main>

        <footer className="no-print mt-24 border-t border-[var(--color-line)]">
          <div className="mx-auto max-w-6xl px-5 py-10 text-sm text-[var(--color-muted)]">
            <div className="grid gap-8 sm:grid-cols-3">
              {NAV.map((group) => (
                <div key={group.group}>
                  <div className="mb-2 font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                    {group.group}
                  </div>
                  <ul className="flex flex-col gap-1">
                    {group.items.map((item) => (
                      <li key={item.href}>
                        <Link href={item.href} className="!text-[var(--color-muted)] !no-underline hover:!text-white">
                          {item.label}
                        </Link>
                      </li>
                    ))}
                  </ul>
                </div>
              ))}
            </div>

            <p className="mt-10 max-w-2xl">
              <strong className="text-[var(--color-body)]">oApogee is a passive instrumentation
              payload.</strong>{' '}
              It does not fire ejection charges, control deployment, ignite motors, or command any
              pyrotechnic device.{' '}
              <Link href="/safety">Read the safety and rules page</Link> before you build or fly
              anything.
            </p>

            <p className="mt-6 max-w-2xl text-xs text-[var(--color-dim)]">
              Hardware under CERN-OHL-S 2.0, firmware under Apache 2.0, documentation under CC BY-SA
              4.0. The design is open, the name is not. <Link href="/about">Licensing in full</Link>
              . <a href="https://github.com/Xaxis/oapogee.space">Source on GitHub</a>.
            </p>
            <p className="mt-2 text-xs text-[var(--color-dim)]">
              oApogee is not affiliated with Apogee Components.
            </p>
          </div>
        </footer>
      </body>
    </html>
  )
}

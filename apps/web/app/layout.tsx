import type { Metadata } from 'next'
import Link from 'next/link'
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

// Three links, not twenty. Every page inside the docs shell already carries the
// full navigation in its sidebar, so a header that repeats it is duplicated
// chrome that makes the site look larger and harder than it is. These three are
// the entry points somebody arriving cold actually needs: where to begin, where
// the reference material is, and where the source lives.
const HEADER_LINKS = [
  { href: '/start', label: 'Start here' },
  { href: '/build', label: 'Build guide' },
  { href: '/reference', label: 'Reference' },
]

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        {/* Every page inside the docs shell puts the whole route table ahead of
            its content, so without this a keyboard user tabs through twenty
            five links to reach the first word of every page. Fixed rather than
            absolute so the link is still on screen when focus lands on it after
            a client side navigation on a scrolled page, and above the sticky
            header's z-50. */}
        <a
          href="#main"
          className="no-print sr-only focus:not-sr-only focus:fixed focus:left-3 focus:top-3 focus:z-[60] focus:rounded focus:border focus:border-[var(--color-line-bright)] focus:bg-[var(--color-surface)] focus:px-4 focus:py-2 focus:!no-underline"
        >
          Skip to content
        </a>

        <header className="no-print sticky top-0 z-50 border-b border-[var(--color-line)] bg-[var(--color-ink)]/90 backdrop-blur">
          <div className="mx-auto flex max-w-[1500px] flex-wrap items-center gap-x-6 gap-y-2 px-5 py-3 lg:px-8">
            <Link href="/" className="wordmark text-lg !text-white !no-underline">
              <span className="o">o</span>apogee
            </Link>
            <nav aria-label="Main" className="flex flex-wrap items-center gap-x-5 gap-y-1 text-sm">
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

        {/* No width constraint here. The docs shell needs the full viewport for
            its sidebar, and the homepage sets its own container. A max-width on
            main would quietly squash one of the two.

            tabIndex -1 because the id alone only moves the sequential focus
            start point, and Safari does not do even that, so the skip link
            would appear to do nothing. */}
        <main id="main" tabIndex={-1}>
          {children}
        </main>

        <footer className="no-print mt-24 border-t border-[var(--color-line)]">
          {/* Deliberately not a link farm. Every docs page carries the full
              navigation in its sidebar; a footer repeating it adds twenty links
              that nobody uses and buries the one thing that has to be read. */}
          <div className="mx-auto max-w-[1500px] px-5 py-10 text-sm text-[var(--color-muted)] lg:px-8">
            <p className="max-w-2xl">
              <strong className="text-[var(--color-body)]">
                oApogee is a passive instrumentation payload.
              </strong>{' '}
              It does not fire ejection charges, control deployment, ignite motors, or command any
              pyrotechnic device. <Link href="/safety">Read the safety and rules</Link> before you
              build or fly anything.
            </p>

            <div className="mt-8 flex flex-wrap items-center gap-x-6 gap-y-2 text-xs text-[var(--color-dim)]">
              <a href="https://github.com/Xaxis/oapogee.space">GitHub</a>
              <Link href="/about">Licence</Link>
              <Link href="/status">Status</Link>
              <span>Not affiliated with Apogee Components.</span>
            </div>
          </div>
        </footer>
      </body>
    </html>
  )
}

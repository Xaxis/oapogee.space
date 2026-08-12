'use client'

import Link from 'next/link'
import { usePathname } from 'next/navigation'
import { useState } from 'react'
import { NAV_GROUPS, navFor, findNav, neighbours } from '@/lib/routes'

/**
 * Shared chrome for every page except the homepage.
 *
 * Applied by the (docs) route group, so pages keep their flat URLs and gain a
 * persistent sidebar, a breadcrumb, and a reading order without each one
 * re-implementing navigation. Before this every page was an island: the only
 * route from the build guide to the troubleshooting entry it referenced was a
 * link somebody remembered to write, and the only way to discover the packet
 * spec was to already know it existed.
 *
 * The sidebar, the breadcrumb and the previous/next links all read the same
 * route table, so they cannot disagree about what the site contains.
 */
export function DocsShell({ children }: { children: React.ReactNode }) {
  const pathname = usePathname()
  const here = findNav(pathname)
  const { prev, next } = neighbours(pathname)
  const [open, setOpen] = useState(false)

  return (
    <div className="mx-auto max-w-[1500px] px-5 lg:grid lg:grid-cols-[220px_minmax(0,1fr)] lg:gap-10 lg:px-8">
      <aside className="no-print border-b border-[var(--color-line)] lg:sticky lg:top-14 lg:h-[calc(100dvh-3.5rem)] lg:overflow-y-auto lg:border-b-0 lg:pt-8">
        {/* On a phone the sidebar is a disclosure rather than a permanent
            column, and its closed state shows where you are, which is the only
            part of it that matters when you are not navigating. */}
        <button
          type="button"
          onClick={() => setOpen((v) => !v)}
          aria-expanded={open}
          className="flex w-full items-center justify-between py-3 text-sm text-[var(--color-muted)] lg:hidden"
        >
          <span>
            {here ? (
              <>
                <span className="text-[var(--color-dim)]">{here.group}</span>
                <span className="mx-2 text-[var(--color-dim)]">/</span>
                <span className="text-white">{here.label}</span>
              </>
            ) : (
              'Documentation'
            )}
          </span>
          <span aria-hidden className="font-mono text-xs text-[var(--color-dim)]">
            {open ? 'close' : 'menu'}
          </span>
        </button>

        <nav
          className={`${open ? 'block' : 'hidden'} pb-8 lg:block`}
          aria-label="Documentation"
        >
          {NAV_GROUPS.map((group) => (
            <div key={group} className="mb-7">
              <p className="mb-2 font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                {group}
              </p>
              <ul className="border-l border-[var(--color-line)]">
                {navFor(group).map((item) => {
                  const active = pathname === item.href
                  return (
                    <li key={item.href}>
                      <Link
                        href={item.href}
                        // The (docs) layout stays mounted across navigations
                        // within the group, so this state survives the click.
                        // Without closing it the destination page opens
                        // underneath a phone screenful of navigation.
                        onClick={() => setOpen(false)}
                        aria-current={active ? 'page' : undefined}
                        className={`-ml-px flex border-l-2 py-1.5 pl-3 text-sm leading-snug !no-underline transition-colors ${
                          active
                            ? 'border-[var(--color-hivis)] font-medium !text-white'
                            : 'border-transparent !text-[var(--color-muted)] hover:border-[var(--color-line-bright)] hover:!text-white'
                        }`}
                      >
                        {item.label}
                      </Link>
                    </li>
                  )
                })}
              </ul>
            </div>
          ))}
        </nav>
      </aside>

      <div className="min-w-0 py-10 lg:py-12">
        {here && (
          <nav
            aria-label="Breadcrumb"
            className="no-print mb-6 hidden font-mono text-xs uppercase tracking-widest text-[var(--color-dim)] lg:block"
          >
            <Link href="/" className="!text-[var(--color-dim)] !no-underline hover:!text-white">
              oApogee
            </Link>
            <span className="mx-2">/</span>
            <span>{here.group}</span>
            <span className="mx-2">/</span>
            <span className="text-[var(--color-muted)]">{here.label}</span>
          </nav>
        )}

        {children}

        {(prev || next) && (
          <nav
            aria-label="Reading order"
            className="no-print mt-20 grid gap-4 border-t border-[var(--color-line)] pt-8 sm:grid-cols-2"
          >
            {prev ? (
              <Link
                href={prev.href}
                className="rounded-lg border border-[var(--color-line)] p-4 !no-underline hover:border-[var(--color-line-bright)]"
              >
                <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                  Previous
                </div>
                <div className="mt-1 font-medium !text-white">{prev.label}</div>
              </Link>
            ) : (
              <span />
            )}
            {next && (
              <Link
                href={next.href}
                className="rounded-lg border border-[var(--color-line)] p-4 text-right !no-underline hover:border-[var(--color-line-bright)]"
              >
                <div className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                  Next
                </div>
                <div className="mt-1 font-medium !text-white">{next.label}</div>
              </Link>
            )}
          </nav>
        )}
      </div>
    </div>
  )
}

import Link from 'next/link'
import { NAV_GROUPS, navFor } from '@/lib/routes'

/**
 * The one page somebody reaches when something is broken, so it has to offer a
 * way forward rather than an apology.
 *
 * It used to say "most of oApogee is still unwritten" and "the navigation lists
 * everything that exists". The first stopped being true once every route in the
 * table rendered, and the second was never true here: this page sits outside
 * the docs shell, so it has no sidebar, and it was pointing at navigation that
 * is not on it. Three header links and a dead end.
 *
 * The route table is rendered here instead. It is the same list the sidebar,
 * the breadcrumb and the reading order are built from, so this page cannot fall
 * behind the site the way a hand-written list of links would.
 */
export default function NotFound() {
  return (
    // main sets no width or gutter, so this page supplies the same outer
    // container the header and the footer use and keeps the reading measure on
    // an inner element. Centring the 46rem block instead would leave its left
    // edge hundreds of pixels inboard of the wordmark above it.
    <div className="mx-auto max-w-[1500px] px-5 py-16 lg:px-8">
      <div className="max-w-[46rem]">
        <p className="font-mono text-xs uppercase tracking-widest text-[var(--color-hivis)]">404</p>
        <h1 className="mt-4 text-3xl font-semibold text-white">There is no page here</h1>
        <p className="mt-4 text-[var(--color-muted)]">
          Either the address is wrong or something on this site links somewhere it should not. If a
          page on oApogee sent you here, that is a bug worth{' '}
          <a href="https://github.com/Xaxis/oapogee.space/issues">reporting</a>. Everything the site
          contains is below.
        </p>
      </div>

      <div className="mt-12 grid gap-10 sm:grid-cols-2 lg:grid-cols-3">
        {NAV_GROUPS.map((group) => (
          <nav key={group} aria-label={group}>
            <p className="mb-3 font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
              {group}
            </p>
            <ul className="flex flex-col gap-1.5 border-l border-[var(--color-line)] text-sm">
              {navFor(group).map((item) => (
                <li key={item.href} className="pl-3">
                  <Link
                    href={item.href}
                    className="!text-[var(--color-muted)] !no-underline hover:!text-white"
                  >
                    {item.label}
                  </Link>
                </li>
              ))}
            </ul>
          </nav>
        ))}
      </div>
    </div>
  )
}

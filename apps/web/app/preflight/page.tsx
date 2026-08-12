import type { Metadata } from 'next'
import { getPreflight } from '@/lib/data'

export const metadata: Metadata = {
  title: 'Preflight checklist',
  description:
    'The oApogee flight day checklist. Printable, one action per line, with the reason kept separate from the action.',
}

// Designed to be printed, laminated, and used on a range table by somebody who
// is also thinking about a motor and a launch rail. The reasons are on screen
// and hidden in print: once you know why an item is there, the why is noise, and
// a checklist you have to read paragraphs of is a checklist you skip.

export default function Preflight() {
  const { intro, sections, footer, updated } = getPreflight()

  return (
    <div>
      <header className="max-w-[46rem]">
        <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">
          Preflight checklist
        </h1>
        <p className="mt-3 text-lg text-[var(--color-muted)]">{intro}</p>
        <div className="no-print mt-5 flex flex-wrap gap-2">
          <span className="chip chip-draft">draft</span>
          <span className="chip">updated {updated}</span>
          <span className="chip">print this page</span>
        </div>
      </header>

      <div className="mt-12 flex flex-col gap-12">
        {sections.map((section) => (
          <section key={section.id} className="break-inside-avoid">
            <div className="flex max-w-3xl flex-wrap items-baseline gap-3 border-b border-[var(--color-line)] pb-2">
              <h2 className="text-xl font-semibold text-white">{section.title}</h2>
              <span className="font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
                {section.where}
              </span>
            </div>

            <ol className="mt-5 flex max-w-3xl flex-col gap-5">
              {section.items.map((item) => (
                <li key={item.id} id={item.id} className="flex gap-4 scroll-mt-24">
                  {/* A real box, because this gets printed and ticked with a pen. */}
                  <span
                    aria-hidden
                    className="mt-1 h-4 w-4 shrink-0 rounded-sm border border-[var(--color-line-bright)] print:border-black"
                  />
                  <div className="min-w-0">
                    <div className="flex flex-wrap items-baseline gap-2">
                      <span className="font-medium text-white print:text-black">{item.text}</span>
                      {item.critical && (
                        <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-alert)]">
                          critical
                        </span>
                      )}
                      {item.tiers.length < 3 && (
                        <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-dim)]">
                          {item.tiers.join(', ')} only
                        </span>
                      )}
                    </div>
                    <p className="no-print mt-1 text-sm text-[var(--color-muted)]">{item.why}</p>
                  </div>
                </li>
              ))}
            </ol>
          </section>
        ))}
      </div>

      <p className="mt-14 max-w-[46rem] border-t border-[var(--color-line)] pt-6 text-sm text-[var(--color-muted)]">
        {footer}
      </p>
    </div>
  )
}

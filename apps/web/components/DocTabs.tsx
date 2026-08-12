'use client'

import { useId, useSyncExternalStore, type ReactNode } from 'react'

/**
 * Panelled navigation for content that is parallel rather than sequential.
 *
 * The bill of materials carries the same board built two ways, and the
 * troubleshooting page carries six unrelated categories. Neither is a
 * narrative: nobody reads the Board path on the way to the Modules path, and
 * somebody chasing a radio symptom should not scroll through the power
 * symptoms to reach it. Stacked vertically they were a single very long scroll
 * where the useful part was always somewhere else.
 *
 * Selection lives in the URL hash, for two reasons. A link to a specific panel
 * is worth having, and it survives a reload, which matters when somebody is
 * following a build guide with the page open on a phone.
 *
 * Reading the hash through useSyncExternalStore rather than useEffect avoids
 * the hydration trap where the server renders the default panel, the client
 * computes a different one, and React discards the client's value because the
 * markup already matched.
 */

export interface DocTab {
  id: string
  label: string
  /** Shown under the tab strip while this panel is open. */
  hint?: string
  content: ReactNode
}

function subscribe(onChange: () => void) {
  window.addEventListener('hashchange', onChange)
  return () => window.removeEventListener('hashchange', onChange)
}

export function DocTabs({ tabs, label }: { tabs: DocTab[]; label: string }) {
  const hash = useSyncExternalStore(
    subscribe,
    () => window.location.hash,
    () => ''
  )
  const listId = useId()

  const bare = hash.replace(/^#/, '')

  // Resolve the hash to a panel in two steps.
  //
  // First by name, with a prefix match so a nested deep link such as
  // "#modules-mcu" still opens the modules panel.
  //
  // Then, failing that, by asking which panel contains the element the hash
  // names. Panelling a page silently breaks every existing link into it: an
  // anchor that used to be a section heading becomes content inside a closed
  // panel, and the browser scrolls to nothing. Rather than rewrite those links
  // and break the next set, any anchor living inside a panel opens it.
  let selected = tabs.findIndex((t) => bare === t.id || bare.startsWith(`${t.id}-`))
  if (selected === -1 && bare && typeof document !== 'undefined') {
    const target = document.getElementById(bare)
    if (target) {
      const panel = target.closest('[data-doctab]')
      const id = panel?.getAttribute('data-doctab')
      if (id) selected = tabs.findIndex((t) => t.id === id)
    }
  }
  if (selected === -1) selected = 0

  return (
    <div>
      <div
        role="tablist"
        aria-label={label}
        id={listId}
        className="no-print flex flex-wrap gap-1 border-b border-[var(--color-line)]"
      >
        {tabs.map((tab, i) => {
          const active = i === selected
          return (
            <a
              key={tab.id}
              role="tab"
              href={`#${tab.id}`}
              aria-selected={active}
              className={`-mb-px border-b-2 px-4 py-2.5 text-sm !no-underline transition-colors ${
                active
                  ? 'border-[var(--color-hivis)] font-medium !text-white'
                  : 'border-transparent !text-[var(--color-muted)] hover:!text-white'
              }`}
            >
              {tab.label}
            </a>
          )
        })}
      </div>

      {tabs[selected]?.hint && (
        <p className="mt-4 max-w-2xl text-sm text-[var(--color-muted)]">{tabs[selected].hint}</p>
      )}

      {tabs.map((tab, i) => (
        <div
          key={tab.id}
          data-doctab={tab.id}
          role="tabpanel"
          aria-labelledby={listId}
          // Closed panels stay in the DOM rather than unmounting, so that the
          // anchor lookup above can find an element inside a panel that is not
          // currently open, and so the print stylesheet gets the whole page.
          hidden={i !== selected}
          className="mt-6 print:!block"
        >
          {tab.content}
        </div>
      ))}
    </div>
  )
}

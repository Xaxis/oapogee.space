'use client'

import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useRouter } from 'next/navigation'
import type { SearchDoc } from '@/lib/search-index'

/**
 * Search across the whole site.
 *
 * Twenty-one pages is past the point where navigation alone is enough,
 * particularly for the two cases that matter most: somebody with a symptom in
 * front of them, and somebody holding a part wondering if it is the right one.
 * Both are searches for a phrase that appears in exactly one place, so scoring
 * is deliberately simple and favours exact and prefix matches over cleverness.
 *
 * No dependency and no index file to fetch: the index is built when the site
 * builds and shipped with the page. It is a few tens of kilobytes because it
 * indexes titles, headings, symptoms, glossary terms and part numbers rather
 * than every paragraph, which is also what makes the results good.
 */

const KIND_LABEL: Record<SearchDoc['kind'], string> = {
  page: 'Page',
  section: 'Section',
  symptom: 'Symptom',
  term: 'Term',
  part: 'Part',
}

/**
 * Higher is better, 0 means no match. Every term in the query must appear
 * somewhere, so a two-word query narrows rather than widens.
 */
function score(doc: SearchDoc, terms: string[]): number {
  const title = doc.title.toLowerCase()
  const context = doc.context.toLowerCase()
  const keywords = (doc.keywords ?? '').toLowerCase()
  let total = 0

  for (const term of terms) {
    let best = 0
    if (title === term) best = 100
    else if (title.startsWith(term)) best = 60
    else if (title.includes(term)) best = 40
    else if (keywords.includes(term)) best = 30
    else if (context.includes(term)) best = 15
    if (best === 0) return 0
    total += best
  }

  // A symptom is what somebody in trouble is looking for, and a whole page beats
  // a section of one when both match equally.
  if (doc.kind === 'symptom') total += 12
  if (doc.kind === 'page') total += 6
  // Shorter titles match more precisely for the same overlap.
  return total - Math.min(title.length, 60) / 20
}

export function Search({ docs }: { docs: SearchDoc[] }) {
  const [open, setOpen] = useState(false)
  const [query, setQuery] = useState('')
  const [active, setActive] = useState(0)
  const inputRef = useRef<HTMLInputElement>(null)
  const router = useRouter()

  const results = useMemo(() => {
    const terms = query.toLowerCase().trim().split(/\s+/).filter(Boolean)
    if (!terms.length) return []
    return docs
      .map((doc) => ({ doc, s: score(doc, terms) }))
      .filter((r) => r.s > 0)
      .sort((a, b) => b.s - a.s)
      .slice(0, 12)
      .map((r) => r.doc)
  }, [docs, query])

  const close = useCallback(() => {
    setOpen(false)
    setQuery('')
    setActive(0)
  }, [])

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
        e.preventDefault()
        setOpen((v) => !v)
        return
      }
      // A bare slash is the other convention people try, but not while they are
      // typing into something.
      const el = document.activeElement
      const typing =
        el instanceof HTMLInputElement ||
        el instanceof HTMLTextAreaElement ||
        (el as HTMLElement | null)?.isContentEditable
      if (e.key === '/' && !typing && !open) {
        e.preventDefault()
        setOpen(true)
      }
      if (e.key === 'Escape' && open) close()
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [open, close])

  useEffect(() => {
    if (open) inputRef.current?.focus()
  }, [open])

  useEffect(() => setActive(0), [query])

  const go = (href: string) => {
    close()
    router.push(href)
  }

  const onInputKey = (e: React.KeyboardEvent) => {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setActive((i) => Math.min(i + 1, results.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setActive((i) => Math.max(i - 1, 0))
    } else if (e.key === 'Enter' && results[active]) {
      e.preventDefault()
      go(results[active].href)
    }
  }

  return (
    <>
      <button
        type="button"
        onClick={() => setOpen(true)}
        className="flex items-center gap-2 rounded-md border border-[var(--color-line)] px-3 py-1.5 text-sm text-[var(--color-dim)] transition-colors hover:border-[var(--color-line-bright)] hover:text-[var(--color-body)]"
      >
        <span>Search</span>
        <kbd className="hidden font-mono text-xs text-[var(--color-dim)] sm:inline">/</kbd>
      </button>

      {open && (
        <div
          className="fixed inset-0 z-[70] flex items-start justify-center bg-black/70 p-4 pt-[10vh] backdrop-blur-sm"
          onClick={close}
          role="presentation"
        >
          <div
            className="w-full max-w-xl overflow-hidden rounded-xl border border-[var(--color-line-bright)] bg-[var(--color-surface)] shadow-2xl"
            onClick={(e) => e.stopPropagation()}
            role="dialog"
            aria-modal="true"
            aria-label="Search"
          >
            <input
              ref={inputRef}
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              onKeyDown={onInputKey}
              placeholder="Search pages, symptoms, terms and part numbers"
              aria-label="Search"
              className="w-full border-b border-[var(--color-line)] bg-transparent px-4 py-3.5 text-base text-white outline-none placeholder:text-[var(--color-dim)]"
            />

            {query && results.length === 0 && (
              <p className="px-4 py-6 text-sm text-[var(--color-muted)]">
                Nothing matches. Symptoms are indexed by what you saw rather than by which part
                failed, so try describing the symptom.
              </p>
            )}

            {results.length > 0 && (
              <ul className="max-h-[55vh] overflow-y-auto py-1">
                {results.map((doc, i) => (
                  <li key={`${doc.href}-${doc.title}`}>
                    <button
                      type="button"
                      onMouseEnter={() => setActive(i)}
                      onClick={() => go(doc.href)}
                      className={`flex w-full items-center gap-3 px-4 py-2.5 text-left transition-colors ${
                        i === active ? 'bg-[var(--color-surface-2)]' : ''
                      }`}
                    >
                      <span className="min-w-0 flex-1">
                        <span className="block truncate text-sm text-white">{doc.title}</span>
                        {doc.context && (
                          <span className="block truncate text-xs text-[var(--color-muted)]">
                            {doc.context}
                          </span>
                        )}
                      </span>
                      <span className="chip shrink-0">{KIND_LABEL[doc.kind]}</span>
                    </button>
                  </li>
                ))}
              </ul>
            )}

            <div className="flex items-center gap-4 border-t border-[var(--color-line)] px-4 py-2 font-mono text-xs text-[var(--color-dim)]">
              <span>up and down to move</span>
              <span>enter to open</span>
              <span>esc to close</span>
            </div>
          </div>
        </div>
      )}
    </>
  )
}

import type { Frontmatter, MarkerCount, TocEntry } from '@/lib/content'

const STATUS_CHIP: Record<string, string> = {
  draft: 'chip-draft',
  'needs-review': 'chip-draft',
  verified: 'chip-verified',
}

const STATUS_MEANING: Record<string, string> = {
  draft: 'Written, not reviewed by anyone. Check the primary sources it links to.',
  'needs-review': 'The author believes this is right and wants a second pair of eyes on it.',
  verified: 'Every number and every step has been checked on real hardware.',
}

/**
 * Every prose page: title block, contents rail, body.
 *
 * The contents rail only appears when there is enough page to need one. Four
 * headings is a list somebody reads instead of scrolling; two is furniture.
 */
export function DocPage({
  frontmatter,
  html,
  markers,
  toc,
}: {
  frontmatter: Frontmatter
  html: string
  markers: MarkerCount
  toc: TocEntry[]
}) {
  const showToc = toc.length >= 4

  return (
    <article className={showToc ? 'xl:grid xl:grid-cols-[minmax(0,1fr)_190px] xl:gap-12' : ''}>
      <div className="min-w-0">
        <header className="mb-10 max-w-[46rem]">
          <h1 className="text-3xl font-semibold leading-tight text-white sm:text-4xl">
            {frontmatter.title}
          </h1>
          <p className="mt-3 text-lg text-[var(--color-muted)]">{frontmatter.description}</p>

          <div className="mt-5 flex flex-wrap items-center gap-2">
            <span className={`chip ${STATUS_CHIP[frontmatter.status] ?? ''}`}>
              {frontmatter.status}
            </span>
            <span className="chip">{frontmatter.difficulty}</span>
            <span className="chip">updated {frontmatter.updated}</span>
            {markers.total > 0 && (
              <span className="chip">
                {markers.total} open marker{markers.total === 1 ? '' : 's'}
              </span>
            )}
          </div>

          <p className="mt-4 text-sm text-[var(--color-dim)]">
            {STATUS_MEANING[frontmatter.status]}
            {markers.total > 0 && (
              <>
                {' '}
                The highlighted blocks below are places where a number or a step is deliberately
                missing rather than guessed at.
              </>
            )}
          </p>
        </header>

        <div className="prose" dangerouslySetInnerHTML={{ __html: html }} />
      </div>

      {showToc && (
        <nav
          aria-label="On this page"
          className="no-print hidden xl:sticky xl:top-24 xl:block xl:self-start"
        >
          <p className="mb-3 font-mono text-xs uppercase tracking-widest text-[var(--color-dim)]">
            On this page
          </p>
          <ul className="flex flex-col gap-1.5 border-l border-[var(--color-line)] text-sm">
            {toc.map((entry) => (
              <li key={entry.id} className={entry.depth === 3 ? 'pl-6' : 'pl-3'}>
                <a
                  href={`#${entry.id}`}
                  className="!text-[var(--color-muted)] !no-underline hover:!text-white"
                >
                  {entry.text}
                </a>
              </li>
            ))}
          </ul>
        </nav>
      )}
    </article>
  )
}

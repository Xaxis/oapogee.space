import type { Frontmatter, MarkerCount } from '@/lib/content'

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

export function DocPage({
  frontmatter,
  html,
  markers,
}: {
  frontmatter: Frontmatter
  html: string
  markers: MarkerCount
}) {
  return (
    <article>
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
    </article>
  )
}

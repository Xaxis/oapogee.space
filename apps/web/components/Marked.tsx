import { MARKER_LABELS } from '@/lib/markers'

/**
 * Free text from data/, with any verification marker rendered as a label.
 *
 * Markers do not only appear at the start of a field. "Adequate for detecting
 * boost acceleration, less good for integrating it. TODO(verify): compare the
 * noise floors." is one sentence of substance followed by one open question,
 * and both belong on the page. Splitting only a leading marker left the rest
 * printing the literal string "TODO(verify):" at readers.
 *
 * No hooks and no imports that touch disk, so this renders on the server for
 * the mounting page and inside the client bill of materials alike.
 */

const KINDS = ['confirm-on-hardware', 'verify', 'confirm', 'photo']
const SPLIT = new RegExp(`TODO\\((?:${KINDS.join('|')})\\):\\s*`, 'g')
const KIND_AT = new RegExp(`TODO\\((${KINDS.join('|')})\\):\\s*`, 'g')

export function Marked({ text, className }: { text: string; className?: string }) {
  const bodies = text.split(SPLIT)
  const kinds = [...text.matchAll(KIND_AT)].map((m) => m[1])

  return (
    <p className={className}>
      {bodies.map((body, i) => {
        const trimmed = body.trim()
        // bodies[0] is whatever preceded the first marker, so the marker for
        // bodies[n] is kinds[n - 1].
        const kind = i > 0 ? kinds[i - 1] : null
        if (!trimmed && !kind) return null
        return (
          <span key={i}>
            {kind && (
              <span className="font-mono text-xs uppercase tracking-wider text-[var(--color-orange)]">
                {i > 0 && bodies[i - 1].trim() ? ' ' : ''}
                {MARKER_LABELS[kind] ?? 'Unverified'}:{' '}
              </span>
            )}
            {trimmed}
          </span>
        )
      })}
    </p>
  )
}

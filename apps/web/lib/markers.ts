/**
 * Verification markers, shared between the prose pipeline and the data-driven
 * components.
 *
 * Markers are written inline as `TODO(verify): ...` because that is the form
 * that is useful to whoever is editing the file. Prose has always turned them
 * into a labelled callout on the way to the page. The components that render
 * data/ did not, so the bill of materials and the mounting page printed the
 * literal string "TODO(verify):" at readers, doubled with a label the component
 * had already drawn above it.
 *
 * No filesystem access here on purpose: BomList is a client component and
 * cannot import lib/content.ts, which reads from disk.
 */

export const MARKER_LABELS: Record<string, string> = {
  verify: 'Unverified',
  'confirm-on-hardware': 'Needs hardware',
  confirm: 'Needs a decision',
  photo: 'Photo needed',
}

// Longest first, so confirm-on-hardware is not matched as confirm.
const KINDS = ['confirm-on-hardware', 'verify', 'confirm', 'photo']
const PATTERN = new RegExp(`^\\s*TODO\\((${KINDS.join('|')})\\):\\s*`)

export type Marker = { label: string | null; body: string }

/** Split a leading TODO marker off a string, returning its label and the rest. */
export function splitMarker(text: string): Marker {
  const m = PATTERN.exec(text)
  if (!m) return { label: null, body: text.trim() }
  return { label: MARKER_LABELS[m[1]] ?? 'Unverified', body: text.slice(m[0].length).trim() }
}

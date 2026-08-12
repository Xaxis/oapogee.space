import { readFileSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { REPO_ROOT } from './repo'

const PUBLIC = join(REPO_ROOT, 'apps/web/public')

export type InlineSvg = { svg: string; width: number; height: number }

/**
 * Read a generated SVG so it can be inlined and made interactive.
 *
 * Inlined rather than served through an <img> because a pan and zoom viewer
 * needs the drawing to be part of the document: an image in a scroll box is
 * where this started, and it is unusable on a phone. Both files here are
 * produced by this repository's own generators, so there is no untrusted markup
 * involved.
 */
export function readSvg(relativePath: string): InlineSvg | null {
  const path = join(PUBLIC, relativePath)
  if (!existsSync(path)) return null

  const svg = readFileSync(path, 'utf8')

  // Prefer the viewBox: it is the authoritative coordinate space, and width and
  // height attributes are sometimes absent or expressed in units.
  const viewBox = /viewBox="([\d.\s-]+)"/.exec(svg)
  if (viewBox) {
    const [, , w, h] = viewBox[1].trim().split(/\s+/).map(Number)
    if (Number.isFinite(w) && Number.isFinite(h) && w > 0 && h > 0) {
      return { svg, width: w, height: h }
    }
  }

  const w = Number(/\swidth="([\d.]+)"/.exec(svg)?.[1])
  const h = Number(/\sheight="([\d.]+)"/.exec(svg)?.[1])
  if (Number.isFinite(w) && Number.isFinite(h) && w > 0 && h > 0) return { svg, width: w, height: h }

  return null
}

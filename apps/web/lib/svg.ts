import { readFileSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { REPO_ROOT } from './repo'

const PUBLIC = join(REPO_ROOT, 'apps/web/public')

export type InlineSvg = {
  svg: string
  width: number
  height: number
  /**
   * Where the drawing starts. Not always the origin: the PCB export puts a
   * 213 unit wide board somewhere inside an 800 unit canvas, and the generator
   * gives it a viewBox tight to the board rather than to the canvas. A viewer
   * that assumed (0, 0) would fit and pan around a region the drawing is not
   * in, which is exactly what it did.
   */
  minX: number
  minY: number
}

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
    const [minX, minY, w, h] = viewBox[1].trim().split(/\s+/).map(Number)
    if ([minX, minY, w, h].every(Number.isFinite) && w > 0 && h > 0) {
      return { svg, width: w, height: h, minX, minY }
    }
  }

  const w = Number(/\swidth="([\d.]+)"/.exec(svg)?.[1])
  const h = Number(/\sheight="([\d.]+)"/.exec(svg)?.[1])
  if (Number.isFinite(w) && Number.isFinite(h) && w > 0 && h > 0)
    return { svg, width: w, height: h, minX: 0, minY: 0 }

  return null
}

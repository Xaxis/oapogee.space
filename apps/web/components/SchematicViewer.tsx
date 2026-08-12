'use client'

import { useCallback, useEffect, useRef, useState } from 'react'

/**
 * Pan and zoom for the generated drawings.
 *
 * A schematic in a scroll box is not an interface. The sheet is over a thousand
 * pixels wide with 8pt designators, so on a phone you got a letterbox onto one
 * corner of it and no way to reach another. It is one of the most useful things
 * this project publishes and it was effectively desktop-only.
 *
 * ZOOM IS APPLIED TO THE VIEWBOX, NOT AS A CSS TRANSFORM, and that is the whole
 * design of this component rather than an implementation detail.
 *
 * The first version scaled a wrapper with `transform: scale()` and promoted it
 * with `will-change: transform`, on the reasoning that compositing is cheaper
 * than re-rasterising vector geometry every frame. That reasoning is right and
 * the trade is wrong: promoting the layer makes the browser rasterise the SVG
 * once at its current size and then scale that bitmap, so zooming in produced a
 * blurry enlargement of a small render. Every pin label and every part
 * designator became unreadable at exactly the magnification somebody had zoomed
 * in to read them at.
 *
 * Driving the viewBox instead re-renders the vectors at every zoom level, so the
 * text is crisp at any magnification and stays crisp on a high density display.
 * It costs a repaint per frame, which for a static line drawing is nothing.
 */

const MIN_SPAN_FRACTION = 0.02 // deepest zoom: 2% of the drawing fills the view
const MAX_SPAN_FRACTION = 4 // furthest out: 4x the drawing fits in the view
const ZOOM_STEP = 1.5

type Box = { x: number; y: number; w: number; h: number }

export function SchematicViewer({
  svg,
  label,
  description,
  naturalWidth,
  naturalHeight,
  className = '',
}: {
  svg: string
  label: string
  description: string
  naturalWidth: number
  naturalHeight: number
  className?: string
}) {
  const hostRef = useRef<HTMLDivElement>(null)
  const svgRef = useRef<SVGSVGElement | null>(null)
  const [box, setBox] = useState<Box>({ x: 0, y: 0, w: naturalWidth, h: naturalHeight })
  const [dragging, setDragging] = useState(false)
  const [ready, setReady] = useState(false)
  // The width the fitted view spans, so the readout can report magnification
  // relative to "the whole sheet", which is what 100% means to a reader.
  const [fitW, setFitW] = useState(naturalWidth)

  const pointers = useRef(new Map<number, { x: number; y: number }>())
  const pinch = useRef<{ dist: number; w: number; h: number } | null>(null)
  const pan = useRef<{ x: number; y: number; box: Box } | null>(null)

  // Take over the injected SVG once: strip its intrinsic size so it fills the
  // host, and give it a viewBox if the generator did not (the tscircuit export
  // carries width and height only).
  useEffect(() => {
    const el = hostRef.current?.querySelector('svg')
    if (!el) return
    svgRef.current = el
    if (!el.getAttribute('viewBox')) {
      el.setAttribute('viewBox', `0 0 ${naturalWidth} ${naturalHeight}`)
    }
    el.setAttribute('width', '100%')
    el.setAttribute('height', '100%')
    el.setAttribute('preserveAspectRatio', 'xMidYMid meet')
    el.style.display = 'block'
    setReady(true)
  }, [svg, naturalWidth, naturalHeight])

  useEffect(() => {
    const el = svgRef.current
    if (!el || !ready) return
    el.setAttribute('viewBox', `${box.x} ${box.y} ${box.w} ${box.h}`)
  }, [box, ready])

  /** Frame the whole drawing, matched to the host's aspect so nothing is cropped. */
  const fit = useCallback(() => {
    const host = hostRef.current
    if (!host) return
    const aspect = host.clientWidth / Math.max(host.clientHeight, 1)
    const drawingAspect = naturalWidth / naturalHeight
    // preserveAspectRatio letterboxes for us, so the viewBox only has to contain
    // the drawing. Matching the host aspect keeps the reported zoom honest.
    const w = aspect > drawingAspect ? naturalHeight * aspect : naturalWidth
    const h = aspect > drawingAspect ? naturalHeight : naturalWidth / aspect
    setFitW(w)
    setBox({ x: (naturalWidth - w) / 2, y: (naturalHeight - h) / 2, w, h })
  }, [naturalWidth, naturalHeight])

  // Fit once, when the drawing first appears.
  useEffect(() => {
    if (ready) fit()
  }, [ready, fit])

  // On resize, keep where the reader is and what magnification they chose, and
  // only correct the aspect ratio. Re-fitting here would throw away their zoom
  // every time a phone rotated, a keyboard opened, or a window was dragged
  // wider, which is the moment somebody is most likely to be reading closely.
  useEffect(() => {
    const host = hostRef.current
    if (!host || !ready) return
    const ro = new ResizeObserver(() => {
      const aspect = host.clientWidth / Math.max(host.clientHeight, 1)
      setBox((prev) => {
        const h = prev.w / aspect
        return { ...prev, y: prev.y + (prev.h - h) / 2, h }
      })
    })
    ro.observe(host)
    return () => ro.disconnect()
  }, [ready])

  /** Zoom keeping the drawing point under (px, py), in host pixels, fixed. */
  const zoomAbout = useCallback(
    (factor: number, px: number, py: number) => {
      const host = hostRef.current
      if (!host) return
      setBox((prev) => {
        const minW = naturalWidth * MIN_SPAN_FRACTION
        const maxW = naturalWidth * MAX_SPAN_FRACTION
        const w = Math.min(Math.max(prev.w / factor, minW), maxW)
        const k = w / prev.w
        const h = prev.h * k
        // The fraction of the viewport the cursor sits at is the same fraction
        // of the viewBox, so the user-space point under it is recoverable.
        const fx = px / Math.max(host.clientWidth, 1)
        const fy = py / Math.max(host.clientHeight, 1)
        return {
          w,
          h,
          x: prev.x + (prev.w - w) * fx,
          y: prev.y + (prev.h - h) * fy,
        }
      })
    },
    [naturalWidth]
  )

  const zoomCentre = (factor: number) => {
    const host = hostRef.current
    if (!host) return
    zoomAbout(factor, host.clientWidth / 2, host.clientHeight / 2)
  }

  // Bound imperatively: React's onWheel is passive and a passive listener cannot
  // preventDefault, so the page would scroll away underneath the drawing.
  useEffect(() => {
    const host = hostRef.current
    if (!host) return
    const onWheel = (e: WheelEvent) => {
      e.preventDefault()
      const r = host.getBoundingClientRect()
      zoomAbout(Math.exp(-e.deltaY * 0.0015), e.clientX - r.left, e.clientY - r.top)
    }
    host.addEventListener('wheel', onWheel, { passive: false })
    return () => host.removeEventListener('wheel', onWheel)
  }, [zoomAbout])

  const onPointerDown = (e: React.PointerEvent) => {
    ;(e.currentTarget as Element).setPointerCapture?.(e.pointerId)
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY })
    if (pointers.current.size === 1) {
      setDragging(true)
      pan.current = { x: e.clientX, y: e.clientY, box }
    } else if (pointers.current.size === 2) {
      const [a, b] = [...pointers.current.values()]
      pinch.current = { dist: Math.hypot(a.x - b.x, a.y - b.y), w: box.w, h: box.h }
      pan.current = null
      setDragging(false)
    }
  }

  const onPointerMove = (e: React.PointerEvent) => {
    if (!pointers.current.has(e.pointerId)) return
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY })
    const host = hostRef.current
    if (!host) return

    if (pointers.current.size === 2 && pinch.current) {
      const [a, b] = [...pointers.current.values()]
      const dist = Math.hypot(a.x - b.x, a.y - b.y)
      const r = host.getBoundingClientRect()
      const target = pinch.current.w / (dist / pinch.current.dist)
      setBox((prev) => {
        const w = Math.min(
          Math.max(target, naturalWidth * MIN_SPAN_FRACTION),
          naturalWidth * MAX_SPAN_FRACTION
        )
        const k = w / prev.w
        const h = prev.h * k
        const fx = ((a.x + b.x) / 2 - r.left) / Math.max(host.clientWidth, 1)
        const fy = ((a.y + b.y) / 2 - r.top) / Math.max(host.clientHeight, 1)
        return { w, h, x: prev.x + (prev.w - w) * fx, y: prev.y + (prev.h - h) * fy }
      })
      return
    }

    if (pan.current) {
      const start = pan.current
      // A pixel of drag is worth however much user space one pixel currently
      // covers, so panning feels the same at every zoom level.
      const perPxX = start.box.w / Math.max(host.clientWidth, 1)
      const perPxY = start.box.h / Math.max(host.clientHeight, 1)
      setBox({
        ...start.box,
        x: start.box.x - (e.clientX - start.x) * perPxX,
        y: start.box.y - (e.clientY - start.y) * perPxY,
      })
    }
  }

  const endPointer = (e: React.PointerEvent) => {
    pointers.current.delete(e.pointerId)
    if (pointers.current.size < 2) pinch.current = null
    if (pointers.current.size === 0) {
      setDragging(false)
      pan.current = null
    }
  }

  const onKeyDown = (e: React.KeyboardEvent) => {
    const host = hostRef.current
    if (!host) return
    const nudge = (e.shiftKey ? 0.25 : 0.08) * box.w
    const nudgeY = (e.shiftKey ? 0.25 : 0.08) * box.h
    const moves: Record<string, [number, number]> = {
      ArrowLeft: [-nudge, 0],
      ArrowRight: [nudge, 0],
      ArrowUp: [0, -nudgeY],
      ArrowDown: [0, nudgeY],
    }
    if (moves[e.key]) {
      e.preventDefault()
      const [dx, dy] = moves[e.key]
      setBox((prev) => ({ ...prev, x: prev.x + dx, y: prev.y + dy }))
      return
    }
    if (e.key === '+' || e.key === '=') {
      e.preventDefault()
      zoomCentre(ZOOM_STEP)
    } else if (e.key === '-' || e.key === '_') {
      e.preventDefault()
      zoomCentre(1 / ZOOM_STEP)
    } else if (e.key === '0') {
      e.preventDefault()
      fit()
    }
  }

  const percent = Math.round((fitW / Math.max(box.w, 1)) * 100)

  const Button = ({
    onClick,
    label: aria,
    children,
  }: {
    onClick: () => void
    label: string
    children: React.ReactNode
  }) => (
    <button
      type="button"
      onClick={onClick}
      aria-label={aria}
      className="flex h-9 w-9 items-center justify-center rounded border border-[var(--color-line-bright)] bg-[var(--color-ink)]/85 font-mono text-sm text-[var(--color-body)] backdrop-blur transition-colors hover:border-[var(--color-hivis)] hover:text-[var(--color-hivis)] focus-visible:outline focus-visible:outline-2 focus-visible:outline-[var(--color-hivis)]"
    >
      {children}
    </button>
  )

  return (
    <figure className={`m-0 ${className}`}>
      {/* Matched to the paper colour both generators draw on, so the area around
          a fitted sheet reads as the sheet's margin rather than a rendering gap. */}
      <div
        className="relative overflow-hidden rounded-lg border border-[var(--color-line)]"
        style={{ background: '#f5f1ed' }}
      >
        <div
          ref={hostRef}
          role="img"
          aria-label={`${label}. ${description}`}
          tabIndex={0}
          onKeyDown={onKeyDown}
          onPointerDown={onPointerDown}
          onPointerMove={onPointerMove}
          onPointerUp={endPointer}
          onPointerCancel={endPointer}
          className="h-[65vh] max-h-[760px] min-h-[340px] w-full touch-none outline-none focus-visible:ring-2 focus-visible:ring-inset focus-visible:ring-[var(--color-hivis)] [&>svg]:h-full [&>svg]:w-full"
          style={{ cursor: dragging ? 'grabbing' : 'grab' }}
          dangerouslySetInnerHTML={{ __html: svg }}
        />

        <div className="no-print absolute right-3 top-3 flex flex-col gap-1.5">
          <Button onClick={() => zoomCentre(ZOOM_STEP)} label="Zoom in">
            +
          </Button>
          <Button onClick={() => zoomCentre(1 / ZOOM_STEP)} label="Zoom out">
            &minus;
          </Button>
          <Button onClick={fit} label="Fit the whole drawing">
            &#8853;
          </Button>
        </div>

        <div
          aria-live="polite"
          className="no-print pointer-events-none absolute bottom-3 left-3 rounded border border-[var(--color-line-bright)] bg-[var(--color-ink)]/85 px-2 py-1 font-mono text-xs text-[var(--color-muted)] backdrop-blur"
        >
          {percent}%
        </div>
      </div>

      <figcaption className="no-print mt-2 font-mono text-xs text-[var(--color-dim)]">
        Drag to pan, scroll or pinch to zoom. With the drawing focused: arrows pan, plus and minus
        zoom, 0 fits.
      </figcaption>
    </figure>
  )
}

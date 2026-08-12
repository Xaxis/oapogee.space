'use client'

import { useCallback, useEffect, useRef, useState } from 'react'

/**
 * Pan and zoom for the generated drawings.
 *
 * A schematic in a scroll box is not an interface. It is 1600 pixels wide, the
 * part designators are 8pt, and on a phone you get a letterbox onto one corner
 * of it with no way to find the other corner. The drawing is one of the most
 * useful things this project publishes and it was effectively desktop-only.
 *
 * So: drag to pan, wheel or pinch to zoom, buttons for everyone else, and
 * keyboard for anyone who cannot use a pointer at all. Fit is the default
 * because the first question is always "what am I looking at", and the answer to
 * that is the whole sheet.
 *
 * The transform is applied to a wrapper rather than to the SVG's viewBox, so the
 * browser composites it rather than re-rasterising vector geometry on every
 * frame, and so this works identically for the tscircuit schematic and the
 * generated block diagram without either knowing about it.
 */

const MIN_SCALE = 0.2
const MAX_SCALE = 8
const ZOOM_STEP = 1.4

type Transform = { x: number; y: number; scale: number }

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
  const viewportRef = useRef<HTMLDivElement>(null)
  const [t, setT] = useState<Transform>({ x: 0, y: 0, scale: 1 })
  const [fitScale, setFitScale] = useState(1)
  const [dragging, setDragging] = useState(false)

  // Live pointers, so two of them can be read as a pinch without a gesture
  // library. Pointer events cover mouse, touch and pen with one code path.
  const pointers = useRef(new Map<number, { x: number; y: number }>())
  const pinchStart = useRef<{ dist: number; scale: number } | null>(null)
  const panStart = useRef<{ x: number; y: number; tx: number; ty: number } | null>(null)

  const fit = useCallback(() => {
    const el = viewportRef.current
    if (!el) return
    const pad = 16
    const scale = Math.min(
      (el.clientWidth - pad) / naturalWidth,
      (el.clientHeight - pad) / naturalHeight
    )
    const next = Math.max(Math.min(scale, MAX_SCALE), MIN_SCALE)
    setFitScale(next)
    setT({
      x: (el.clientWidth - naturalWidth * next) / 2,
      y: (el.clientHeight - naturalHeight * next) / 2,
      scale: next,
    })
  }, [naturalWidth, naturalHeight])

  useEffect(() => {
    fit()
    const el = viewportRef.current
    if (!el) return
    const ro = new ResizeObserver(fit)
    ro.observe(el)
    return () => ro.disconnect()
  }, [fit])

  /** Zoom about a point in viewport coordinates, so the thing under the cursor stays under it. */
  const zoomAbout = useCallback((factor: number, cx: number, cy: number) => {
    setT((prev) => {
      const scale = Math.max(Math.min(prev.scale * factor, MAX_SCALE), MIN_SCALE)
      const k = scale / prev.scale
      return { scale, x: cx - (cx - prev.x) * k, y: cy - (cy - prev.y) * k }
    })
  }, [])

  const zoomCentre = (factor: number) => {
    const el = viewportRef.current
    if (!el) return
    zoomAbout(factor, el.clientWidth / 2, el.clientHeight / 2)
  }

  // Wheel is bound imperatively because React's onWheel is passive, and a
  // passive listener cannot preventDefault, so the page would scroll away
  // underneath the drawing while you tried to zoom it.
  useEffect(() => {
    const el = viewportRef.current
    if (!el) return
    const onWheel = (e: WheelEvent) => {
      e.preventDefault()
      const rect = el.getBoundingClientRect()
      zoomAbout(
        Math.exp(-e.deltaY * 0.0015),
        e.clientX - rect.left,
        e.clientY - rect.top
      )
    }
    el.addEventListener('wheel', onWheel, { passive: false })
    return () => el.removeEventListener('wheel', onWheel)
  }, [zoomAbout])

  const onPointerDown = (e: React.PointerEvent) => {
    ;(e.target as Element).setPointerCapture?.(e.pointerId)
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY })

    if (pointers.current.size === 1) {
      setDragging(true)
      panStart.current = { x: e.clientX, y: e.clientY, tx: t.x, ty: t.y }
    } else if (pointers.current.size === 2) {
      const [a, b] = [...pointers.current.values()]
      pinchStart.current = { dist: Math.hypot(a.x - b.x, a.y - b.y), scale: t.scale }
      panStart.current = null
      setDragging(false)
    }
  }

  const onPointerMove = (e: React.PointerEvent) => {
    if (!pointers.current.has(e.pointerId)) return
    pointers.current.set(e.pointerId, { x: e.clientX, y: e.clientY })

    if (pointers.current.size === 2 && pinchStart.current) {
      const [a, b] = [...pointers.current.values()]
      const dist = Math.hypot(a.x - b.x, a.y - b.y)
      const el = viewportRef.current
      if (!el) return
      const rect = el.getBoundingClientRect()
      const target = pinchStart.current.scale * (dist / pinchStart.current.dist)
      setT((prev) => {
        const scale = Math.max(Math.min(target, MAX_SCALE), MIN_SCALE)
        const k = scale / prev.scale
        const cx = (a.x + b.x) / 2 - rect.left
        const cy = (a.y + b.y) / 2 - rect.top
        return { scale, x: cx - (cx - prev.x) * k, y: cy - (cy - prev.y) * k }
      })
      return
    }

    if (panStart.current) {
      const s = panStart.current
      setT((prev) => ({ ...prev, x: s.tx + (e.clientX - s.x), y: s.ty + (e.clientY - s.y) }))
    }
  }

  const endPointer = (e: React.PointerEvent) => {
    pointers.current.delete(e.pointerId)
    if (pointers.current.size < 2) pinchStart.current = null
    if (pointers.current.size === 0) {
      setDragging(false)
      panStart.current = null
    }
  }

  const onKeyDown = (e: React.KeyboardEvent) => {
    const step = e.shiftKey ? 120 : 40
    const moves: Record<string, [number, number]> = {
      ArrowLeft: [step, 0],
      ArrowRight: [-step, 0],
      ArrowUp: [0, step],
      ArrowDown: [0, -step],
    }
    if (moves[e.key]) {
      e.preventDefault()
      const [dx, dy] = moves[e.key]
      setT((prev) => ({ ...prev, x: prev.x + dx, y: prev.y + dy }))
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

  const percent = Math.round((t.scale / (fitScale || 1)) * 100)

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
      className="flex h-8 w-8 items-center justify-center rounded border border-[var(--color-line-bright)] bg-[var(--color-ink)]/80 font-mono text-sm text-[var(--color-body)] backdrop-blur transition-colors hover:border-[var(--color-hivis)] hover:text-[var(--color-hivis)]"
    >
      {children}
    </button>
  )

  return (
    <figure className={`m-0 ${className}`}>
      {/* Matched to the paper colour both generators draw on, so the area
          around a fitted sheet reads as the sheet's margin rather than as a
          rendering gap. */}
      <div
        className="relative overflow-hidden rounded-lg border border-[var(--color-line)]"
        style={{ background: '#f5f1ed' }}
      >
        <div
          ref={viewportRef}
          role="img"
          aria-label={`${label}. ${description}`}
          tabIndex={0}
          onKeyDown={onKeyDown}
          onPointerDown={onPointerDown}
          onPointerMove={onPointerMove}
          onPointerUp={endPointer}
          onPointerCancel={endPointer}
          className="h-[60vh] max-h-[720px] min-h-[320px] w-full touch-none outline-none focus-visible:ring-2 focus-visible:ring-[var(--color-hivis)]"
          style={{ cursor: dragging ? 'grabbing' : 'grab' }}
        >
          <div
            aria-hidden
            className="origin-top-left will-change-transform [&>svg]:block [&>svg]:h-auto [&>svg]:w-auto"
            style={{
              transform: `translate3d(${t.x}px, ${t.y}px, 0) scale(${t.scale})`,
              width: naturalWidth,
              height: naturalHeight,
            }}
            dangerouslySetInnerHTML={{ __html: svg }}
          />
        </div>

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
          className="no-print pointer-events-none absolute bottom-3 left-3 rounded border border-[var(--color-line-bright)] bg-[var(--color-ink)]/80 px-2 py-1 font-mono text-xs text-[var(--color-muted)] backdrop-blur"
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

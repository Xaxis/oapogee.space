'use client'

import { useEffect, useRef, useState } from 'react'
import * as THREE from 'three'

/**
 * The flight state machine, in three dimensions.
 *
 * This is an explanation rather than an ornament, and it is deliberately not a
 * plot of anything. There is no oApogee flight data, because no oApogee has
 * flown, and a homepage that opened with a fabricated flight trace would
 * undermine the one claim this project actually makes: that every number on the
 * site is measured or sourced.
 *
 * So the curve is a diagram. It carries the qualitative shape of a rocket
 * flight, steep accelerating boost, decelerating coast, a rounded top, and a
 * slow descent under recovery, with no axes, no units, and no numbers anywhere.
 * What it teaches is the sequence in data/flight-phases.yaml: which phase
 * follows which, where apogee sits relative to burnout, and why the interesting
 * part of a flight is over in a handful of seconds. That is genuinely useful to
 * a newcomer and it claims nothing that has not been established.
 *
 * The traveller runs the loop continuously and the active phase is reported
 * back to the caller so the legend can follow it.
 *
 * Everything is disposed on unmount, the loop pauses when the canvas leaves the
 * viewport or the tab is hidden, motion is dropped entirely under
 * prefers-reduced-motion, and the module is dynamically imported so it never
 * blocks first paint.
 */

// Phase boundaries as fractions of the traverse. Chosen to read correctly, not
// to represent any particular motor: boost is a small fraction of the flight
// and descent is most of it, which is the proportion that surprises people.
//
// PAD_IDLE is deliberately absent. The traverse is the flight, and PAD_IDLE is
// everything before it starts, so it has no length here. It is still in the
// legend, where clicking it reads its summary.
const PHASE_STOPS = [
  { id: 'ARMED', at: 0.0 },
  { id: 'BOOST', at: 0.04 },
  { id: 'COAST', at: 0.16 },
  { id: 'APOGEE', at: 0.42 },
  { id: 'DESCENT', at: 0.46 },
  { id: 'LANDED', at: 0.97 },
]

const HIVIS = 0xffcf1b
const DIM = 0x3d434c

// The trajectory, as a function rather than as sampled points, so it can be
// evaluated at any resolution. u runs 0 to 1 across the whole flight.
function trajectory(u: number, out = new THREE.Vector3()): THREE.Vector3 {
  const APEX = 0.42 // u at apogee, matching PHASE_STOPS
  let h: number
  if (u <= APEX) {
    // Climb: fast and nearly linear through boost, flattening through coast.
    const t = u / APEX
    h = Math.sin(t * Math.PI * 0.5) ** 0.85
  } else {
    // Descent under recovery: near constant rate, which is the point of a
    // parachute, with a slight ease as it settles.
    const t = (u - APEX) / (1 - APEX)
    h = 1 - t ** 1.15
  }

  // Downrange drift. A rocket does not come down where it went up, and the
  // asymmetry between the climb and the descent is most of why walkaways
  // happen at all. The descent drifts several times further than the climb for
  // that reason, held to whatever keeps the whole path inside the frame.
  const drift = u <= APEX ? u * 0.4 : APEX * 0.4 + (u - APEX) * 1.55
  const lateral = Math.sin(u * Math.PI * 1.4) * 0.26

  return out.set(drift * 3.2 - 1.6, h * 3.2 - 1.5, lateral)
}

function phaseAt(u: number): string {
  let current = PHASE_STOPS[0].id
  for (const stop of PHASE_STOPS) if (u >= stop.at) current = stop.id
  return current
}

export default function FlightScene({ onPhase }: { onPhase?: (id: string) => void }) {
  const hostRef = useRef<HTMLDivElement>(null)
  const [failed, setFailed] = useState(false)
  const phaseRef = useRef<(id: string) => void>(() => {})
  phaseRef.current = onPhase ?? (() => {})

  useEffect(() => {
    const host = hostRef.current
    if (!host) return

    let renderer: THREE.WebGLRenderer
    try {
      renderer = new THREE.WebGLRenderer({
        antialias: true,
        alpha: true,
        powerPreference: 'low-power',
      })
    } catch {
      setFailed(true)
      return
    }

    const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches

    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    renderer.setSize(host.clientWidth, host.clientHeight, false)
    renderer.domElement.style.width = '100%'
    renderer.domElement.style.height = '100%'
    host.appendChild(renderer.domElement)

    const scene = new THREE.Scene()
    const camera = new THREE.PerspectiveCamera(40, host.clientWidth / host.clientHeight, 0.1, 100)

    // The path spans roughly x in [-1.6, 1.6] and y in [-1.5, 1.7], so the
    // frame is fitted to that box rather than to a guess. Distance is solved
    // from the field of view against whichever of the two dimensions is
    // tighter at the current aspect, which keeps the whole flight visible on a
    // wide desktop hero and on a narrow phone without separate cases.
    const TARGET = new THREE.Vector3(0, 0.1, 0)
    const HALF_W = 2.0
    const HALF_H = 1.9

    // An arrow const rather than a function declaration: a hoisted declaration
    // could in principle be called before the null check above, so TypeScript
    // discards the narrowing on `host` inside one.
    const fit = () => {
      const aspect = host.clientWidth / Math.max(host.clientHeight, 1)
      camera.aspect = aspect
      const vFov = (camera.fov * Math.PI) / 180
      const distForH = HALF_H / Math.tan(vFov / 2)
      const distForW = HALF_W / (Math.tan(vFov / 2) * aspect)
      const dist = Math.max(distForH, distForW) * 1.16
      camera.position.set(TARGET.x, TARGET.y + dist * 0.14, dist)
      camera.lookAt(TARGET)
      camera.updateProjectionMatrix()
      return dist
    }
    let baseDist = fit()

    const disposables: { dispose(): void }[] = []
    const track = <T extends { dispose(): void }>(x: T) => {
      disposables.push(x)
      return x
    }

    // --- ground plane -------------------------------------------------------
    // A reference the eye can read height against, which is the only reason it
    // is there. Kept very dim so the hero text stays the brightest thing.
    const grid = new THREE.GridHelper(18, 18, DIM, DIM)
    grid.position.y = -1.5
    ;(grid.material as THREE.Material).opacity = 0.25
    ;(grid.material as THREE.Material).transparent = true
    scene.add(grid)
    disposables.push(grid.geometry, grid.material as THREE.Material)

    // --- the trajectory -----------------------------------------------------
    const SEGMENTS = 420
    const points = Array.from({ length: SEGMENTS + 1 }, (_, i) => trajectory(i / SEGMENTS))

    // The full path, drawn dim. The travelled portion is redrawn bright on top,
    // so the shape is always legible and the progress reads as illumination
    // rather than as the line being built.
    const pathGeom = track(new THREE.BufferGeometry().setFromPoints(points))
    const pathMat = track(
      new THREE.LineBasicMaterial({ color: DIM, transparent: true, opacity: 0.9 })
    )
    scene.add(new THREE.Line(pathGeom, pathMat))

    const litGeom = track(new THREE.BufferGeometry().setFromPoints(points))
    litGeom.setDrawRange(0, 0)
    const litMat = track(new THREE.LineBasicMaterial({ color: HIVIS, transparent: true, opacity: 0.85 }))
    scene.add(new THREE.Line(litGeom, litMat))

    // --- apogee marker ------------------------------------------------------
    // The one labelled point. It is the number the whole payload exists to
    // produce, so it is the one thing in the scene that gets emphasis.
    const apex = trajectory(0.42)
    const ringGeom = track(new THREE.RingGeometry(0.16, 0.185, 48))
    const ringMat = track(
      new THREE.MeshBasicMaterial({ color: HIVIS, side: THREE.DoubleSide, transparent: true, opacity: 0.5 })
    )
    const ring = new THREE.Mesh(ringGeom, ringMat)
    ring.position.copy(apex)
    scene.add(ring)

    // A dropline from apogee to the ground plane. This is the altitude being
    // measured, drawn as a measurement rather than as decoration.
    const dropGeom = track(
      new THREE.BufferGeometry().setFromPoints([apex, new THREE.Vector3(apex.x, -1.5, apex.z)])
    )
    const dropMat = track(
      new THREE.LineDashedMaterial({
        color: HIVIS,
        transparent: true,
        opacity: 0.28,
        dashSize: 0.12,
        gapSize: 0.1,
      })
    )
    const drop = new THREE.Line(dropGeom, dropMat)
    drop.computeLineDistances()
    scene.add(drop)

    // --- the traveller ------------------------------------------------------
    const dotGeom = track(new THREE.SphereGeometry(0.062, 20, 20))
    const dotMat = track(new THREE.MeshBasicMaterial({ color: 0xffffff }))
    const dot = new THREE.Mesh(dotGeom, dotMat)
    scene.add(dot)

    const glowGeom = track(new THREE.SphereGeometry(0.15, 20, 20))
    const glowMat = track(
      new THREE.MeshBasicMaterial({ color: HIVIS, transparent: true, opacity: 0.22 })
    )
    const glow = new THREE.Mesh(glowGeom, glowMat)
    scene.add(glow)

    // --- loop ---------------------------------------------------------------
    let raf = 0
    let u = 0
    let lastPhase = ''
    let last = performance.now()

    // Two independent reasons to stop advancing, tracked separately because
    // either can clear without the other, and recombined on every event.
    let inView = true
    let shown = true
    let visible = true
    const sync = () => {
      visible = inView && shown
    }

    const CYCLE_MS = 17000
    const HOLD_MS = 1400 // pause on the ground before restarting

    // Everything that puts the scene into the state u describes, with no
    // scheduling in it, so that a single frame can be drawn without starting
    // the loop. That is what the reduced motion path below needs.
    function draw(now: number) {
      const clamped = Math.min(u, 1)
      const p = trajectory(clamped)
      dot.position.copy(p)
      glow.position.copy(p)

      litGeom.setDrawRange(0, Math.floor(clamped * SEGMENTS) + 1)

      const phase = phaseAt(clamped)
      if (phase !== lastPhase) {
        lastPhase = phase
        phaseRef.current(phase)
      }

      // Apogee pulses as the traveller reaches it, then settles.
      const nearApex = 1 - Math.min(Math.abs(clamped - 0.42) / 0.12, 1)
      ringMat.opacity = 0.35 + nearApex * 0.5
      ring.scale.setScalar(1 + nearApex * 0.25)
      ring.lookAt(camera.position)

      // A very slow orbit around the fitted distance. Enough to read as three
      // dimensional, slow enough not to compete with the text beside it.
      if (!reduced) {
        const a = now / 30000
        camera.position.x = TARGET.x + Math.sin(a) * baseDist * 0.13
        camera.position.z = Math.cos(a) * baseDist * 0.13 + baseDist
        camera.lookAt(TARGET)
      }

      renderer.render(scene, camera)
    }

    function frame(now: number) {
      raf = requestAnimationFrame(frame)
      const dt = Math.min(now - last, 64)
      last = now

      if (visible) {
        u += dt / CYCLE_MS
        if (u > 1 + HOLD_MS / CYCLE_MS) u = 0
      }

      draw(now)
    }

    // Under prefers-reduced-motion nothing moves: one frame is drawn and the
    // loop is never started. It is held at apogee because that is the one
    // labelled point in the scene, and drawing it reports APOGEE to the legend,
    // so the phase buttons remain the way to read the rest of the sequence.
    if (reduced) {
      u = 0.42 // apogee, matching PHASE_STOPS
      draw(performance.now())
    } else {
      raf = requestAnimationFrame(frame)
    }

    // --- lifecycle ----------------------------------------------------------
    // An IntersectionObserver only fires on a threshold crossing, and returning
    // to a backgrounded tab is not one, so the tab flag has to restore itself.
    // Clearing a single shared flag on hide with nothing to set it back froze
    // the scene permanently after the first tab switch.
    const io = new IntersectionObserver(
      ([e]) => {
        inView = e.isIntersecting
        sync()
      },
      { threshold: 0.05 }
    )
    io.observe(host)

    const onVisibility = () => {
      shown = !document.hidden
      sync()
    }
    document.addEventListener('visibilitychange', onVisibility)

    const ro = new ResizeObserver(() => {
      const { clientWidth: w, clientHeight: h } = host
      if (!w || !h) return
      renderer.setSize(w, h, false)
      baseDist = fit()
      // fit() moves the camera, and with the loop stopped nothing else will
      // repaint or re-aim the billboarded apogee ring.
      if (reduced) draw(performance.now())
    })
    ro.observe(host)

    return () => {
      cancelAnimationFrame(raf)
      io.disconnect()
      ro.disconnect()
      document.removeEventListener('visibilitychange', onVisibility)
      for (const d of disposables) d.dispose()
      renderer.dispose()
      host.removeChild(renderer.domElement)
    }
  }, [])

  if (failed) return null
  return <div ref={hostRef} className="h-full w-full" aria-hidden />
}

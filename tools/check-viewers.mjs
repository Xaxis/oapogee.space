#!/usr/bin/env node
/**
 * Every drawing viewer actually pans, zooms, and has the drawing in the frame.
 *
 * This exists because the viewer has now broken three times, each time in a way
 * no other check could see. Once React re-applied dangerouslySetInnerHTML and
 * detached the SVG the effect was mutating, so the readout counted up and the
 * drawing never moved. Once the PCB export put a 213 unit board inside an 800
 * unit canvas and the viewer assumed the drawing started at the origin, so it
 * framed empty space beside it. And once fit() ran while a tab was hidden,
 * where the host has no width: dividing by it gave an infinite viewBox, the
 * next resize computed -Infinity + Infinity, and an SVG with NaN in its viewBox
 * draws nothing at all.
 *
 * Every one of those built cleanly, type-checked, rendered a plausible page and
 * passed every other check in this repository. The only thing that finds them
 * is driving a real browser and asserting the viewBox moves, which is what this
 * does. It is the same Chrome DevTools Protocol approach as check-responsive.
 *
 *   node tools/check-viewers.mjs http://localhost:3000
 */

import { spawn } from 'node:child_process'

import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const arg = process.argv.find((a) => a.startsWith('--base='))
let BASE = arg?.slice('--base='.length) ?? process.argv[2]
let server

async function waitFor(url, tries = 60) {
  for (let i = 0; i < tries; i++) {
    try {
      if ((await fetch(url)).ok) return
    } catch {
      /* not up yet */
    }
    await new Promise((r) => setTimeout(r, 500))
  }
  throw new Error(`timed out waiting for ${url}`)
}

// Self-contained, the same way check-responsive is: a check somebody has to
// remember to start a server for is a check that stops being run.
if (!BASE) {
  const port = 4188
  BASE = `http://127.0.0.1:${port}`
  server = spawn('yarn', ['workspace', '@oapogee/web', 'start', '-p', String(port)], {
    cwd: ROOT,
    stdio: 'ignore',
    detached: true,
  })
  await waitFor(`${BASE}/`).catch(() => {
    throw new Error('the site server did not start. Run `make build` first.')
  })
}
const CHROME =
  process.env.CHROME_PATH ?? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
const PORT = 9422

// Pages carrying an interactive drawing. A page with none is not an error: the
// count is asserted per page so a viewer that silently stops rendering is a
// failure rather than a quiet pass.
const PAGES = [{ path: '/reference/schematic', viewers: 3 }]

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))

const chrome = spawn(
  CHROME,
  [
    `--remote-debugging-port=${PORT}`,
    '--headless=new',
    '--no-first-run',
    '--window-size=1400,1000',
    '--user-data-dir=/tmp/oapogee-viewer-check',
    'about:blank',
  ],
  { stdio: 'ignore' }
)

let wsUrl
for (let i = 0; i < 80 && !wsUrl; i++) {
  try {
    const list = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()
    wsUrl = list.find((t) => t.type === 'page')?.webSocketDebuggerUrl
  } catch {
    /* not up yet */
  }
  if (!wsUrl) await sleep(250)
}
if (!wsUrl) {
  console.error('Could not start Chrome. Set CHROME_PATH if it is somewhere unusual.')
  process.exit(1)
}

const sock = new WebSocket(wsUrl)
await new Promise((r) => (sock.onopen = r))
let msgId = 0
const pending = new Map()
sock.onmessage = (m) => {
  const d = JSON.parse(m.data)
  if (pending.has(d.id)) {
    pending.get(d.id)(d)
    pending.delete(d.id)
  }
}
const send = (method, params = {}) =>
  new Promise((res) => {
    const id = ++msgId
    pending.set(id, res)
    sock.send(JSON.stringify({ id, method, params }))
  })
const js = async (expression) =>
  (await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }))?.result
    ?.result?.value

await send('Page.enable')
await send('Runtime.enable')

let failures = 0

for (const page of PAGES) {
  const url = `${BASE}${page.path}`
  await send('Page.navigate', { url })
  await sleep(4500)

  const count = await js(`document.querySelectorAll('svg[viewBox]').length`)
  if (count !== page.viewers) {
    console.error(`error ${page.path}: expected ${page.viewers} viewers, found ${count}`)
    failures++
    continue
  }

  for (let i = 0; i < count; i++) {
    const label =
      (await js(
        `(() => { const t=[...document.querySelectorAll('[role="tab"]')][${i}]; return t ? t.textContent.trim() : 'viewer ${i}' })()`
      )) ?? `viewer ${i}`

    // Open the tab this viewer lives in and put it on screen. A hidden tab has
    // no layout, and an element below the fold receives no input at all, which
    // is a way to write a test that passes while measuring nothing.
    await js(
      `(() => { const t=[...document.querySelectorAll('[role="tab"]')]; if (t[${i}]) t[${i}].click(); return 1 })()`
    )
    await sleep(700)
    await js(
      `(() => { const s=document.querySelectorAll('svg[viewBox]')[${i}];
        (s.closest('[tabindex]') || s.parentElement).scrollIntoView({ block: 'center' }); return 1 })()`
    )
    await sleep(400)

    const viewBox = () =>
      js(`document.querySelectorAll('svg[viewBox]')[${i}].getAttribute('viewBox')`)

    const start = await viewBox()
    if (!start || /NaN|Infinity/.test(start)) {
      console.error(`error ${page.path} ${label}: viewBox is "${start}"`)
      failures++
      continue
    }

    // How much of the drawing the fitted view actually contains.
    const framed = await js(`(() => {
      const s = document.querySelectorAll('svg[viewBox]')[${i}]
      const [x, y, w, h] = s.getAttribute('viewBox').split(/\\s+/).map(Number)
      let hit = 0, total = 0
      for (const el of s.querySelectorAll('rect,path,circle,line,polygon')) {
        let b; try { b = el.getBBox() } catch { continue }
        if (!b || (b.width === 0 && b.height === 0)) continue
        total++
        if (b.x + b.width > x && b.x < x + w && b.y + b.height > y && b.y < y + h) hit++
      }
      return total ? Math.round(100 * hit / total) : -1
    })()`)

    await js(`(() => { const s=document.querySelectorAll('svg[viewBox]')[${i}]; const r=s.getBoundingClientRect();
      const cy = Math.min(Math.max(r.top + r.height / 2, 10), innerHeight - 10)
      const e = new WheelEvent('wheel', { deltaY: -400, clientX: r.left + r.width / 2, clientY: cy, bubbles: true, cancelable: true })
      s.dispatchEvent(e); s.parentElement && s.parentElement.dispatchEvent(e); return 1 })()`)
    await sleep(450)
    const zoomedIn = await viewBox()

    await js(`(() => { const s=document.querySelectorAll('svg[viewBox]')[${i}]
      let root = s
      for (let k = 0; k < 8 && root; k++) { if (root.querySelector('button[aria-label="Zoom out"]')) break; root = root.parentElement }
      const b = root && root.querySelector('button[aria-label="Zoom out"]')
      if (b) { b.click(); b.click() }
      return !!b })()`)
    await sleep(450)
    const zoomedOut = await viewBox()

    // A real mouse drag. Synthetic pointer events do not exercise pointer
    // capture and can be swallowed by it.
    const at = JSON.parse(
      await js(`(() => { const r=document.querySelectorAll('svg[viewBox]')[${i}].getBoundingClientRect();
        return JSON.stringify({ x: r.left + r.width / 2, y: Math.min(Math.max(r.top + r.height / 2, 10), innerHeight - 10) }) })()`)
    )
    await send('Input.dispatchMouseEvent', { type: 'mousePressed', x: at.x, y: at.y, button: 'left', buttons: 1, clickCount: 1 })
    for (const d of [25, 60, 95, 130]) {
      await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: at.x + d, y: at.y + d, button: 'left', buttons: 1 })
      await sleep(40)
    }
    await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: at.x + 130, y: at.y + 130, button: 'left', buttons: 0, clickCount: 1 })
    await sleep(450)
    const panned = await viewBox()

    const problems = []
    if (start === zoomedIn) problems.push('the wheel does not zoom in')
    if (zoomedIn === zoomedOut) problems.push('the zoom out button does nothing')
    if (zoomedOut === panned) problems.push('dragging does not pan')
    if (framed < 60) problems.push(`only ${framed}% of the drawing is inside the fitted view`)
    if (/NaN|Infinity/.test(panned)) problems.push(`viewBox became "${panned}"`)

    if (problems.length) {
      for (const p of problems) console.error(`error ${page.path} [${label}]: ${p}`)
      failures += problems.length
    } else {
      console.log(`  ${page.path} [${label}]: pans, zooms both ways, drawing ${framed}% in frame`)
    }
  }
}

chrome.kill()
if (server) {
  try {
    process.kill(-server.pid)
  } catch {
    /* already gone */
  }
}

if (failures) {
  console.error(`\n${failures} viewer problem${failures === 1 ? '' : 's'}.`)
  process.exit(1)
}
console.log('Every drawing viewer pans, zooms in and out, and frames its drawing.')
process.exit(0)

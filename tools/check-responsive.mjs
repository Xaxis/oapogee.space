#!/usr/bin/env node
/**
 * No page scrolls sideways on a phone.
 *
 * This exists because the site did exactly that and nothing caught it. A flex
 * item does not shrink below its content unless told to, so one 900px schematic
 * inside an `overflow-x-auto` container quietly widened every section around it
 * and pushed the whole document off the right edge. Every local check was green,
 * the desktop screenshots were fine, and the site was unusable on a phone.
 *
 * A rule that is only enforced by remembering to look is not enforced. So this
 * drives a real browser at a real phone width and measures the real layout.
 *
 * It speaks the DevTools protocol directly rather than pulling in a browser
 * automation library. The check needs three protocol calls, and a dependency
 * that ships its own Chromium would be larger than the rest of this repository.
 *
 *   node tools/check-responsive.mjs            start a server, check, tear down
 *   node tools/check-responsive.mjs --base=... check a running server
 */

import { spawn, execFileSync } from 'node:child_process'
import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))

// 320 is the narrowest viewport still worth supporting, and it is where layout
// bugs show first. 390 is a current iPhone. Checking both catches the class of
// bug that only appears under a hard minimum width.
const WIDTHS = [320, 390]

const ROUTES = [
  '/',
  '/start',
  '/bom',
  '/build',
  '/firmware',
  '/mounting',
  '/ground-station',
  '/preflight',
  '/safety',
  '/data',
  '/troubleshooting',
  '/flights',
  '/reference',
  '/reference/schematic',
  '/reference/telemetry-packet',
  '/reference/log-format',
  '/glossary',
  '/faq',
  '/changelog',
  '/about',
]

const CHROME_CANDIDATES = [
  process.env.CHROME_PATH,
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium-browser',
  '/usr/bin/chromium',
].filter(Boolean)

const chrome = CHROME_CANDIDATES.find((p) => existsSync(p))
if (!chrome) {
  console.error('No Chrome found. Set CHROME_PATH, or skip this check locally.')
  process.exit(1)
}

const arg = process.argv.find((a) => a.startsWith('--base='))
let base = arg?.slice('--base='.length)
let server

async function waitFor(url, tries = 60) {
  for (let i = 0; i < tries; i++) {
    try {
      const res = await fetch(url)
      if (res.ok) return await res.json().catch(() => ({}))
    } catch {
      // not up yet
    }
    await new Promise((r) => setTimeout(r, 500))
  }
  throw new Error(`timed out waiting for ${url}`)
}

if (!base) {
  const port = 4177
  base = `http://127.0.0.1:${port}`
  server = spawn('yarn', ['workspace', '@oapogee/web', 'start', '-p', String(port)], {
    cwd: ROOT,
    stdio: 'ignore',
    detached: true,
  })
  await waitFor(`${base}/`).catch(async () => {
    // The root returns HTML, not JSON, so waitFor's parse gives {}. A failure
    // here means the server genuinely never came up.
    throw new Error('the site server did not start. Run `make build` first.')
  })
}

// --- drive the browser ------------------------------------------------------

const DEBUG_PORT = 9333
const browser = spawn(
  chrome,
  [
    '--headless=new',
    '--no-first-run',
    '--no-default-browser-check',
    '--disable-gpu',
    '--hide-scrollbars',
    // CI runners run as a user without the kernel namespaces Chrome's sandbox
    // needs, and /dev/shm is small in containers. Without these the browser
    // exits before it opens the debug port, and the only symptom is a timeout
    // that says nothing about why.
    '--no-sandbox',
    '--disable-dev-shm-usage',
    `--remote-debugging-port=${DEBUG_PORT}`,
    '--user-data-dir=/tmp/oapogee-responsive-profile',
    'about:blank',
  ],
  { stdio: ['ignore', 'ignore', 'pipe'] }
)

// Kept so a startup failure can say what the browser actually complained about
// rather than only that it never appeared.
let browserStderr = ''
browser.stderr.on('data', (chunk) => {
  browserStderr += chunk.toString()
})

const cleanup = () => {
  try {
    browser.kill()
  } catch {}
  if (server) {
    try {
      process.kill(-server.pid)
    } catch {}
  }
}
process.on('exit', cleanup)
process.on('SIGINT', () => {
  cleanup()
  process.exit(130)
})

const version = await waitFor(`http://127.0.0.1:${DEBUG_PORT}/json/version`).catch((e) => {
  console.error(`Chrome never opened its debug port: ${e.message}`)
  console.error(`  binary: ${chrome}`)
  if (browserStderr.trim()) console.error(browserStderr.trim().split('\n').slice(-12).join('\n'))
  cleanup()
  process.exit(1)
})
const ws = new WebSocket(version.webSocketDebuggerUrl)
await new Promise((resolve, reject) => {
  ws.onopen = resolve
  ws.onerror = reject
})

let nextId = 1
const pending = new Map()
const events = new Map()

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data)
  if (msg.id && pending.has(msg.id)) {
    const { resolve, reject } = pending.get(msg.id)
    pending.delete(msg.id)
    msg.error ? reject(new Error(msg.error.message)) : resolve(msg.result)
    return
  }
  if (msg.method) {
    const waiters = events.get(msg.method) ?? []
    events.set(msg.method, [])
    for (const w of waiters) w(msg.params)
  }
}

const send = (method, params = {}, sessionId) =>
  new Promise((resolve, reject) => {
    const id = nextId++
    pending.set(id, { resolve, reject })
    ws.send(JSON.stringify({ id, method, params, sessionId }))
  })

const once = (method) =>
  new Promise((resolve) => {
    const waiters = events.get(method) ?? []
    waiters.push(resolve)
    events.set(method, waiters)
  })

const { targetId } = await send('Target.createTarget', { url: 'about:blank' })
const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true })
await send('Page.enable', {}, sessionId)
await send('Runtime.enable', {}, sessionId)

const failures = []
let checked = 0

for (const width of WIDTHS) {
  await send(
    'Emulation.setDeviceMetricsOverride',
    { width, height: 900, deviceScaleFactor: 1, mobile: true },
    sessionId
  )

  for (const route of ROUTES) {
    const loaded = once('Page.loadEventFired')
    await send('Page.navigate', { url: `${base}${route}` }, sessionId)
    await Promise.race([loaded, new Promise((r) => setTimeout(r, 15000))])
    // The three.js hero mounts after load and could in principle change layout.
    await new Promise((r) => setTimeout(r, 350))

    const { result } = await send(
      'Runtime.evaluate',
      {
        returnByValue: true,
        expression: `(() => {
          const doc = document.documentElement
          const limit = doc.clientWidth

          // Content that sticks out of a container which scrolls horizontally
          // is doing exactly what it should: a wide schematic or a wide table
          // scrolls inside its own box. Only elements with no scrollable
          // ancestor actually widen the document, so only those are offenders.
          const insideScroller = (el) => {
            for (let p = el.parentElement; p && p !== document.body; p = p.parentElement) {
              const ox = getComputedStyle(p).overflowX
              if (ox === 'auto' || ox === 'scroll' || ox === 'hidden' || ox === 'clip') return true
            }
            return false
          }

          const over = []
          for (const el of document.querySelectorAll('body *')) {
            const r = el.getBoundingClientRect()
            if (r.width === 0 || r.right <= limit + 1) continue
            if (insideScroller(el)) continue
            over.push({
              tag: el.tagName.toLowerCase(),
              cls: (el.getAttribute('class') || '').slice(0, 70),
              right: Math.round(r.right),
            })
          }

          // The outermost offenders are the ones worth naming; a parent that is
          // too wide drags all its children into the list.
          over.sort((a, b) => b.right - a.right)
          return { scrollWidth: doc.scrollWidth, clientWidth: limit, offenders: over.slice(0, 4) }
        })()`,
      },
      sessionId
    )

    checked++
    const { scrollWidth, clientWidth, offenders } = result.value
    // One pixel of slack: subpixel rounding on borders is not a layout bug.
    if (scrollWidth > clientWidth + 1) {
      failures.push({ route, width, scrollWidth, clientWidth, offenders })
    }
  }
}

ws.close()
cleanup()

if (failures.length) {
  for (const f of failures) {
    console.error(
      `error ${f.route} at ${f.width}px scrolls sideways: ` +
        `document is ${f.scrollWidth}px wide in a ${f.clientWidth}px viewport`
    )
    for (const o of f.offenders) {
      console.error(`        ${o.tag}.${o.cls || '(no class)'} extends to ${o.right}px`)
    }
  }
  console.error(`\n${failures.length} page/width combinations overflow horizontally.`)
  process.exit(1)
}

console.log(`No horizontal overflow across ${checked} page and width combinations.`)
process.exit(0)

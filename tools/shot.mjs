#!/usr/bin/env node
/**
 * Screenshot a page at a real device width.
 *
 * `chrome --headless --screenshot --window-size=390,x` does not do this.
 * Headless Chrome enforces a minimum window width, so a narrow window renders a
 * wider layout and crops it, which looks exactly like a page that fails to wrap
 * and sends you chasing a responsive bug that is not there. Emulation through
 * the DevTools protocol sets the layout viewport properly.
 *
 *   node tools/shot.mjs <url> <out.png> [width] [height]
 */

import { spawn } from 'node:child_process'
import { writeFileSync, existsSync } from 'node:fs'

const [url, out, width = '390', height = '1600'] = process.argv.slice(2)
if (!url || !out) {
  console.error('usage: node tools/shot.mjs <url> <out.png> [width] [height]')
  process.exit(1)
}

const chrome = [
  process.env.CHROME_PATH,
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
]
  .filter(Boolean)
  .find((p) => existsSync(p))

if (!chrome) {
  console.error('No Chrome found. Set CHROME_PATH.')
  process.exit(1)
}

const PORT = 9334
const browser = spawn(
  chrome,
  [
    '--headless=new',
    '--no-first-run',
    '--no-default-browser-check',
    '--disable-gpu',
    '--enable-unsafe-swiftshader',
    '--hide-scrollbars',
    '--no-sandbox',
    `--remote-debugging-port=${PORT}`,
    '--user-data-dir=/tmp/oapogee-shot-profile',
    'about:blank',
  ],
  { stdio: 'ignore' }
)

const done = (code) => {
  try {
    browser.kill()
  } catch {}
  process.exit(code)
}

async function waitFor(u, tries = 60) {
  for (let i = 0; i < tries; i++) {
    try {
      const res = await fetch(u)
      if (res.ok) return await res.json()
    } catch {}
    await new Promise((r) => setTimeout(r, 300))
  }
  throw new Error(`timed out waiting for ${u}`)
}

const version = await waitFor(`http://127.0.0.1:${PORT}/json/version`)
const ws = new WebSocket(version.webSocketDebuggerUrl)
await new Promise((res, rej) => {
  ws.onopen = res
  ws.onerror = rej
})

let id = 1
const pending = new Map()
const waiters = new Map()
ws.onmessage = (e) => {
  const m = JSON.parse(e.data)
  if (m.id && pending.has(m.id)) {
    const { resolve, reject } = pending.get(m.id)
    pending.delete(m.id)
    m.error ? reject(new Error(m.error.message)) : resolve(m.result)
  } else if (m.method) {
    const list = waiters.get(m.method) ?? []
    waiters.set(m.method, [])
    list.forEach((fn) => fn(m.params))
  }
}
const send = (method, params = {}, sessionId) =>
  new Promise((resolve, reject) => {
    const n = id++
    pending.set(n, { resolve, reject })
    ws.send(JSON.stringify({ id: n, method, params, sessionId }))
  })
const once = (method) =>
  new Promise((resolve) => {
    const list = waiters.get(method) ?? []
    list.push(resolve)
    waiters.set(method, list)
  })

const { targetId } = await send('Target.createTarget', { url: 'about:blank' })
const { sessionId } = await send('Target.attachToTarget', { targetId, flatten: true })
await send('Page.enable', {}, sessionId)
await send(
  'Emulation.setDeviceMetricsOverride',
  { width: Number(width), height: Number(height), deviceScaleFactor: 2, mobile: Number(width) < 700 },
  sessionId
)

const loaded = once('Page.loadEventFired')
await send('Page.navigate', { url }, sessionId)
await Promise.race([loaded, new Promise((r) => setTimeout(r, 20000))])
await new Promise((r) => setTimeout(r, 1200))

// Optional script run before the capture, so an interaction can be driven and
// then photographed: --eval='document.querySelector("[aria-label=Zoom in]").click()'
const evalArg = process.argv.find((a) => a.startsWith('--eval='))
if (evalArg) {
  await send('Runtime.enable', {}, sessionId)
  const r = await send(
    'Runtime.evaluate',
    { expression: evalArg.slice('--eval='.length), awaitPromise: true, returnByValue: true },
    sessionId
  )
  if (r.exceptionDetails) console.error('eval threw:', r.exceptionDetails.text)
  await new Promise((r) => setTimeout(r, 800))
}

const { data } = await send(
  'Page.captureScreenshot',
  { format: 'png', captureBeyondViewport: !process.argv.includes('--viewport') },
  sessionId
)
writeFileSync(out, Buffer.from(data, 'base64'))
console.log(`wrote ${out} at ${width}x${height}`)
ws.close()
done(0)

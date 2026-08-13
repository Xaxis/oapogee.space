#!/usr/bin/env node
/**
 * Every supplier link this site asserts still resolves.
 *
 * Deliberately NOT part of `make check` and not run in CI. A build that fails
 * because somebody else's website is having a bad morning teaches contributors
 * to ignore a red build, which costs far more than a stale link. Run it when
 * touching the bill of materials, and on whatever schedule suits.
 *
 * What it checks, and what it cannot: that the URL responds. It says nothing
 * about whether the page still describes the part it did when somebody opened
 * it, which is why every product link carries the date it was checked and why
 * this prints those dates rather than pretending to have re-read the pages.
 *
 *   node tools/check-suppliers.mjs
 */

import { readFileSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const bom = parse(readFileSync(join(ROOT, 'data/bom.yaml'), 'utf8'))
const suppliers = parse(readFileSync(join(ROOT, 'data/suppliers.yaml'), 'utf8'))

// Some retailers refuse a request with no user agent, which is not a broken
// link and must not be reported as one.
const UA =
  'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124 Safari/537.36'

const targets = []

for (const part of bom.parts ?? []) {
  if (part.breakout?.url) {
    targets.push({ url: part.breakout.url, what: `${part.id} breakout`, checked: part.breakout.checked })
  }
  for (const sub of part.substitutes ?? []) {
    if (sub.url) targets.push({ url: sub.url, what: `${part.id} substitute ${sub.mpn}` })
  }
}

// One search per supplier, using a part number that is actually on this site, to
// confirm the template still works rather than every part individually.
const sample = (bom.parts ?? []).find((p) => p.mpn && p.confidence === 'asserted')?.mpn
for (const sup of [...(suppliers.distributors ?? []), ...(suppliers.makers ?? [])]) {
  if (sample) {
    targets.push({
      url: sup.search.replace('{mpn}', encodeURIComponent(sample)),
      what: `${sup.name} search template`,
    })
  }
}

const results = await Promise.all(
  targets.map(async (t) => {
    try {
      const res = await fetch(t.url, {
        redirect: 'follow',
        headers: { 'user-agent': UA },
        signal: AbortSignal.timeout(30000),
      })
      // 403 from a retailer is bot protection, not a dead link. Mouser serves
      // this URL to a browser and refuses it here, and reporting that as broken
      // would train somebody to ignore this check, which costs more than the
      // stale link it exists to catch. Reported separately and not as a failure.
      const blocked = res.status === 403 || res.status === 429
      return { ...t, status: res.status, ok: res.ok || blocked, blocked }
    } catch (e) {
      return { ...t, status: 0, ok: false, error: e instanceof Error ? e.message : String(e) }
    }
  })
)

let bad = 0
for (const r of results) {
  const mark = r.blocked ? 'bot ' : r.ok ? 'ok  ' : 'FAIL'
  const when = r.checked ? `  (page read ${r.checked})` : ''
  console.log(`${mark} ${String(r.status).padStart(3)}  ${r.what}${when}\n       ${r.url}`)
  if (!r.ok) bad++
}

if (bad) {
  console.error(
    `\n${bad} supplier link${bad === 1 ? '' : 's'} did not resolve. ` +
      `Check by hand before changing anything: a retailer blocking automated requests ` +
      `looks identical to a dead link from here.`
  )
  process.exit(1)
}

const blocked = results.filter((r) => r.blocked).length
console.log(
  `\nAll ${results.length} supplier links resolve` +
    (blocked
      ? `, ${blocked} of them behind bot protection that refuses automated requests but serves a browser.`
      : '.')
)

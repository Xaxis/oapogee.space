#!/usr/bin/env node
//
// Every internal link resolves, and every anchor exists on the page it points
// at.
//
// Cross-page links are how this site works: a build checkpoint sends you to a
// troubleshooting symptom, a glossary term is linked on first use, a safety
// claim points at the section that justifies it. A dead one is worse here than
// on most sites, because it appears exactly when somebody is stuck and looking
// for the thing it promised.
//
// Anchors are checked, not just paths. A link to /safety that lands on the page
// is not the same as a link to /safety#radio that lands on the radio section,
// and the second is the one that silently rots when a heading is reworded.

import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs'
import { join, relative, extname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const CONTENT = join(ROOT, 'content')
const SPEC = join(ROOT, 'docs/spec')
const DATA = join(ROOT, 'data')

const errors = []

// The route table lives in the web app because that is what consumes it, and it
// is TypeScript, which this plain-node tool cannot import. Parsing it is a small
// contained ugliness, and the failure mode is a false positive rather than a
// missed one: a route this misses gets reported as unknown and a human looks.
function loadRoutes() {
  const src = readFileSync(join(ROOT, 'apps/web/lib/routes.ts'), 'utf8')

  const content = new Map()
  for (const m of src.matchAll(/\{\s*slug:\s*'([^']+)',\s*file:\s*'([^']+)'/g)) {
    content.set(`/${m[1]}`, m[2])
  }

  const generated = new Set(['/'])
  for (const m of src.matchAll(/\{\s*href:\s*'([^']+)'/g)) generated.add(m[1])

  if (content.size === 0) errors.push('could not parse any content routes from routes.ts')
  return { content, generated }
}

const { content: CONTENT_ROUTES, generated: GENERATED_ROUTES } = loadRoutes()

// Matches github-slugger, which is what rehype-slug uses to build heading ids.
const slugify = (s) =>
  s
    .toLowerCase()
    .trim()
    .replace(/[^\p{L}\p{N}\s-]/gu, '')
    .replace(/\s+/g, '-')

function headingAnchors(markdown) {
  const anchors = new Set()
  const seen = new Map()
  let inFence = false
  for (const line of markdown.split('\n')) {
    if (/^\s*```/.test(line)) {
      inFence = !inFence
      continue
    }
    if (inFence) continue
    const m = /^(#{1,6})\s+(.+?)\s*$/.exec(line)
    if (!m) continue
    // Strip inline markup the same way the renderer does before slugging.
    const text = m[2].replace(/`([^`]*)`/g, '$1').replace(/\*\*?([^*]*)\*\*?/g, '$1')
    const base = slugify(text)
    const n = seen.get(base) ?? 0
    seen.set(base, n + 1)
    anchors.add(n === 0 ? base : `${base}-${n}`)
  }
  return anchors
}

// Generated pages get their anchors from the data that produced them, which is
// the only way to check them without rendering the site.
function generatedAnchors() {
  const yaml = (f) => parse(readFileSync(join(DATA, f), 'utf8'))
  const out = new Map()

  out.set('/troubleshooting', new Set(yaml('troubleshooting.yaml').entries.map((e) => e.id)))
  out.set('/glossary', new Set(yaml('glossary.yaml').terms.map((t) => t.id)))
  out.set(
    '/preflight',
    new Set(yaml('preflight.yaml').sections.flatMap((s) => s.items.map((i) => i.id)))
  )
  out.set('/bom', new Set())
  out.set('/flights', new Set())
  out.set('/status', new Set())
  out.set('/', new Set())
  return out
}

const GENERATED_ANCHORS = generatedAnchors()

// Anchors available on each content route, from its markdown source.
const CONTENT_ANCHORS = new Map()
for (const [route, file] of CONTENT_ROUTES) {
  const path = join(CONTENT, `${file}.md`)
  if (!existsSync(path)) {
    errors.push(`route ${route} points at content/${file}.md, which does not exist`)
    continue
  }
  CONTENT_ANCHORS.set(route, headingAnchors(readFileSync(path, 'utf8')))
}

// The two specs render at nested routes from docs/spec/.
const SPEC_ROUTES = {
  '/reference/telemetry-packet': 'telemetry-packet',
  '/reference/log-format': 'log-format',
}
for (const [route, file] of Object.entries(SPEC_ROUTES)) {
  const path = join(SPEC, `${file}.md`)
  if (!existsSync(path)) {
    errors.push(`route ${route} points at docs/spec/${file}.md, which does not exist`)
    continue
  }
  CONTENT_ANCHORS.set(route, headingAnchors(readFileSync(path, 'utf8')))
}

function anchorsFor(route) {
  return CONTENT_ANCHORS.get(route) ?? GENERATED_ANCHORS.get(route) ?? null
}

function knownRoute(route) {
  return CONTENT_ROUTES.has(route) || GENERATED_ROUTES.has(route) || route in SPEC_ROUTES
}

// --- walk every source that can contain a link ------------------------------

function walk(dir, acc = []) {
  if (!existsSync(dir)) return acc
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry)
    if (statSync(full).isDirectory()) walk(full, acc)
    else if (['.md', '.yaml', '.yml'].includes(extname(full))) acc.push(full)
  }
  return acc
}

const files = [...walk(CONTENT), ...walk(SPEC), ...walk(DATA)]

let checked = 0
for (const file of files) {
  const rel = relative(ROOT, file)
  const text = readFileSync(file, 'utf8')

  // Markdown inline links only. Bare paths in prose are not links and are not
  // the checker's business.
  for (const m of text.matchAll(/\[[^\]]*\]\(([^)\s]+)\)/g)) {
    const href = m[1]

    // External, mail, and relative repository paths are out of scope. A relative
    // path like ../../content/safety.md is a link between files in a checkout,
    // which resolves on GitHub and is checked by existence rather than routing.
    if (/^(https?:|mailto:|#)/.test(href)) continue

    if (href.startsWith('.')) {
      const target = join(file, '..', href.split('#')[0])
      if (!existsSync(target)) {
        errors.push(`${rel}: relative link "${href}" does not exist`)
      }
      checked++
      continue
    }

    if (!href.startsWith('/')) continue

    checked++
    const [route, anchor] = href.split('#')

    if (!knownRoute(route)) {
      errors.push(`${rel}: link to "${href}" but ${route} is not a route`)
      continue
    }

    if (!anchor) continue

    const available = anchorsFor(route)
    if (available === null) continue
    if (!available.has(anchor)) {
      const near = [...available].filter((a) => a.includes(anchor) || anchor.includes(a))
      errors.push(
        `${rel}: link to "${href}" but ${route} has no anchor "${anchor}"` +
          (near.length ? `. Did you mean ${near.slice(0, 3).map((a) => `#${a}`).join(', ')}?` : '')
      )
    }
  }
}

// --- the site's own links ----------------------------------------------------
//
// This checker only ever read content/, docs/spec/ and data/, so a broken href
// inside a React component was invisible to it. One shipped: the rebuilt bill
// of materials page linked /schematic, the route is /reference/schematic, and
// it 404'd in production while CI stayed green. Markdown is not the only place
// a link can be wrong, and the components are where the navigation actually is.

const WEB = join(ROOT, 'apps/web')
const PUBLIC = join(WEB, 'public')

function walkSource(dir, acc = []) {
  if (!existsSync(dir)) return acc
  for (const entry of readdirSync(dir)) {
    if (entry === 'node_modules' || entry.startsWith('.next')) continue
    const full = join(dir, entry)
    if (statSync(full).isDirectory()) walkSource(full, acc)
    else if (['.tsx', '.ts'].includes(extname(full))) acc.push(full)
  }
  return acc
}

for (const file of [
  ...walkSource(join(WEB, 'app')),
  ...walkSource(join(WEB, 'components')),
]) {
  const rel = relative(ROOT, file)
  readFileSync(file, 'utf8')
    .split('\n')
    .forEach((line, i) => {
      // Only literal attribute values. A template expression is a runtime
      // decision this cannot resolve, and guessing at one would produce noise
      // that teaches somebody to stop reading the output.
      for (const m of line.matchAll(/(?:href|src)=["'`](\/[^"'`${}\s]*)["'`]/g)) {
        const href = m[1]
        const [route, anchor] = href.split('#')
        checked++

        // A path ending in an extension is an asset in public/, not a route.
        if (/\.[a-z0-9]+$/i.test(route)) {
          if (!existsSync(join(PUBLIC, route))) {
            errors.push(`${rel}:${i + 1}: "${route}" is not a file in apps/web/public`)
          }
          continue
        }

        if (route === '/') continue
        if (!knownRoute(route)) {
          errors.push(`${rel}:${i + 1}: link to "${href}" but ${route} is not a route`)
          continue
        }
        if (!anchor) continue
        const available = anchorsFor(route)
        if (available === null) continue
        if (!available.has(anchor)) {
          errors.push(`${rel}:${i + 1}: link to "${href}" but ${route} has no anchor "${anchor}"`)
        }
      }
    })
}

if (errors.length) {
  for (const e of errors) console.error(`error ${e}`)
  console.error(`\n${errors.length} broken link${errors.length === 1 ? '' : 's'}.`)
  process.exit(1)
}
console.log(`${checked} internal links resolve, anchors included.`)

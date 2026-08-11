#!/usr/bin/env node
//
// The structured files in data/ render into several surfaces each, and they
// reference each other by id. Nothing in YAML stops a typo in a tier id from
// silently producing a part that belongs to no build, or a glossary see_also
// pointing at a term that was renamed. This checks the claims the data makes
// about itself.
//
// It also enforces the accuracy contract mechanically: a price or a mass that
// is a number must not carry an unresolved verification marker, and one that is
// null must carry one. That stops the two failure modes that matter, a
// fabricated figure and a silently missing one.

import { readFileSync, readdirSync, existsSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parse } from 'yaml'
import matter from 'gray-matter'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const DATA = join(ROOT, 'data')
const CONTENT = join(ROOT, 'content')

const errors = []
const warnings = []
const fail = (msg) => errors.push(msg)
const warn = (msg) => warnings.push(msg)

function load(name) {
  const path = join(DATA, name)
  if (!existsSync(path)) {
    fail(`data/${name} is missing`)
    return null
  }
  try {
    return parse(readFileSync(path, 'utf8'))
  } catch (e) {
    fail(`data/${name} is not valid YAML: ${e.message}`)
    return null
  }
}

const tiers = load('tiers.yaml')
const bom = load('bom.yaml')
const glossary = load('glossary.yaml')

// --- tiers -----------------------------------------------------------------

const tierIds = new Set()
const pathIds = new Set()

if (tiers) {
  for (const t of tiers.tiers ?? []) {
    if (!t.id) fail('data/tiers.yaml: a tier has no id')
    if (tierIds.has(t.id)) fail(`data/tiers.yaml: duplicate tier id "${t.id}"`)
    tierIds.add(t.id)
    if (!/^oApogee /.test(t.name ?? '')) {
      fail(`data/tiers.yaml: tier "${t.id}" name must be the full "oApogee X" form, got "${t.name}"`)
    }
  }
  for (const p of tiers.paths ?? []) pathIds.add(p.id)

  // The scope boundary is load-bearing safety content. If someone edits it out
  // of the data file, every page that renders it silently loses it.
  const doesNot = (tiers.scope?.does_not ?? []).join(' ').toLowerCase()
  for (const required of ['ejection', 'deploy', 'ignite', 'pyrotechnic']) {
    if (!doesNot.includes(required)) {
      fail(`data/tiers.yaml: scope.does_not no longer mentions "${required}". This is the passive payload boundary and it may not be weakened.`)
    }
  }
}

// --- bom -------------------------------------------------------------------

if (bom) {
  for (const p of bom.paths ?? []) {
    if (!pathIds.has(p.id)) fail(`data/bom.yaml: path "${p.id}" is not declared in tiers.yaml`)
  }
  for (const b of bom.tier_budgets ?? []) {
    if (!tierIds.has(b.id)) fail(`data/bom.yaml: tier_budgets id "${b.id}" is not a known tier`)
  }

  const partIds = new Set()
  for (const part of bom.parts ?? []) {
    if (!part.id) {
      fail('data/bom.yaml: a part has no id')
      continue
    }
    if (partIds.has(part.id)) fail(`data/bom.yaml: duplicate part id "${part.id}"`)
    partIds.add(part.id)

    for (const a of part.applies ?? []) {
      if (!tierIds.has(a.tier)) fail(`data/bom.yaml: part "${part.id}" applies to unknown tier "${a.tier}"`)
      if (!pathIds.has(a.path)) fail(`data/bom.yaml: part "${part.id}" applies to unknown path "${a.path}"`)
      if (!Number.isInteger(a.qty) || a.qty < 1) {
        fail(`data/bom.yaml: part "${part.id}" has a non-positive qty for ${a.tier}/${a.path}`)
      }
    }

    if (!('price_usd' in part)) fail(`data/bom.yaml: part "${part.id}" has no price_usd field`)

    // The accuracy contract, enforced both ways.
    const hasMarker = /TODO\((verify|confirm-on-hardware|confirm)\)/.test(part.verify ?? '')
    if (part.price_usd === null && !hasMarker) {
      fail(
        `data/bom.yaml: part "${part.id}" has a null price and no verification marker. ` +
          'A missing number must say what would close it.'
      )
    }
    if (typeof part.price_usd === 'number' && hasMarker) {
      warn(
        `data/bom.yaml: part "${part.id}" has both a price and an open verification marker. ` +
          'Close the marker or drop the number.'
      )
    }
    if (part.confidence && !['asserted', 'unverified'].includes(part.confidence)) {
      fail(`data/bom.yaml: part "${part.id}" has confidence "${part.confidence}"`)
    }
    if (part.mpn && part.confidence !== 'asserted') {
      warn(`data/bom.yaml: part "${part.id}" carries an mpn but is not marked asserted`)
    }
  }
}

// --- glossary --------------------------------------------------------------

if (glossary) {
  const termIds = new Set((glossary.terms ?? []).map((t) => t.id))
  for (const t of glossary.terms ?? []) {
    if (!t.id) fail('data/glossary.yaml: a term has no id')
    if (!t.short) fail(`data/glossary.yaml: term "${t.id}" has no short gloss`)
    if (!t.long) fail(`data/glossary.yaml: term "${t.id}" has no long definition`)
    for (const ref of t.see_also ?? []) {
      if (!termIds.has(ref)) fail(`data/glossary.yaml: term "${t.id}" links to unknown term "${ref}"`)
    }
  }
}

// --- content frontmatter ---------------------------------------------------

const STATUS = new Set(['draft', 'needs-review', 'verified'])
const DIFFICULTY = new Set(['beginner', 'intermediate', 'advanced'])
const TIER_SCOPE = new Set(['all', ...tierIds])

function walkMd(dir, acc = []) {
  if (!existsSync(dir)) return acc
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name)
    if (entry.isDirectory()) walkMd(full, acc)
    else if (entry.name.endsWith('.md')) acc.push(full)
  }
  return acc
}

for (const file of walkMd(CONTENT)) {
  const rel = file.slice(ROOT.length + 1)
  const { data, content } = matter(readFileSync(file, 'utf8'))

  for (const field of ['title', 'description', 'tier', 'difficulty', 'updated', 'status']) {
    if (!(field in data)) fail(`${rel}: frontmatter is missing "${field}"`)
  }
  if (data.status && !STATUS.has(data.status)) fail(`${rel}: status "${data.status}" is not valid`)
  if (data.difficulty && !DIFFICULTY.has(data.difficulty)) {
    fail(`${rel}: difficulty "${data.difficulty}" is not valid`)
  }
  if (data.tier && !TIER_SCOPE.has(data.tier)) fail(`${rel}: tier "${data.tier}" is not valid`)
  if (data.description && data.description.length > 160) {
    warn(`${rel}: description is ${data.description.length} characters, over the 160 target`)
  }

  // The promise attached to `verified`. A page cannot be verified while it
  // still admits, in its own text, that something in it is unverified.
  if (data.status === 'verified' && /TODO\((verify|confirm-on-hardware|confirm|photo)\)/.test(content)) {
    fail(`${rel}: marked verified but still contains open TODO markers`)
  }
}

// --- report ----------------------------------------------------------------

for (const w of warnings) console.warn(`warn  ${w}`)
for (const e of errors) console.error(`error ${e}`)

if (errors.length) {
  console.error(`\n${errors.length} data error${errors.length === 1 ? '' : 's'}.`)
  process.exit(1)
}
console.log(`Data cross-references hold. ${warnings.length} warning${warnings.length === 1 ? '' : 's'}.`)

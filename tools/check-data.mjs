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

    // The accuracy contract, enforced both ways, over every measured quantity a
    // part carries. It covered price_usd alone for a long time while this
    // file's own header claimed it covered mass, so a fabricated mass_g would
    // have passed CI silently. Mass is the number a stability check depends on,
    // which makes it the worse one to get wrong.
    const hasMarker = /TODO\((verify|confirm-on-hardware|confirm)\)/.test(part.verify ?? '')
    for (const field of ['price_usd', 'mass_g']) {
      if (part[field] === null && !hasMarker) {
        fail(
          `data/bom.yaml: part "${part.id}" has a null ${field} and no verification marker. ` +
            'A missing number must say what would close it.'
        )
      }
      if (typeof part[field] === 'number' && hasMarker) {
        warn(
          `data/bom.yaml: part "${part.id}" has a ${field} and an open verification marker. ` +
            'Close the marker or drop the number.'
        )
      }
    }
    // Three levels, because a breakout board is a different kind of claim from
    // a component. "asserted" means a manufacturer part number this repository
    // is willing to stand behind. "supplier" means a specific product from a
    // specific vendor: chosen, orderable, and named, but carrying a number the
    // vendor can revise or retire without telling anybody, which is why
    // CLAUDE.md says never to assert one. "unverified" means not chosen yet.
    if (part.confidence && !['asserted', 'supplier', 'unverified'].includes(part.confidence)) {
      fail(`data/bom.yaml: part "${part.id}" has confidence "${part.confidence}"`)
    }
    if (part.mpn && part.confidence === 'unverified') {
      warn(
        `data/bom.yaml: part "${part.id}" carries an mpn but is marked unverified. ` +
          'Use "asserted" for a manufacturer part number or "supplier" for a vendor product number.'
      )
    }
  }
}

// --- the printed parts against the circuit source ---------------------------
//
// The enclosure is built around the board outline, and the outline was written
// down twice: data/mechanical.yaml for the models, and the board element in
// hardware/oapogee.tsx for the PCB. They disagreed, 55 by 20 against 60 by 22,
// and nothing noticed, so both printed parts were sized for a board no other
// file described. Also enforces that every provisional dimension keeps the
// `closes` field mechanical.yaml promises it has, which was documented and
// unchecked.
{
  const mech = load('mechanical.yaml')
  const dim = (id) => (mech.params ?? []).find((p) => p.id === id)?.value

  const tsx = readFileSync(join(ROOT, 'hardware/oapogee.tsx'), 'utf8')
  const board = /<board\s+width="([\d.]+)mm"\s+height="([\d.]+)mm"/.exec(tsx)
  if (!board) {
    fail('hardware/oapogee.tsx: could not read the board outline, so nothing checks the enclosure')
  } else {
    const [, w, h] = board
    if (Number(w) !== dim('board_width')) {
      fail(
        `board_width is ${dim('board_width')} mm in data/mechanical.yaml and ${w} mm in ` +
          'hardware/oapogee.tsx. The enclosure is built for the board, so these are one number.'
      )
    }
    if (Number(h) !== dim('board_length')) {
      fail(
        `board_length is ${dim('board_length')} mm in data/mechanical.yaml and ${h} mm in ` +
          'hardware/oapogee.tsx. The enclosure is built for the board, so these are one number.'
      )
    }
  }

  for (const p of mech.params ?? []) {
    if (p.provenance === 'provisional' && !/TODO\((verify|confirm-on-hardware|confirm)\)/.test(p.closes ?? '')) {
      fail(
        `data/mechanical.yaml: "${p.id}" is provisional with no closing condition. ` +
          'A guess has to say what evidence retires it.'
      )
    }
    if (p.provenance !== 'provisional' && !p.source && p.provenance !== 'practice') {
      warn(`data/mechanical.yaml: "${p.id}" is ${p.provenance} but cites no source`)
    }
  }
}

// --- every data file at least parses ----------------------------------------
//
// Not every file here has a cross-reference worth checking, and the ones that
// do not were simply never loaded. data/troubleshooting.yaml was one of them:
// it renders a whole page and a duplicate key in it passed this check silently,
// which is how a YAML map quietly loses half its content. Parsing every file is
// cheap and catches the class.
{
  for (const name of readdirSync(DATA).filter((f) => /\.ya?ml$/.test(f))) {
    try {
      const doc = load(name)
      if (doc === null || typeof doc !== 'object') {
        fail(`data/${name}: parses to ${doc === null ? 'null' : typeof doc}, which is not a document`)
      }
    } catch (e) {
      fail(`data/${name}: ${String(e.message).split('\n')[0]}`)
    }
  }
}

// --- tier budgets -----------------------------------------------------------
//
// The brief's "under $60" and "under 25 g" are targets, and the whole point of
// this file is that a target never gets printed as though it were measured.
// Both currently sit null behind a marker, which is correct, and nothing
// checked that they stay that way.
{
  for (const b of bom.tier_budgets ?? []) {
    for (const [field, marker] of [
      ['target_bom_usd', 'target_bom_verify'],
      ['target_mass_g', 'target_mass_verify'],
    ]) {
      const hasMarker = /TODO\((verify|confirm-on-hardware|confirm)\)/.test(b[marker] ?? '')
      if (b[field] === null && !hasMarker) {
        fail(
          `data/bom.yaml: tier budget "${b.id}" has a null ${field} and no ${marker} marker. ` +
            'A missing number must say what would close it.'
        )
      }
      if (typeof b[field] === 'number' && hasMarker) {
        warn(
          `data/bom.yaml: tier budget "${b.id}" has a ${field} and an open ${marker}. ` +
            'A target published as a figure is the one thing this project does not do.'
        )
      }
    }
  }
}

// --- system diagram --------------------------------------------------------

// The block diagram is drawn from part ids. If a part is renamed or dropped and
// the diagram is not updated, it keeps rendering a board that no longer exists,
// which is exactly the failure a generated diagram is supposed to prevent.
const system = load('system.yaml')
if (system && bom) {
  const partIds = new Set((bom.parts ?? []).map((p) => p.id))
  const nodeIds = new Set()

  for (const n of system.nodes ?? []) {
    if (!n.id) fail('data/system.yaml: a node has no id')
    if (nodeIds.has(n.id)) fail(`data/system.yaml: duplicate node id "${n.id}"`)
    nodeIds.add(n.id)

    // `also` covers parts a node stands for without drawing separately: the
    // Modules-path equivalent of a chip, a connector, an antenna. They still
    // have to resolve.
    for (const ref of [n.part, ...(n.also ?? [])].filter(Boolean)) {
      if (!partIds.has(ref)) {
        fail(`data/system.yaml: node "${n.id}" references unknown part "${ref}"`)
      }
    }
    if (!n.part && !n.external) {
      fail(`data/system.yaml: node "${n.id}" has no part and is not marked external`)
    }
    for (const tier of n.tiers ?? []) {
      if (!tierIds.has(tier)) fail(`data/system.yaml: node "${n.id}" lists unknown tier "${tier}"`)
    }
    if (n.col >= (system.grid?.cols ?? 0) || n.row >= (system.grid?.rows ?? 0)) {
      fail(`data/system.yaml: node "${n.id}" sits outside the declared grid`)
    }
  }

  const KINDS = new Set((system.legend ?? []).map((l) => l.kind))
  for (const e of system.edges ?? []) {
    if (!nodeIds.has(e.from)) fail(`data/system.yaml: edge from unknown node "${e.from}"`)
    if (!nodeIds.has(e.to)) fail(`data/system.yaml: edge to unknown node "${e.to}"`)
    if (!KINDS.has(e.kind)) {
      fail(`data/system.yaml: edge ${e.from} -> ${e.to} has kind "${e.kind}", which the legend does not explain`)
    }
  }

  // A part in the bill of materials but on no node is worth knowing about, with
  // one exception. Passives are deliberately absent: a block diagram that showed
  // every pull-up and decoupling capacitor would stop being a block diagram, and
  // the schematic is where they belong. They are checked against the schematic
  // instead, below, which is the drawing that should have them.
  const drawn = new Set(
    (system.nodes ?? []).flatMap((n) => [n.part, ...(n.also ?? [])]).filter(Boolean)
  )
  for (const part of bom.parts ?? []) {
    if (part.role === 'passive') continue
    if (!drawn.has(part.id)) {
      warn(`data/system.yaml: part "${part.id}" appears in the BOM but not on the diagram`)
    }
  }
}

// --- the bill of materials against the schematic ----------------------------

/**
 * Every component on the schematic is a part somebody has to buy.
 *
 * These two drifted apart immediately and silently: the circuit gained an
 * arming switch, a battery sense divider, bus pull-ups and decoupling, and the
 * bill of materials listed none of them. A builder following the parts list
 * could not have populated the board, and nothing said so, because the two
 * files had no relationship a machine could check.
 *
 * They do now. Every reference designator in the generated netlist must be
 * claimed by exactly one part, and every designator a part claims must exist on
 * the schematic. Where a part asserts a manufacturer part number, the schematic
 * must agree with it.
 */
const NETLIST = join(ROOT, 'apps/web/public/hardware/oapogee-netlist.txt')

if (bom && existsSync(NETLIST)) {
  const text = readFileSync(NETLIST, 'utf8')
  const section = /^COMPONENTS:\n([\s\S]*?)\n\n/m.exec(text)

  if (!section) {
    fail('could not find the COMPONENTS section in the generated netlist')
  } else {
    const onSchematic = new Map()
    for (const line of section[1].split('\n')) {
      const m = /^\s*-\s*([A-Z]+\d+):\s*(.+?)\s*$/.exec(line)
      if (m) onSchematic.set(m[1], m[2])
    }

    const claimedBy = new Map()
    for (const part of bom.parts ?? []) {
      for (const des of part.designators ?? []) {
        if (claimedBy.has(des)) {
          fail(
            `designator ${des} is claimed by both "${claimedBy.get(des)}" and "${part.id}" in data/bom.yaml`
          )
        }
        claimedBy.set(des, part.id)

        if (!onSchematic.has(des)) {
          fail(
            `data/bom.yaml part "${part.id}" claims designator ${des}, which is not on the schematic. ` +
              `Either the part is obsolete or hardware/oapogee.tsx is missing it.`
          )
        }
      }
    }

    for (const [des, value] of onSchematic) {
      const partId = claimedBy.get(des)
      if (!partId) {
        fail(
          `${des} (${value}) is on the schematic and no part in data/bom.yaml claims it. ` +
            `Every component somebody has to solder is a component somebody has to buy.`
        )
        continue
      }

      // Where the bill of materials asserts a part number, the drawing has to
      // agree. A silent divergence here means the page and the board describe
      // different components.
      const part = bom.parts.find((x) => x.id === partId)
      if (part?.mpn && part.confidence === 'asserted' && !value.includes(part.mpn)) {
        fail(
          `${des} is "${value}" on the schematic but data/bom.yaml asserts ${part.mpn} for "${partId}"`
        )
      }
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

// docs/spec/ renders at /reference/telemetry-packet and /reference/log-format,
// which are public routes with the same frontmatter contract as content/. They
// were exempt, so the rule that a page cannot be `verified` while it still
// contains a TODO marker did not apply to the two pages a third party
// implements the wire format from.
for (const file of [...walkMd(CONTENT), ...walkMd(join(ROOT, 'docs/spec'))]) {
  const rel = file.slice(ROOT.length)
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

// --- artifacts the prose says exist ------------------------------------------
//
// The Reference page said the fabrication package with Gerbers was on the site.
// It was, until a check found that the radio had no reference oscillator, at
// which point `make fab` started refusing to write it and the files came down.
// The sentence stayed. So did one in the changelog, and another calling the
// custom board the path "whose fabrication files now exist".
//
// A page that promises a download is making a checkable claim, so it gets
// checked: every hardware artifact named anywhere in the prose has to be a file
// that is actually there. This is the same failure as the board dimensions, one
// layer out: the site described a state of the world that a tool had since
// changed underneath it.

{
  const PUBLIC_HW = join(ROOT, 'apps/web/public/hardware')
  const present = existsSync(PUBLIC_HW) ? new Set(readdirSync(PUBLIC_HW)) : new Set()

  const prose = []
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      if (entry.name.startsWith('.') || entry.name === 'node_modules') continue
      const full = join(dir, entry.name)
      if (entry.isDirectory()) walk(full)
      else if (/\.mdx?$/.test(entry.name)) prose.push(full)
    }
  }
  for (const d of ['content', 'docs']) if (existsSync(join(ROOT, d))) walk(join(ROOT, d))
  for (const f of ['CHANGELOG.md', 'README.md']) {
    if (existsSync(join(ROOT, f))) prose.push(join(ROOT, f))
  }

  for (const file of prose) {
    const rel = file.slice(ROOT.length)
    const text = readFileSync(file, 'utf8')
    for (const m of text.matchAll(/\boapogee[\w.-]*\.(zip|kicad_pcb|kicad_sch|svg|json|txt|stl)\b/g)) {
      if (!present.has(m[0])) {
        fail(
          `${rel} names the artifact ${m[0]}, which is not in apps/web/public/hardware. ` +
            `Either it is not built, or the prose is describing a file this project no longer publishes.`
        )
      }
    }
  }

  // --- and the claim made without naming a file -----------------------------
  //
  // The rule above catches a filename. It does not catch a sentence, and a
  // sentence is what actually shipped: six places said the fabrication files
  // were published and that somebody could order a board today, while
  // check-pcb.mjs said fab_ready false and the Gerbers were absent from
  // public/hardware. The site's own schematic page carried the claim twenty
  // lines above the generated list contradicting it.
  //
  // So the claim is tied to the check that decides it. While the board has an
  // open blocker, no prose and no component may say the package is published or
  // the board is orderable. Negations are what the site should be saying and
  // are left alone.
  const STATUS_FILE = join(ROOT, 'hardware/pcb-status.json')
  if (existsSync(STATUS_FILE)) {
    const pcb = JSON.parse(readFileSync(STATUS_FILE, 'utf8'))
    if (!pcb.fab_ready) {
      const claims = [
        [/(fabrication (files|package)|gerber)/i, /publish/i, 'says the fabrication package is published'],
        [/\b(board|pcb)\b/i, /\borderable\b/i, 'calls the board orderable'],
      ]
      // "not published", "held back", "refuses to write it": the true forms.
      const NEGATED = /\b(not|never|no|nor|cannot|without|unpublished|held back|refus\w*|absent|until|while|would be)\b/i

      const sources = [...prose]
      const walkSrc = (dir) => {
        if (!existsSync(dir)) return
        for (const entry of readdirSync(dir, { withFileTypes: true })) {
          if (entry.name.startsWith('.') || entry.name === 'node_modules') continue
          const full = join(dir, entry.name)
          if (entry.isDirectory()) walkSrc(full)
          else if (/\.tsx?$/.test(entry.name)) sources.push(full)
        }
      }
      walkSrc(join(ROOT, 'apps/web/app'))
      walkSrc(join(ROOT, 'apps/web/components'))
      for (const f of ['data/bom.yaml', 'data/tiers.yaml']) sources.push(join(ROOT, f))

      for (const file of sources) {
        const rel = file.slice(ROOT.length)
        // Sentence-ish. Long enough to hold a negation, short enough that one
        // sentence's "not" cannot excuse the next sentence's claim.
        for (const raw of readFileSync(file, 'utf8').split(/(?<=[.!?])\s+|\n\n+/)) {
          const sentence = raw.replace(/\s+/g, ' ').trim()
          if (!sentence || NEGATED.test(sentence)) continue
          for (const [subject, verb, what] of claims) {
            if (subject.test(sentence) && verb.test(sentence)) {
              fail(
                `${rel} ${what}, and hardware/pcb-status.json says fab_ready is false with ` +
                  `${pcb.blockers.length} open blocker(s). The sentence: "${sentence.slice(0, 120)}"`
              )
            }
          }
        }
      }
    }
  }
}

// --- every open question says where it stands --------------------------------
//
// docs/open-questions.md exists so that a decision taken quietly is visible as
// one, which only works if every question in it says whether it has been taken.
// One had drifted into saying neither: RP2350 or RP2040 read as a note to
// self, and a reader could not tell whether the board was designed around an
// answer or around an assumption. The distinction is the whole document.
{
  const path = join(ROOT, 'docs/open-questions.md')
  if (existsSync(path)) {
    const lines = readFileSync(path, 'utf8').split('\n')
    // The closing section is guidance for contributors rather than a question.
    const NOT_A_QUESTION = new Set(['How to weigh in'])
    let heading = null
    let verdict = false
    const finish = () => {
      if (heading && !verdict && !NOT_A_QUESTION.has(heading)) {
        fail(
          `docs/open-questions.md: "${heading}" says neither **Decided** nor **Open**. ` +
            'A question that states no verdict is the quiet decision this file exists to prevent.'
        )
      }
    }
    for (const line of lines) {
      const m = /^##\s+(.+?)\s*$/.exec(line)
      if (m) {
        finish()
        heading = m[1]
        verdict = false
      } else if (/\*\*(Decided|Open)\b/.test(line)) {
        verdict = true
      }
    }
    finish()
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

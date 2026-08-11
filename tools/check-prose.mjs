#!/usr/bin/env node
//
// Enforces the mechanical rules in CONTENT-STYLE.md that a human reviewer will
// not reliably catch on the fortieth page.
//
// Two of these are style. One of them, the name check, is not: shortening
// oApogee to "Apogee" collides with Apogee Components, who own that word in
// model rocketry completely, and a stray "the Apogee board" in a heading is
// both confusing to readers and an invitation to trademark friction. That is
// worth a build failure.

import { readFileSync, readdirSync, statSync } from 'node:fs'
import { join, relative, extname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('..', import.meta.url))
const SCAN_DIRS = ['content', 'data', 'docs']
const ALSO = ['CONTENT-STYLE.md', 'NOTES-FOR-WIL.md', 'README.md']
const SCAN_EXT = new Set(['.md', '.yaml', '.yml'])
const SKIP_DIRS = new Set(['node_modules', '.git', '.next'])

const RULES = [
  {
    id: 'em-dash',
    // U+2014 em dash and U+2013 en dash used as a sentence break.
    test: (line) => /[—]/.test(line) || /\s[–]\s/.test(line),
    message: 'em dash or spaced en dash. Use a comma, a colon, parentheses, or two sentences.',
  },
  {
    id: 'emoji',
    test: (line) => /\p{Extended_Pictographic}/u.test(line),
    message: 'emoji in content. Not permitted in body content.',
  },
  {
    id: 'name-misspelling',
    test: (line) => /\bOApogee\b|\bOpenApogee\b|\bOpen Apogee\b|\bo-Apogee\b|\bO-Apogee\b/.test(line),
    message: 'misspelled project name. It is always written oApogee.',
  },
  {
    id: 'name-shortened',
    // The bare word is legitimate when it means the flight event, and when it
    // names Apogee Components. It is not legitimate as a stand-in for the
    // project, which in practice means immediately before a project noun.
    test: (line) =>
      /\bApogee\s+(board|firmware|payload|kit|Solo|Link|Track|project|site|docs|team)\b/.test(line),
    message:
      'the project name shortened to "Apogee". That word belongs to Apogee Components in this ' +
      'hobby. There is no short form: write oApogee.',
  },
]

// Fenced code blocks and inline code are exempt: a code sample may legitimately
// contain any of these, and a URL containing a dash is not prose.
function* proseLines(text) {
  let inFence = false
  const lines = text.split('\n')
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    if (/^\s*```/.test(line)) {
      inFence = !inFence
      continue
    }
    if (inFence) continue
    yield [i + 1, line.replace(/`[^`]*`/g, '').replace(/https?:\/\/\S+/g, '')]
  }
}

function walk(dir, acc = []) {
  let entries
  try {
    entries = readdirSync(dir)
  } catch {
    return acc
  }
  for (const entry of entries) {
    if (SKIP_DIRS.has(entry)) continue
    const full = join(dir, entry)
    if (statSync(full).isDirectory()) walk(full, acc)
    else if (SCAN_EXT.has(extname(full))) acc.push(full)
  }
  return acc
}

const files = [...SCAN_DIRS.flatMap((d) => walk(join(ROOT, d))), ...ALSO.map((f) => join(ROOT, f))]

let failures = 0
for (const file of files) {
  let text
  try {
    text = readFileSync(file, 'utf8')
  } catch {
    continue
  }
  // This file describes the forbidden constructs, so it cannot obey them.
  if (file.endsWith('CONTENT-STYLE.md')) continue
  for (const [lineNo, line] of proseLines(text)) {
    for (const rule of RULES) {
      if (rule.test(line)) {
        console.error(`${relative(ROOT, file)}:${lineNo}  [${rule.id}] ${rule.message}`)
        failures++
      }
    }
  }
}

if (failures) {
  console.error(`\n${failures} prose issue${failures === 1 ? '' : 's'}.`)
  process.exit(1)
}
console.log(`Prose rules hold across ${files.length} files.`)

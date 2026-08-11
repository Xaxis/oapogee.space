# oApogee content style

Every page on this site is a set of instructions that someone will follow while
holding a soldering iron, or a set of numbers they will use to decide whether a
rocket is safe to fly. The documentation is the product. This file exists so
that a contributor writing page fourteen sounds like the person who wrote page
one, and so that the accuracy bar does not quietly drop over time.

Read the whole thing before writing. It is short.

## The name

The project is **oApogee**. Lowercase `o`, capital `A`, one word, no space, no
hyphen. Never "OApogee", "Open Apogee", "OpenApogee", or "o-Apogee". The logo
wordmark may be set in all lowercase as `oapogee`, but running prose always uses
oApogee.

**Never shorten it to "Apogee."** Apogee Components is the dominant retailer in
model rocketry and owns that word in this hobby completely. "The Apogee board"
or "Apogee recommends" would confuse readers and invite trademark friction.
There is no short form. It is oApogee in headings, in body copy, in image alt
text, in code comments, in repo names, in commit messages, and on the board
silkscreen.

The bare word "apogee" is fine when it means the physical event, the highest
point of a flight, which is what it means most of the time on this site. The
test is whether it names the project or the moment. "Apogee detection requires
three consecutive descending samples" is correct. "The Apogee firmware" is not.

The tiers are **oApogee Solo**, **oApogee Link**, and **oApogee Track**. Inside a
single section, after one full mention, "Solo", "Link", and "Track" alone are
fine, and are preferred in tables where the column already establishes context.

Pronunciation, "oh-APP-uh-jee", appears once, on the About page. The `o` stands
for open, stated once, on the About page, in one sentence. Neither belongs
anywhere else. Do not build a mythology around the name.

## The accuracy rule

This outranks every other rule in this file.

**Never invent a number.** Not a price, not a manufacturer part number, not a
mass, not a radio range, not a battery life, not a measured altitude, not a
current draw, not a print time. If there is no verified source and no
measurement from the maintainer, write a marker and move on:

```
TODO(verify): typical mass of the Solo build with a 150 mAh cell, measured on a
0.1 g scale, three assembled units.
```

Three markers, used precisely:

- `TODO(verify)` for a number, a price, a link, or a claim that needs a source
  or a measurement. Always say what evidence would close it.
- `TODO(confirm-on-hardware)` for a procedural step that cannot be checked until
  a physical board exists. The step may be written out in full, it is simply not
  trusted yet.
- `TODO(photo)` for an image slot, with a description of what the photo must
  show, not just "photo here".

Every marker gets aggregated into `TODO-VERIFY.md`. A marker with no closing
condition is not a marker, it is a shrug.

A page full of confident fabricated specs is worse than an empty page. Readers
in this hobby spend money and fly hardware over other people's heads based on
what a site like this tells them. An honest gap costs a reader five minutes. A
plausible wrong number costs them a rocket, or worse.

Rounded, clearly-labelled physics is not invention. "A 25 g payload on a C6-5
will cost you altitude" is a statement about how rocketry works. "A 25 g payload
on a C6-5 reduces apogee by 31 m" is a measurement, and needs OpenRocket output
or a real flight behind it.

## Voice

Second person, imperative in procedures.

> Solder the barometer first.

Not:

> The barometer should be soldered first.

Direct and unhyped. No "revolutionary", no "game-changing", no "unleash", no
"seamless", no "simply", no "just". "Just solder the header" tells a beginner
whose hands are shaking that their difficulty is a personal failing.

Confident about what works, honest about what does not. If the radio range
collapses in trees, say so with a number and say how the number was obtained.
The credibility of this site is the only asset it has.

Do not sell. The reader already decided to be here.

## Mechanics

- **Never use em dashes.** Use a comma, a colon, parentheses, or two sentences.
  This is checked in CI by `make prose`.
- No emoji in body content, ever. Status icons in the interface are a design
  decision, not a content one.
- No rhetorical questions as section openers.
- Short paragraphs. Three or four sentences is usually enough.
- Procedures are numbered lists. Reference material is tables. Neither is prose.
- Avoid bullet lists inside body prose. If a paragraph wants to become a list, it
  is usually reference material in the wrong section.
- Sentence case headings, always. "Static ports and why they matter", not
  "Static Ports And Why They Matter".
- Units get a space before them, "25 g", "902 MHz", "24 mm", except degrees and
  percent, "45deg", "20%". SI units, with imperial in parentheses where a US
  hobbyist buys the part in imperial, for example "24 mm (BT-50)".
- Motor designations are uppercase with the delay attached, "C6-5", "B6-4".
- Write "3D printed" and "3D printable", not "3d-printed".

## Layering, not dumbing down

Three readers, in priority order: the determined beginner, the educator, the
engineer. Never simplify a technical fact to serve the first reader. Layer it
instead.

The procedure comes first, in plain imperative steps a beginner can follow
without understanding why. The explanation follows, in a collapsible block or a
linked deep dive, for the reader who wants it. The engineer gets a reference
section with no hand-holding at all, reachable in one click from the top of the
page.

A beginner who is told what to do and offered the reason is respected. A
beginner who is given a simplified reason that is subtly wrong has been
patronised, and will discover it later.

## Checkpoints

Every build step that changes the state of the hardware ends with an observable
result. Not "the sensor should now work". Something the reader can see:

> You should see the status LED pulse green twice. The serial console should
> print a pressure between 95000 and 103000 Pa. If it prints 0, the sensor is
> not on the bus, go to Troubleshooting: the barometer never appears.

Write the failure branch next to the success branch. A beginner who sees the
wrong thing at step 7 needs to know at step 7, not after they have soldered
everything else on top of the mistake.

## Jargon

Define a term on first use on each page, briefly, in the sentence where it
appears. Then add it to `data/glossary.yaml` so it can be linked or hovered
consistently across the site.

The glossary entry is the long definition. The inline gloss is one clause.

> The payload sits above the static ports, the vent holes that let the
> barometer see outside air pressure.

## Safety language

Safety content is not a disclaimer and is not written in disclaimer voice. It is
procedural, specific, and states the actual failure mode rather than gesturing
at liability.

> A punctured LiPo cell can ignite minutes after the damage. Do not fly a cell
> that has been crushed, and do not put it back in a drawer.

Not:

> Users assume all risk associated with lithium polymer batteries.

Two hard boundaries appear anywhere they could plausibly be misread:

1. **oApogee is a passive instrumentation payload.** It does not fire ejection
   charges, control deployment, ignite motors, or command any pyrotechnic
   device. Any page that could imply otherwise is a bug.
2. **Part 15 and Part 97 are different legal regimes.** Never blur them. The
   unlicensed default and the amateur radio path are separately labelled and
   never mixed in the same procedure.

Link to primary sources for rules. NAR, Tripoli, the FAA, the FCC. Do not
paraphrase a safety code loosely and do not quote a version without a date.

## Frontmatter

Every prose page in `content/` carries this frontmatter:

```yaml
---
title: Safety and rules
description: One sentence, under 160 characters, used for the page meta description.
tier: all            # all | solo | link | track
difficulty: beginner # beginner | intermediate | advanced
time_estimate: null  # ISO-8601-ish human string, or null when not a procedure
updated: 2026-08-11  # ISO date, the day the content last actually changed
status: draft        # draft | needs-review | verified
---
```

`status` is a promise to the reader, so it is a promise about process:

- `draft`, written, not checked by anyone.
- `needs-review`, the author believes it is right and wants a second pair of
  eyes on the technical content.
- `verified`, a human with the relevant expertise has checked every number and
  every step on real hardware. Nothing reaches `verified` while it still
  contains a `TODO(verify)` or a `TODO(confirm-on-hardware)`.

Only the maintainer moves a page to `verified`.

## Content lives outside prose when it renders more than once

The bill of materials, the glossary, the preflight checklist, the tier
comparison, and the troubleshooting index are structured data in `data/`, in
YAML. They render into the site, into printable sheets, and into the repository
README. A table hand-written inside a Markdown page will drift from the one next
to it, and there is no way to check that it has not.

Prose pages reference the data. They do not restate it.

## Markdown stays portable

Plain CommonMark plus GFM tables. No framework components in Markdown, no MDX
imports, no HTML beyond `<details>` and `<summary>` for the layering pattern.
The content in this repository should survive being read on GitHub, piped
through pandoc, or printed. That constraint is what keeps it a documentation
project rather than a website with words in it.

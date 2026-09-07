---
title: About and license
description: What oApogee is for, where the name comes from, and the licensing decision with its tradeoffs laid out rather than assumed.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-09-07
status: draft
---

# About and license

## Why this exists

The options for measuring a model rocket flight are a barbell.

At one end, beeper altimeters that cost a few tens of dollars and report a single
number: peak altitude, beeped out in tones you count in a field. At the other,
professional flight computers costing several hundred dollars, aimed at
certified high power rocketry and carrying capabilities, and consequences, that
a person flying a C motor out of a soccer field neither needs nor wants.

In between there are a dozen abandoned repositories with a schematic, no build
documentation, and a last commit from four years ago.

Nobody has produced a well-documented, cheap, genuinely reproducible telemetry
payload for the person who wants an actual flight profile rather than one
number.

**The documentation is the product.** The hardware is commodity: a barometer, an
IMU, a microcontroller, a radio, all of it available to anyone who reads a few
datasheets. What would make this project worth existing is that a motivated
beginner can follow this site from start to finish and end up with a working
payload and a real flight graph.

That is the thesis, and why the writing is treated as the work rather than a
record of it.

## The name

**oApogee.** Lowercase `o`, capital `A`, one word.

Apogee is the highest point of the flight: the instant the entire launch is
about, and the single number every rocketeer wants. The `o` makes it open.

Pronounced "oh-APP-uh-jee."

### It is never shortened to "Apogee"

Apogee Components is the dominant retailer in model rocketry and owns that word
in this hobby completely. Writing `the Apogee board` would confuse readers and
invite trademark friction, so there is no short form. It is oApogee everywhere,
including in headings, image alt text, code comments, and on the board
silkscreen.

**oApogee is not affiliated with Apogee Components.**

## What openness has to mean here

A project named for openness that ships a black box loses the audience it was
built for. So the bar is source files, not exports:

- The **KiCad project**, not a PDF of the schematic.
- The **parametric CAD source** for the sled and the pod, not only exported STLs,
  in a format that opens without a commercial licence.
- The **complete firmware**, including the calibration constants and the filter
  coefficients.
- The **wire formats**, specified formally enough that a third party can write
  their own decoder without asking permission or reverse engineering anything.
  See the [telemetry packet spec](/reference/telemetry-packet) and the [log
  format spec](/reference/log-format).

**If any part of this design cannot be opened, this page will say so explicitly
and say why.** Quietly omitting something is worse than declaring it.

As of today there is nothing to declare, because there is nothing built. That is
not the same as a clean bill of health and this section will be revisited when
there is hardware.

## Licensing

Decided. Three licences, because this repository holds three different kinds of
thing and treating them the same would be wrong in both directions.

| What | Licence |
|---|---|
| Hardware design files | CERN-OHL-S 2.0, strongly reciprocal |
| Firmware and software | Apache 2.0 |
| Documentation and content | CC BY-SA 4.0 |

Full texts are in `LICENSES/`; `LICENSE` at the repository root says which
applies to which files. The reasoning is kept below because each choice cost
something worth knowing about.

The asymmetry is deliberate: reciprocal hardware, permissive firmware. The
board is the thing worth protecting from a closed fork, and the firmware is the
thing worth having other people embed everywhere.

Each choice cost something, and the cost is the part worth knowing:

- **CERN-OHL-S** requires anyone who makes and distributes a product based on
  the design to publish their modified sources. The permissive `-P` would
  maximise adoption instead, and some vendors avoid reciprocal licences
  entirely. That is the cost, and it is accepted.
- **Apache 2.0** adds an express patent grant, which matters once money is
  involved and MIT does not offer. What it gives up is MIT's familiarity to
  hobbyists.
- **CC BY-SA 4.0** lets anyone reproduce this build guide commercially, with
  attribution, as long as they share alike. Given that the documentation is the
  product, that is a real thing to accept. `CC BY-NC-SA` would prevent it and is
  not an open licence by the usual definition, which sits badly under a name
  whose first letter means open.

### Trademark, which is the part licences do not cover

A licence does not stop somebody selling a clone called oApogee. A trademark
does, and it is the only thing that does.

**The design is open. The name is not.** Nothing in the three licences above
grants any right to use the oApogee name or wordmark.

Build it, modify it, sell it, fork it. Call it something else.

You may say your product is "based on the oApogee design" or "compatible with
oApogee", because that is accurate and useful to whoever is buying. You may not
name it oApogee, brand it as oApogee, or imply that it is an official oApogee
product.

This is the standard open hardware posture, and it is the only one that survives
contact with a manufacturer.

Not registered, and not being registered yet. Registration costs money and buys
enforceability against a problem that does not exist: nothing has been
fabricated and nobody is selling anything called oApogee. Revisit if somebody
ships a product under the name, or if this project starts selling hardware
itself. Both are events rather than dates.

## Status

**Nothing physical exists yet.** No board has been fabricated, assembled,
weighed, priced, or flown. The PCB is laid out and routed, which is a real step
past a paper design and not the same thing as a board that works. Its
fabrication package is not published, because the check that gates it refuses
while the board has an open blocker.

Every unverified figure on this site is marked in place, in the page that would
otherwise state it, saying what evidence would close it. That is deliberately
not summarised anywhere: a count is easy to let drift, and a marker beside the
number it replaces is not.

## The rule this project runs on

**Never publish a number that has not been measured or sourced.**

No price, mass, range, current draw, battery life, or measured altitude appears
on this site unless it came from an invoice, a scale, a datasheet, or a flight.

People in this hobby spend money and fly hardware over other people's heads
based on what a site like this tells them. A page full of confident fabricated
specifications is worse than an empty one. Two consequences you will have
noticed: the bill of materials has no prices in it, and the homepage does not
have a flight graph.

## Contributing

The most useful contribution right now is telling us where this is wrong.

The safety and regulatory pages in particular are written carefully and have not
been reviewed by anyone with formal expertise in the rules they describe. If you
hold an amateur radio licence, or you are active in a NAR or Tripoli club, those
two pages are where being wrong has consequences beyond embarrassment.

Source is at [github.com/Xaxis/oapogee.space](https://github.com/Xaxis/oapogee.space).
Read `CONTENT-STYLE.md` before writing anything; it covers the accuracy rule,
the voice, and the naming conventions, all of which are enforced in the build.

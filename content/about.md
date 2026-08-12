---
title: About and license
description: What oApogee is for, where the name comes from, and the licensing decision with its tradeoffs laid out rather than assumed.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-08-11
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

That is the whole thesis, and it is why the writing is treated as the work
rather than as documentation of the work.

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

Not yet decided. The tradeoffs are below, presented rather than resolved,
because they are the maintainer's call and because the reasoning is more useful
to a reader than the conclusion.

### Hardware: CERN-OHL-S or CERN-OHL-P

Both are open hardware licences from CERN. The difference is reciprocity.

**CERN-OHL-S** is reciprocal: anyone who makes and distributes a product based
on the design must publish their modified sources. **CERN-OHL-P** is permissive:
they need not.

The case for `-S`: the plausible bad outcome for a project like this is a
manufacturer taking the design, producing it at volume, and shipping it as a
closed product. `-S` does not prevent that and is not meant to, but it does
require that their improvements come back. Given that the name promises
openness, taking the licence that keeps downstream open is the consistent
choice.

The case for `-P`: it maximises adoption. Some vendors avoid reciprocal
licences, and if the goal is for this design to end up in as many hands as
possible, permissive gets there faster.

**Current recommendation: CERN-OHL-S 2.0.**

### Firmware: MIT or Apache 2.0

Both permissive, both fine, and the practical difference is narrow.

**Apache 2.0** adds an express patent grant and a patent retaliation clause,
which matter more once money is involved. **MIT** is shorter and more familiar
to hobbyists.

**Current recommendation: Apache 2.0**, because the firmware is the part most
likely to be embedded in something else, and the patent grant is worth the extra
paragraphs when that happens.

Note the asymmetry: reciprocal hardware, permissive firmware. That is deliberate
and it is a common pattern. The board is the thing worth protecting from a
closed fork; the firmware is the thing worth having other people embed
everywhere.

### Documentation: CC BY-SA 4.0

Straightforward, and it matches the reciprocal posture on the hardware.

The consequence, worth stating before agreeing to it: CC BY-SA means anyone can
reproduce this build guide commercially, as long as they attribute and share
alike. Given that the documentation is the product, that is a real thing to
accept rather than a formality.

`CC BY-NC-SA` would prevent that, and it is not an open licence by the usual
definition. It would sit badly under a name whose first letter means open.

**Current recommendation: CC BY-SA 4.0, and accept the consequence.**

### Trademark, which is the part licences do not cover

A licence does not stop somebody selling a clone called oApogee. A trademark
does.

The standard open hardware posture, and the one this project intends: **the
design is open, the name is not.** Build it, sell it, modify it, fork it. Call it
something else.

TODO(confirm): decide whether to register the mark, and publish a short
trademark policy here once decided.

## Status

**oApogee is a design on paper.** Nothing has been fabricated, assembled,
weighed, priced, or flown.

The [status page](/status) counts exactly how much of this site is unverified,
generated from the content files rather than from a hand-maintained list, so it
cannot quietly understate itself.

## The rule this project runs on

**Never publish a number that has not been measured or sourced.**

No price, mass, range, current draw, battery life, or measured altitude appears
on this site unless it came from an invoice, a scale, a datasheet, or a flight. A
missing figure is written as a marker recording what evidence would close it,
never estimated into place.

People in this hobby spend money and fly hardware over other people's heads
based on what a site like this tells them. A page full of confident fabricated
specifications is worse than an empty one.

Two consequences you will have noticed: the bill of materials has no prices in
it, and the homepage does not have a flight graph.

## Contributing

The most useful contribution right now is telling us where this is wrong.

The safety and regulatory pages in particular are written carefully and have not
been reviewed by anyone with formal expertise in the rules they describe. If you
hold an amateur radio licence, or you are active in a NAR or Tripoli club, those
two pages are where being wrong has consequences beyond embarrassment.

Source is at [github.com/Xaxis/oapogee.space](https://github.com/Xaxis/oapogee.space).
Read `CONTENT-STYLE.md` before writing anything; it covers the accuracy rule,
the voice, and the naming conventions, all of which are enforced in the build.

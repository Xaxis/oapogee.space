---
title: FAQ
description: Questions people actually ask about oApogee, including the ones with unsatisfying answers.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-08-11
status: draft
---

# FAQ

Real questions only. Anything asked twice in the wild gets added. Nothing here
is invented as a way of writing marketing copy in question form.

Because the project is new, this page is short, and most of what is here is
questions asked during the design rather than by builders. It will get longer
and more useful as people build things.

## Can I buy one?

Not yet. Kits are intended eventually, and nothing has been fabricated, so there
is nothing to sell. You can order the bare PCB yourself from the published
fabrication files, with the caveat on the schematic page: the pin numbering of
most packages has not been checked against a datasheet.

## Can it deploy my parachute?

No, and it never will.

oApogee is a passive instrumentation payload. It does not fire ejection charges,
control deployment, ignite motors, or command any pyrotechnic device. Your
recovery system is the motor's ejection charge and delay grain, exactly as it
was before you fitted a payload.

This is a design boundary rather than a missing feature. Deployment control
carries a completely different set of failure consequences and belongs to
certified high power rocketry.

## Why is there no price on the bill of materials?

Because nothing has been bought, so there is no honest number to put there.

This site does not publish figures it has not measured or sourced. The targets in
the project brief are under $60 for the full build and under $30 for the entry
build, but a target is not a measurement, and printing one as though it were is
how a site becomes untrustworthy.

## Do I need to know how to program?

No. The microcontroller is flashed by dragging a file onto what appears as a USB
drive. There is no toolchain to install.

You do need to have soldered before, and to be able to use a multimeter for
continuity and voltage.

## Do I need a radio licence?

Not for the default configuration. oApogee Link and Track use the 902 to 928 MHz
band in the US under FCC Part 15, which requires no licence and no callsign.

oApogee Solo has no transmitter at all, which is one of the reasons it exists.

If you hold an amateur licence there is a separate path with more transmit power
available, covered on the [safety page](/safety). Those two regimes are
different and the site never mixes them.

## Can I fly it outside the US?

The band changes and so do the rules. A 902 to 928 MHz module is not legal to
operate in most of Europe, where the equivalent short range band is 863 to
870 MHz, and that is a different radio module rather than a firmware setting.

The EU and UK rules are not yet documented properly on this site, including
whether airborne use is permitted, which several national administrations
restrict. That is marked as unfinished rather than guessed at. See the
[safety page](/safety).

## Will it fit my rocket?

The external pod straps to the outside of any body tube and requires no
modification to the airframe, which is what makes it work on a rocket you
already own. That is the answer for a small rocket.

The internal sled is cut for a BT-55 payload bay by default, and it needs a
rocket that has a payload bay. It is not offered for a BT-50, because a 22 mm
board in that bore leaves about 0.6 mm of rail either side, which prints and
then breaks. Both parts are parametric: measure your own tube and re-render.

The pod costs you drag and moves your centre of gravity, so
[re-simulate](/mounting) before flying. That is not a formality.

## What is the highest it can go?

TODO(verify): two separate ceilings, and readers will conflate them. The
barometer has a usable pressure range that corresponds to some altitude, and the
radio has a practical range that is usually the lower limit in practice. Both
need stating with their conditions.

The design target is low power flying, A through E motors, with headroom so that
F and G are not blocked later.

## Why not a microSD card?

Because card sockets unseat under boost.

A microSD card is held in by a friction detent, and the acceleration on even a
modest motor is enough to move it. In DIY flight computers this is the single
most common in-flight failure, and its failure mode is the worst available: the
flight proceeds normally and the data is simply gone.

The flash on oApogee is soldered down and cannot unseat. The cost is that you
offload over USB instead of moving a card, which is a mild inconvenience on the
ground in exchange for removing a failure mode in the air.

## Why does my altitude read slightly negative after landing?

That is usually correct rather than a fault.

Altitude is measured against a pressure reference taken on the pad. If
atmospheric pressure rose during the flight, or the rocket landed lower than the
pad, the reading goes negative. oApogee does not clamp it at zero, because
clamping would hide a real measurement.

See [my altitude reads negative](/troubleshooting#altitude-negative) for the
cases where it does indicate a problem.

## What does the `o` stand for?

Open. That is the whole story, and it is told once, on the [about page](/about).

## How do you pronounce it?

"oh-APP-uh-jee."

## Is this related to Apogee Components?

No. oApogee is not affiliated with Apogee Components, who are an unrelated and
much older retailer in this hobby. The name is never shortened to "Apogee" for
exactly that reason.

## Can I help?

Yes, and the most useful help right now is telling us where the site is wrong.

The safety and regulatory pages especially. They are written carefully and have
not been reviewed by anyone with formal expertise in the rules they describe.

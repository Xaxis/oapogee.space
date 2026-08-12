---
title: Ground station
description: The receiver for oApogee Link and Track. Assembly, pairing, antennas, and honest expectations about range.
tier: link
difficulty: intermediate
time_estimate: null
updated: 2026-08-11
status: draft
---

# Ground station

oApogee Link and oApogee Track are not usable without a receiver. This is a
second build with its own parts list, and any price you have seen quoted for
those tiers is incomplete until you add this to it.

## What it is

A matching LoRa receiver on the same microcontroller family as the payload,
connected to a laptop or phone by USB. The display runs in a browser over
WebSerial, so there is nothing to install on the flight line and anybody who
wants to watch can open a page.

### The browser requirement, stated plainly

**WebSerial is not supported in Safari, and is not available in any browser on
iOS.** A Chromium-based browser on a desktop or Android device works. An iPhone
does not, and no amount of trying different browsers on it will help, because
they all use the same underlying engine.

This is a real limitation and it excludes a large share of the phones at a
typical launch. The alternative designs are worse: a native app per platform
that nobody installs, or a local server the user has to run before they can see
an altitude. A plain serial terminal fallback is documented below for anyone the
browser path locks out.

TODO(confirm): the brief asks for confirmation of this approach before building
it out. The recommendation is WebSerial with the requirement stated prominently,
plus the terminal fallback.

## Building it

TODO(confirm-on-hardware): the ground station has not been built. The parts are
listed in the [bill of materials](/bom); the assembly steps below are the
structure, not a tested procedure.

1. Fit the microcontroller module to the carrier.
2. Fit the radio module.
3. Attach the antenna. **Never power a transmitter with no antenna attached**,
   even though this end mostly listens.
4. Print and fit the enclosure.
5. Flash the ground station firmware, which is a separate `.uf2` from the
   payload firmware.

> **Checkpoint.** Connecting the ground station by USB and opening the receiver
> page shows a connected device and a packet counter. With a payload powered
> nearby, the counter increments.
>
> Nothing at all: [the ground station receives nothing](/troubleshooting#no-packets).
> Packets arriving but failing: [packets arrive but most fail the CRC](/troubleshooting#crc-failures).

## Pairing

There is no pairing, in the Bluetooth sense. Both ends have to agree on the
radio configuration, and if they do, the link works.

The settings that must match on both ends: frequency, bandwidth, spreading
factor, and coding rate. One mismatch and the link is silent rather than
degraded, which is a useful property when debugging: a partially working link
means something other than a settings mismatch.

Multiple payloads on one field share the band. Each packet carries the payload's
device identifier, so a ground station can display several flights at once, and
whether it does is a display decision rather than a protocol one.

## Antennas

The antenna is the cheapest part of the whole system and the one most able to
ruin it.

**On the rocket**, mass and space dominate, so the antenna is small and
compromised, and there is not much choice about that.

**On the ground**, mass does not matter at all. This is where link budget is
cheap to buy, and where a better antenna is worth carrying.

Two things that cost nothing and help:

- **Match the band.** A 902 to 928 MHz antenna on an 868 MHz module, or the
  reverse, appears to work at a few metres and fails at any real distance. This
  is the most common range problem and the easiest to overlook.
- **Match the polarisation.** Two antennas at right angles to each other lose a
  large fraction of the link budget for nothing. If the rocket's antenna is
  vertical on the pad, hold yours vertically.

TODO(verify): compare a quarter wave whip against a small directional antenna,
measured, and publish both ranges so a reader can decide whether carrying the
directional one is worth it.

## Range

TODO(verify): the entire range claim. This is the number readers most want and
the number most often exaggerated in this hobby, so it gets measured or it does
not get published.

What the measurement must state alongside any figure: antenna type at both ends,
orientation, spreading factor, bandwidth, transmit power, and terrain. A range
figure without those is not a measurement, it is an anecdote.

Three cases must be published, including the ones that are disappointing:

1. **Rocket in the air, line of sight.** The best case, and the one everybody
   quotes.
2. **Rocket on the ground after landing.** Far worse, because the antenna is
   lying in grass instead of in clear air. This is the case that actually
   matters for recovery, and it is the one nobody publishes.
3. **Through trees.** Worse again, and worth a number rather than a shrug.

Two things that are safe to say now without measuring, because they are
properties of the system rather than of a particular flight:

- A conductive airframe, carbon fibre or metal, blocks the link. If your rocket
  is carbon, the antenna has to be outside it.
- A lower spreading factor buys update rate and costs range. That trade is
  yours to make, and the site will publish both ends of it rather than only the
  flattering one.

## The terminal fallback

For anyone the browser path excludes, the ground station also emits
human-readable lines over the same serial connection, readable in any serial
terminal.

TODO(confirm-on-hardware): define and document that text format. It should be
one line per packet, fixed field order, easy to grep and easy to paste into a
message when asking for help.

## Recording

The ground station records every packet it receives, including the ones that
fail the CRC, with a receive timestamp.

Failed packets are kept deliberately. Discarding them silently would make the
link look better than it is, and the count of failures against successes is the
honest measure of link quality. A conforming receiver counts both and exposes
both.

The downlink recording is a second, independent copy of the flight. If the
payload is never recovered, it is the only copy, which is most of the argument
for Link over Solo.

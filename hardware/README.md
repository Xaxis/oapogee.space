# oApogee hardware

The circuit, as source. `oapogee.tsx` is a [tscircuit](https://tscircuit.com)
design: the schematic is written as code, and the drawing, the netlist, the
machine-readable circuit JSON and a KiCad schematic are all rendered from it.

That is what makes the openness promise checkable rather than rhetorical. A
project whose first letter means open should publish the KiCad project, not a
PDF of a schematic, and this directory is where that comes from.

## What this is and is not

**This is a netlist and a layout. It is not a board anyone has held.**

The connectivity is the design decision worth reviewing, and it is now backed by
real pin numbers: every package's numbering is transcribed from its
manufacturer's datasheet, with the table or figure cited beside the `pinLabels`
it produced. Two of those transcriptions replaced guesses that would have
produced a dead board, which is why they are cited rather than trusted.

**What no check in this repository can confirm** is that a footprint's pad
numbering matches the numbering its datasheet uses. A `pinLabel` maps a signal
onto a pad BY NUMBER, and nothing here knows whether pad 1 of a given footprint
is the pad the part calls pin 1. Confirm each package by hand before spending
money.

**No fabrication package is published.** `make fab` refuses to write Gerbers
while `tools/check-pcb.mjs` reports an open blocker, and it does. Those blockers
render on the site's schematic page, generated on every build.

## Building it

```bash
make hw-deps   # once, installs tscircuit here
make hw        # render the artifacts into apps/web/public/hardware/
make check-hw  # the committed artifacts match the source, and the board passes
make fab       # Gerbers and KiCad PCB. Refuses while any blocker is open.
```

Outputs, in order of how much they can be trusted:

| Artifact | What it is |
|---|---|
| `oapogee-netlist.txt` | The connectivity, as text. Diffable, reviewable, and the thing to read first. |
| `oapogee-schematic.svg` | The same connectivity, drawn. |
| `oapogee.kicad_sch` | A KiCad schematic, for anyone who wants to take the design further. |
| `oapogee-pcb.svg` | The routed board: placement, footprints and copper. |
| `oapogee-assembly.svg` | Outlines and designators, for placing parts by hand. |
| `oapogee-circuit.json` | Machine readable, for anyone writing their own tooling. |

`make check-hw` compares the netlist and the schematic against a fresh render,
so the committed drawing cannot quietly stop matching the source, then runs
`tools/check-pcb.mjs` over the board. Two artifacts are excluded from the byte
comparison and compared by content instead: the KiCad schematic mints fresh
UUIDs on every run, and circuit JSON embeds an absolute source path and emits
its advisory warnings in a different order each time. Neither says anything
about the design, and rewriting them on every render buried real changes under
1900 lines of churn.

## Two things that will bite you

**This directory keeps its own `node_modules` on purpose.** Installing tscircuit
at the repository root rehoists the workspace and moves `next` out of where
eslint expects to find it, which breaks lint and build for everyone who has
never opened a schematic. It is installed on demand by `make hw-deps` and is not
part of `yarn install`.

**The tscircuit CLI runs under [bun](https://bun.sh), not node.** Under node it
cannot load a `.tsx` entry point at all, and the error it gives says nothing
about bun. `tools/build-hardware.mjs` checks for bun and says so.

## Layout of the sheet

The schematic is arranged to read the same way the
[system block diagram](../data/system.yaml) does, because two drawings of the
same board that disagree about where things are is a tax on every reader:

- Power along the bottom left: USB-C, charger, cell, 3V3 rail.
- The microcontroller in the middle, with power and the host connection on its
  left side and every bus on its right.
- Flash to the left, since QSPI leaves that side.
- Sensors, radio, GNSS and the recovery aids in a column on the right.

Pin placement on the microcontroller is set explicitly with
`schPinArrangement`. Left to the default, pins land in declaration order and
every net crosses the part.

## Arming, and why it is a GPIO rather than a power switch

The flight state machine's first transition is operator-driven, so the board
carries an arming input. Arming is what starts the pressure reference settling
and the pre-arm ring buffer, and without an input there is no flight.

It is a GPIO, not a switch in the power path. oApogee is a passive payload, so
cutting power arms nothing and protects nobody. What the operator actually needs
is to power the payload up, let the GNSS get a fix and the barometer settle, and
then arm it once the rocket is on the rail, which a power switch cannot express.

The input is pulled up and the switch pulls it down, because a floating input
reads as noise and an input that reads as noise arms a rocket at random.

Mechanical, a slide switch, rather than a reed or Hall sensor. The
sealed-enclosure argument for a magnetic switch never applied here, because the
pod has static ports cut through it and must have. What is left is a trade of an
inspectable state for one anything magnetic in a range box can change, and arm
state is what an operator confirms by eye at the pad. Consequence: the pod needs
an aperture for it, which it does not have yet.

## Battery sense

The telemetry format transmits a battery voltage in every FLIGHT and STATUS
packet, and beacon endurance after landing is the number that decides whether a
payload is findable the next morning. Neither is measurable without a divider: a
single cell reaches 4.2 V, which is above the 3V3 rail and above what the
microcontroller's ADC will accept.

## Open items

- Design the radio's RF front end. Semtech publishes the topology and not the
  matching values, and this is the one thing between the board and a fabricator.
  The argument is in `docs/open-questions.md`.
- Set the charge current programming resistor, which follows from the cell
  capacity, which follows from an endurance requirement nobody has measured.
- Size the battery sense divider against the microcontroller's ADC input range
  and its input impedance, and decide whether it needs switching off between
  readings to stop it draining the cell on the pad.
- Wire the IMU's data-ready interrupt. Polling a high rate sensor gives sample
  intervals that jitter with whatever else the loop is doing, and the log stores
  a timestamp per record precisely because that jitter is real.

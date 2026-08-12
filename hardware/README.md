# oApogee hardware

The circuit, as source. `oapogee.tsx` is a [tscircuit](https://tscircuit.com)
design: the schematic is written as code, and the drawing, the netlist, the
machine-readable circuit JSON and a KiCad schematic are all rendered from it.

That is what makes the openness promise checkable rather than rhetorical. A
project whose first letter means open should publish the KiCad project, not a
PDF of a schematic, and this directory is where that comes from.

## What this is and is not

**This is a netlist. It is not a pinout.**

It says what connects to what, which is the design decision worth reviewing. It
does not say which physical pin of any package a signal lands on. The pin
numbers in the source are structural placeholders assigned in the order the
labels are written, not read from a datasheet.

That is deliberate. This project's rule is that it never publishes a number it
has not measured or sourced, and a datasheet pin number recalled from memory is
exactly the kind of confident, plausible, wrong figure the rule exists to
prevent. Mapping these functional pins onto real packages is a separate step,
done against datasheets, and it is tracked as an open item.

**Do not lay a board out from this and do not send anything here to a
fabricator.**

There is also deliberately no PCB or 3D output. A layout needs real footprints,
real footprints need the pin mapping that has not been done, and a routed board
built on placeholder pins would look far more finished than it is.

## Building it

```bash
make hw-deps   # once, installs tscircuit here
make hw        # render the artifacts into apps/web/public/hardware/
make check-hw  # verify the committed artifacts match the source
```

Outputs, in order of how much they can be trusted:

| Artifact | What it is |
|---|---|
| `oapogee-netlist.txt` | The connectivity, as text. Diffable, reviewable, and the thing to read first. |
| `oapogee-schematic.svg` | The same connectivity, drawn. |
| `oapogee.kicad_sch` | A KiCad schematic, for anyone who wants to take the design further. |
| `oapogee-circuit.json` | Machine readable, for anyone writing their own tooling. |

`make check-hw` compares the netlist and the schematic against a fresh render,
so the committed drawing cannot quietly stop matching the source. Circuit JSON
is excluded from that comparison because it embeds absolute paths from whichever
machine produced it.

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

TODO(confirm): whether the switch is mechanical or magnetic. A slide or screw
switch is the rocketry convention and needs a hole in the pod. A reed or Hall
sensor arms through the wall with a magnet, which keeps the enclosure sealed
apart from its static ports, at the cost of a part that can be armed by anything
magnetic nearby. Decide before the enclosure is designed, because it changes the
pod.

## Battery sense

The telemetry format transmits a battery voltage in every FLIGHT and STATUS
packet, and beacon endurance after landing is the number that decides whether a
payload is findable the next morning. Neither is measurable without a divider: a
single cell reaches 4.2 V, which is above the 3V3 rail and above what the
microcontroller's ADC will accept.

## Open items

Listed at [oapogee.space/status](https://oapogee.space/status):

- Map every functional pin onto a real package pin, from datasheets.
- Choose the buck-boost regulator, currently a placeholder part.
- Set the charge current programming resistor, which follows from the cell
  capacity, which follows from an endurance requirement nobody has measured.
- Size the battery sense divider against the microcontroller's ADC input range
  and its input impedance, and decide whether it needs switching off between
  readings to stop it draining the cell on the pad.
- Decide the decoupling network properly rather than one capacitor per rail.
- Wire the IMU's data-ready interrupt. Polling a high rate sensor gives sample
  intervals that jitter with whatever else the loop is doing, and the log stores
  a timestamp per record precisely because that jitter is real.

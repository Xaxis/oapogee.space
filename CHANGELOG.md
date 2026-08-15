# Changelog

Notable changes to oApogee, hardware, firmware, and documentation together.

Errata are safety information. When hardware and firmware ship, a change to a
detection threshold or a known defect in a board revision appears here, because
somebody flying the previous version needs a way to find that out.

Dates are ISO. Versions follow the repository's `VERSION` file.

## Unreleased

### Added

- **The board.** The circuit source carries footprints and placement for all 27
  parts, the RP2350's 60 pins transcribed from the Raspberry Pi datasheet, and
  the layout routes: 95 traces, 154 vias, nothing unrouted. Gerbers, drill
  files, bill of materials and pick and place are published, gated behind a
  verification pass that refuses to write them while any blocker is open.
- **Firmware.** Sixteen core modules in freestanding C11, host-testable with
  nothing but cmake and a compiler, thirteen test binaries. The passive boundary
  is enforced by a check over the sources rather than asserted in a comment, and
  every flight threshold starts unset so the payload refuses to arm on a guess.
- **A second implementation of the packet format**, in TypeScript, written from
  the specification rather than from the C. `make crossimpl` builds fifteen
  canonical packets with both and compares them byte for byte, which is evidence
  a conformance test cannot produce.
- **A ground station in the browser.** WebSerial receiver with a simulated
  flight, and a live packet decoder on the specification page.
- **The printed parts**, as OpenSCAD source rather than only STLs: an internal
  sled and a two-piece external pod, generated from twenty-eight dimensions that
  each carry where they came from.
- Two format specifications, published so third parties can write their own
  tools. `docs/spec/telemetry-packet.md` fixes the downlink wire format at
  version 1; `docs/spec/log-format.md` fixes the onboard log at version 1. Both
  carry a reference implementation and conformance rules.
- Structured source for the bill of materials, tiers, glossary, flight state
  machine, preflight checklist, troubleshooting index, system diagram, mechanical
  dimensions, suppliers, and the flight log submission format.
- The system block diagram, generated from `data/system.yaml` rather than drawn,
  with a build check that fails when the committed SVG stops matching the data.
- Content pages: start here, build guide, firmware and flashing, mounting,
  ground station, reading your data, safety and rules, reference, FAQ, about and
  license.
- **Checks, because a rule nobody enforces is a rule that gets broken.** Prose
  style; every physical quantity in published prose having a stated source; data
  cross-references; internal links, now including the ones written in the site's
  own source; no page scrolling sideways on a phone; every drawing actually
  panning and zooming with the drawing in frame; the firmware's passive boundary
  and its ban on hardcoded thresholds; the two packet implementations agreeing;
  the printed parts matching their dimensions; the board's own design rules; and
  supplier links resolving.

### Decided

- **No microSD socket.** Card sockets unseat under boost, and the failure mode
  is the worst available: the flight proceeds normally and the data is gone.
  Storage is soldered-down QSPI flash with LittleFS.
- **Passive instrumentation only.** No ejection charge control, no deployment
  control, no igniters, no pyrotechnics, at any tier, ever. The boundary keeps
  the worst outcome of any bug in this project a disappointing graph.
- **Raw and derived values are both logged.** Twelve of thirty-six bytes per
  record, spent so the payload's own work can be audited and so old flights can
  be reprocessed with a better filter.
- **No encryption on the downlink**, so the link stays legal to transmit under
  amateur radio rules. Compact binary is efficiency; scrambling would be
  obfuscation.
- **Two build paths.** Modules, on breakout boards, buildable before a PCB
  exists. Board, the custom PCB, whose fabrication files now exist. The tier owns
  the capability content and the path owns only the assembly content.
- **The high-g accelerometer is optional at every tier, including Solo.**
  Whether an IMU saturates depends on the motor, not on whether the board has a
  radio, and tying the part to Link left the cheapest build most likely to record
  a boost phase that is silently wrong.
- **The arming switch is mechanical.** The case for a magnetic one was a sealed
  enclosure, and the pod is deliberately vented, so sealing was never on offer.
  What is left is trading a state you can see for one a stray magnet can change.
- **The sled is sized for BT-55, and the 24 mm case is pod only.** A 22 mm board
  in a BT-50 bore leaves 0.6 mm of rail, which a slicer will emit and a landing
  will break.
- **WebSerial for the ground station**, with the browser requirement stated
  prominently and a documented terminal fallback. It excludes iOS and the
  alternatives are worse; the wire format is published so somebody else can build
  an iOS client without asking.
- **A complementary filter for v1.** Tuning a Kalman filter would mean inventing
  covariances for sensors that have never flown.
- **`SIM` stays a flag, not a packet type**, with the conformance rule raised
  from should to must: a parallel type for everything simulable would double the
  type space to encode a property orthogonal to the packet's contents.
- **Log block checksums go in the manifest** every flight already writes, which
  keeps the flat-array property that makes the format one line to load.
- **The watchdog is enabled in flight**, with the peak altitude committed as it
  updates. A hung payload cannot be found; a reset keeps the beacon alive, and
  the cost of the reset is the part that is fixable.
- **Licensing.** CERN-OHL-S 2.0 for hardware, Apache 2.0 for firmware and
  software, CC BY-SA 4.0 for documentation. Reciprocal on the board because the
  plausible bad outcome is a closed volume clone and `-S` at least requires that
  improvements come back; permissive on the firmware because that is the part
  worth having other people embed everywhere, with Apache over MIT for the
  express patent grant. The documentation gets share-alike, accepting that a
  competitor may reproduce the build guide commercially with attribution.
- **Trademark policy.** The design is open, the name is not. Licences do not
  stop a clone shipping under this name; a trademark does. Not registered yet:
  registration buys enforceability against a problem that does not exist while
  nothing is being sold.

Every one of these is argued out, with the case against, in
[docs/open-questions.md](https://github.com/Xaxis/oapogee.space/blob/main/docs/open-questions.md).

### Status

**Nothing physical exists.** No board has been fabricated, assembled, weighed,
priced, or flown. Every cost, mass, range and endurance figure on the site is
absent rather than estimated, and each gap records what evidence would close it.

The PCB is a real step past a paper design and is not the same as a board that
works. Its fabrication files are published and one thing stands between them and
an order worth placing: 152 signal names are mapped onto footprint pads by pin
number, and only the microcontroller's 60 have been checked against a datasheet.
The [schematic page](https://oapogee.space/reference/schematic) says so on every
build, generated from the check rather than written by hand.

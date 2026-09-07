# Changelog

Notable changes to oApogee, hardware, firmware, and documentation together.

Errata are safety information. When hardware and firmware ship, a change to a
detection threshold or a known defect in a board revision appears here, because
somebody flying the previous version needs a way to find that out.

Dates are ISO. Versions follow the repository's `VERSION` file.

## Unreleased

### Added

- **The board.** The circuit source carries footprints and placement for 49
  parts, and the layout routes with nothing unrouted. Every package's pin
  numbering is transcribed from its manufacturer's own datasheet rather than
  taken from a footprint library, with the table or figure cited beside it in
  the source.

  The fabrication package is gated behind a verification pass that refuses to
  write it while any blocker is open, and there are open blockers, so it is not
  published. That gate is the feature. It has already stopped one board whose
  radio had no reference oscillator from being orderable.
- **Firmware.** Sixteen core modules in freestanding C11, host-testable with
  nothing but cmake and a compiler, thirteen test binaries. The passive boundary
  is enforced by a check over the sources rather than asserted in a comment, and
  every flight threshold starts unset so the payload refuses to arm on a guess.
- **A second implementation of the packet format**, in TypeScript, written from
  the specification rather than from the C. `make crossimpl` builds a canonical
  packet of every type and shape with both and compares them byte for byte,
  which is evidence a conformance test cannot produce. It reports the count
  rather than this file repeating it.
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

### Fixed

- **The site said you could order a board, and you cannot.** Six places claimed
  the fabrication files were published: the homepage, Start here, About, the
  FAQ, the bill of materials, and the schematic page's own warning box, which
  said it twenty lines above the generated list of blockers contradicting it.
  `make fab` has been refusing to write those files the whole time. The claim is
  now tied to the check that decides it: while `hardware/pcb-status.json` says
  `fab_ready` is false, `make data` fails on any prose or component that says
  the package is published or the board is orderable.
- **The site said the pinouts were unchecked, and they are not.** The commit
  that transcribed all seven remaining packages from their datasheets did not
  update the six documents saying only the microcontroller's sixty pins had been
  read, including the circuit source's own header. What is genuinely unverifiable
  is narrower, and it is what those places now say: no check here can confirm
  that a footprint's pad numbering matches the numbering its datasheet uses.
- **Five documents linked to a page that was deleted.** `oapogee.space/status`
  went when the marker summary did, and the link checker skipped it because it
  was written as an absolute URL and treated as external. It now normalises
  links to this site's own domain and checks them like any other, and it reads
  `docs/`, `hardware/README.md` and `firmware/SAFETY.md` as well.
- The Reference page said the system block diagram was on the bill of materials
  page. It is on the homepage. It also hardcoded the number of open fabrication
  blockers, in the same paragraph as the sentence explaining that the list is
  generated so it cannot go stale. It had gone stale.
- `hardware/README.md` still described a netlist with placeholder pin numbers
  and told the reader not to lay a board out from it. The board is laid out. Its
  open items also still listed the arming switch and the decoupling network,
  both of which are decided.
- `docs/page-map.md` was a proposal asking for approval of decisions taken
  months earlier, naming four data files that were never created and a
  navigation entry for a route that does not exist. It now describes the site as
  built.
- Six scratch exports from routing experiments, about 5 MB, were committed and
  referenced by nothing. They are gone and the pattern is ignored.
- **`firmware/gen/oa_states.h` said it was generated by a tool that did not
  exist.** It carried a note admitting as much. The flight state enumeration has
  four consumers and the wire value in every packet header and every log record
  comes from it, so a hand-written copy of the YAML was the one place in this
  repository where that value could drift silently. `tools/gen-states.mjs` now
  exists, emits the file that was committed by hand, and `make data` fails when
  the two disagree.
- **Two links in the packet specification 404'd on the site.** `docs/spec/`
  renders as two public pages, so a relative repository path in one resolves
  against the page URL: `./log-format.md` landed on `/reference/log-format.md`.
  The link checker verified them by existence on disk, which said nothing about
  the page. It now rejects a repository path in a file that renders.
- **`make mech` could never leave the tree clean.** OpenSCAD does not render
  the same source to the same bytes twice: three runs of an unchanged pod gave
  three different files with the same 862 vertices and the same 1736 triangles,
  52 of them splitting a flat face along the other diagonal. So running it to
  check something rewrote all three STLs, and a real change to a model arrived
  in a binary diff indistinguishable from a no-op one. The render now compares
  the solid rather than the file, using the properties a retriangulation cannot
  change, and keeps the committed bytes when it is the same part. Two
  consecutive runs now leave nothing modified.
- **The 404 page was a dead end that described a site from months ago.** It said
  most of oApogee was unwritten, which stopped being true, and pointed at "the
  navigation", which is not on that page: it sits outside the docs shell and had
  three header links. It renders the route table now, from the same list the
  sidebar is built from.
- **`make check-suppliers` checked seven of the twenty-two external links**, and
  the fifteen it skipped were the ones that matter: every datasheet a pinout was
  transcribed from, and the eCFR, NAR and Tripoli pages the safety and radio
  sections send a reader to instead of paraphrasing. It is `make check-external`
  now and covers all of them.
- One question in `docs/open-questions.md` said neither **Decided** nor
  **Open**, which is the quiet decision that file exists to prevent: a reader
  could not tell whether the board was designed around an answer about the
  microcontroller or around an assumption. `make data` now fails on a question
  that states no verdict.
- The share card read "Solo . Link . Track", with full stops where middots were
  meant, in the one image that represents the project in every chat it is
  pasted into.
- Three comments justified a design decision with a count that had drifted: the
  search index said twenty-one pages when there are twenty, the tools list said
  eight of sixteen carry no note when it is seven of nineteen, and the packet
  spec's own changelog entry said fifteen canonical packets when the tool
  reports sixteen. The counts are gone rather than corrected.
- `firmware/README.md` described a tree of stubs with no tests, and
  `core/allowed-undefined.txt` said the symbol check had only ever run against
  files containing no implementations. There are sixteen implemented modules and
  thirteen test binaries. `README.md` called the project a design on paper and
  claimed the build fails when a marker index goes stale; there is no marker
  index, and what the build actually enforces is stronger. `CONTRIBUTING.md`
  said there was no firmware to debug and listed four of the fourteen checks CI
  runs. `CONTENT-STYLE.md` documented three of the four markers, and its
  checkpoint example taught the fixed pressure band the build guide was
  corrected away from.
- **The board had no crystal.** XIN and XOUT were declared on the
  microcontroller and connected to nothing. The RP2350 runs without one, so
  nothing complained, and the board would have been unflashable: USB needs an
  accurate reference and USB is its only connector. Now Raspberry Pi's own
  circuit, their crystal, their capacitor values, their damping resistor.
- **The GNSS receiver had no antenna.** RF_IN was declared and connected to
  nothing, which is a Track tier that cannot produce a position, the one thing
  that tier is for. It now has a coaxial socket of its own.
- **A footprint that did not exist produced a part with no pads.** Asking
  tscircuit for a footprint it does not have is not an error that stops
  anything: it logs a message, exits zero, and builds the component with no
  copper, so the part is on the schematic and its net silently has none. Both
  coaxial sockets now use a land pattern defined in the source.
- Three new checks, one per defect above, because each of them built cleanly,
  routed cleanly, exported a clean fabrication package and passed every check
  in this repository. A named pin must now be connected or listed with the
  reason it is not; a rejected component property is a blocker; and the count
  of copper pairs too close together for the cheaper fabricator is ratcheted
  rather than only printed, after it doubled in one commit with nothing to say
  so.
- The ground station page said every packet carries a device identifier. The
  header has no identifier field. It now says so, says what to do instead, and
  the question of whether one belongs there is in `docs/open-questions.md`.
- Troubleshooting had path gating in the data that the page never rendered, so
  Modules builders were sent looking for parts that only exist on the custom
  board.

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
  exists. Board, the custom PCB. The tier owns the capability content and the
  path owns only the assembly content.
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
works. Its fabrication files are not published, because the check that gates
them refuses while anything is open, and things are. The schematic page lists
them, generated on every build rather than counted here.

The one that is a design problem rather than an autorouter problem is the
radio's RF front end. Between the transceiver's transmit output, its receive
inputs and one antenna, the reference design puts a switch and about fourteen
matching components whose values Semtech does not print in the datasheet. That
argument is in `docs/open-questions.md` with the three ways to close it.

The [schematic page](https://oapogee.space/reference/schematic) says all of this
on every build, generated from the check rather than written by hand.

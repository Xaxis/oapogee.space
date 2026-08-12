# Changelog

Notable changes to oApogee, hardware, firmware, and documentation together.

Errata are safety information. When hardware and firmware ship, a change to a
detection threshold or a known defect in a board revision appears here, because
somebody flying the previous version needs a way to find that out.

Dates are ISO. Versions follow the repository's `VERSION` file.

## Unreleased

### Added

- Two format specifications, published so third parties can write their own
  tools. `docs/spec/telemetry-packet.md` fixes the downlink wire format at
  version 1; `docs/spec/log-format.md` fixes the onboard log at version 1. Both
  carry a reference implementation and conformance rules.
- Structured source for the bill of materials, tiers, glossary, flight state
  machine, preflight checklist, troubleshooting index, system diagram, and the
  flight log submission format.
- The system block diagram, generated from `data/system.yaml` rather than drawn,
  with a build check that fails when the committed SVG stops matching the data.
- Content pages: start here, build guide, firmware and flashing, mounting,
  ground station, reading your data, safety and rules, reference, FAQ, about and
  license.
- Build checks: prose rules, data cross-references, internal links and anchors,
  the verification marker index, and the schematic.

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
  exists. Board, the custom PCB, once fabricated. The tier owns the capability
  content and the path owns only the assembly content.

- **Licensing.** CERN-OHL-S 2.0 for hardware, Apache 2.0 for firmware and
  software, CC BY-SA 4.0 for documentation. Reciprocal on the board because the
  plausible bad outcome is a closed volume clone and `-S` at least requires that
  improvements come back; permissive on the firmware because that is the part
  worth having other people embed everywhere, with Apache over MIT for the
  express patent grant. The documentation gets share-alike, accepting that a
  competitor may reproduce the build guide commercially with attribution.
- **Trademark policy.** The design is open, the name is not. Licences do not
  stop a clone shipping under this name; a trademark does.

### Not yet decided

- Whether the high-g accelerometer belongs at Link and above, as the project
  brief specifies, or as an option at every tier driven by motor class. See
  `NOTES-FOR-WIL.md`.
- Whether the ground station display uses WebSerial, which excludes iOS
  entirely.
- Complementary filter or Kalman filter for the sensor fusion.

### Status

**Nothing has been fabricated, assembled, weighed, priced, or flown.** Every
cost, mass, range and endurance figure on the site is absent rather than
estimated, and each gap records what evidence would close it. See
[TODO-VERIFY.md](TODO-VERIFY.md), which is generated from the content files and
checked in the build.

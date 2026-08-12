---
title: Reference
description: Schematic, pinout, mechanical drawings, wire format specifications, and errata. No hand-holding.
tier: all
difficulty: advanced
time_estimate: null
updated: 2026-08-11
status: draft
---

# Reference

Everything an engineer needs, with the explanation stripped out. If you want the
reasoning, it is on the page that owns each subject.

## Specifications

- [Telemetry packet format](/reference/telemetry-packet), version 1. The
  downlink wire format: byte layout, packet types, field encodings, CRC, and
  receiver conformance rules.
- [Onboard log format](/reference/log-format), version 1. Directory layout, the
  self-describing manifest, fixed-width record layouts, and reader conformance
  rules.

Both are published so that third parties can write their own tools without
reverse engineering anything, and both carry a reference implementation.

## System block diagram

The generated system diagram is on the [bill of materials](/bom) page. It is
rendered from `data/system.yaml`, whose nodes reference part identifiers in
`data/bom.yaml`, and the build fails if the committed diagram stops matching the
data it was drawn from.

## Flight state machine

| Value | State | Transitions to | On |
|---|---|---|---|
| 0 | `PAD_IDLE` | `ARMED` | Operator arms the payload |
| 1 | `ARMED` | `BOOST` | Sustained upward acceleration, held for N samples |
| 2 | `BOOST` | `COAST` | Acceleration crosses zero, burnout |
| 3 | `COAST` | `APOGEE` | Altitude decreasing across N consecutive samples |
| 4 | `APOGEE` | `DESCENT` | Sustained descent confirmed |
| 5 | `DESCENT` | `LANDED` | Altitude and acceleration quiet for a sustained period |
| 6 | `LANDED` | none | Terminal until power cycle |

The same enumeration appears in the firmware, the log format, and the packet
format. Values 7 to 255 are reserved.

TODO(verify): every threshold marked N above is unset. They must come from
measured sensor noise on real hardware, not from intuition, and each carries its
own verification note in `data/flight-phases.yaml`.

## Flags

Identical bit assignments in the packet header and in each log record.

| Bit | Mask | Name | Meaning when set |
|---|---|---|---|
| 0 | 0x01 | `GNSS_FIX` | Position fix available |
| 1 | 0x02 | `HIGH_G` | High-g accelerometer present and healthy |
| 2 | 0x04 | `BARO_FAULT` | Barometer failed or implausible |
| 3 | 0x08 | `IMU_FAULT` | IMU failed or implausible |
| 4 | 0x10 | `LOG_FULL` | Storage full, logging stopped |
| 5 | 0x20 | `LOW_BATT` | Battery below threshold |
| 6 | 0x40 | `SIM` | Bench test or simulation, not a flight |
| 7 | 0x80 | reserved | Transmit 0, ignore on receive |

## Parts

Manufacturer part numbers, roles, tier membership, substitutes, and the
reasoning behind each choice are on the [bill of materials](/bom), generated
from `data/bom.yaml`.

## Schematic and PCB

TODO(confirm-on-hardware): the KiCad project does not exist. When it does,
publish the project itself rather than a PDF export, along with Gerbers, a pick
and place file, and the assembly bill of materials.

## Pinout

TODO(confirm-on-hardware): the pin assignment is not fixed. It cannot be
published until the schematic exists, and publishing a provisional one would
guarantee that somebody builds against it.

## Mechanical

TODO(confirm-on-hardware): board outline, mounting hole positions and diameters,
connector positions, and the maximum component height on each side. Also the
sled and pod models, published as parametric source rather than only as exported
STLs.

## Firmware

TODO(confirm-on-hardware): configuration file reference, serial command
reference, and the self-test output format.

## Errata

None recorded, because nothing exists to have errata against.

When hardware ships, this section carries every known defect, with the revision
affected and the workaround. Errata for a flight computer are safety
information: if a firmware release changes an apogee detection threshold,
somebody flying the previous build needs a way to find that out.

## Changelog

Not yet started. See the repository history at
[github.com/Xaxis/oapogee.space](https://github.com/Xaxis/oapogee.space).

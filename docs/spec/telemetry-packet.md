---
title: Telemetry packet specification
description: The oApogee downlink wire format. Byte layout, packet types, field encodings, and the rules a conforming receiver must follow.
tier: link
difficulty: advanced
time_estimate: null
updated: 2026-08-11
status: draft
spec_version: 1
---

# Telemetry packet specification

This is the format oApogee Link and oApogee Track transmit over the air. It is
published so that anyone can write a receiver, a decoder, or a logger without
reverse engineering anything.

This document is normative. Where the firmware and this document disagree, one
of them is a bug, and which one is decided by whichever is easier to change
without breaking existing receivers.

**Version 1. Status: draft.** Nothing has been transmitted by real hardware yet.
The layout is settled enough to implement against, and the field ranges have
been chosen deliberately, but no byte of this has been observed on a radio.

## Design constraints, and what they cost

Four constraints shaped every decision below.

**Airtime is the scarce resource.** LoRa trades data rate for range, and a
telemetry packet competes for the air with every retry of the packet before it.
A packet twice as long takes roughly twice as long to send, halving the update
rate at the same settings. So the format is byte-packed with no alignment
padding, no field names, and no self-description on the wire.

**The receiver may miss any packet, and must not care.** Every packet is
independent. There is no state carried between packets, no delta encoding, and
no requirement to have seen packet N to decode packet N+1. A ground station that
comes up halfway through a flight decodes the next packet it hears.

**No encryption, ever.** Amateur radio rules prohibit messages encoded to
obscure their meaning, and oApogee intends to remain legal for licensed
operators to transmit under Part 97. Binary and compact is efficiency; scrambled
is obfuscation. This format is the former and will not become the latter.

**Nothing here can command the vehicle.** The link is downlink-only in the sense
that matters: there is no packet type that causes the payload to do anything. A
receiver cannot arm it, cannot trigger anything, and cannot deploy anything,
because oApogee has no mechanism to deploy. See
[Safety and rules](../../content/safety.md).

The cost of all this: the format is not human-readable, it is not extensible in
place, and adding a field means a version bump. Those are accepted.

## Conventions

- All multi-byte integers are **little-endian**. Both ends of this link run on
  ARM Cortex-M cores that are natively little-endian, so this costs nothing on
  either side. A receiver on a big-endian host must byte-swap.
- Integers are two's complement where signed.
- There is no padding and no alignment. A struct overlay works on Cortex-M and
  x86 with packed attributes; a portable decoder should read field by field.
- Offsets are in bytes from the start of the packet.
- `u16`, `i32` and so on are unsigned and signed integers of that bit width.

## Packet structure

```
+----------------+------------------------+---------+
| header, 8 B    | body, type-dependent   | CRC 2 B |
+----------------+------------------------+---------+
```

Total length is fixed per packet type, so a receiver knows how many bytes to
expect from the type field alone. There is no length field, because a length
field costs a byte on every packet to describe something already known.

### Header, 8 bytes

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 1 | u8 | `hdr` | High nibble: format version. Low nibble: packet type. |
| 1 | 1 | u8 | `flags` | Bit field, see below. |
| 2 | 1 | u8 | `seq` | Increments per transmitted packet, wraps at 255. |
| 3 | 1 | u8 | `state` | Flight state, see below. |
| 4 | 4 | u32 | `t_ms` | Milliseconds since the transition into `ARMED`. Transmitted as 0 while `state` is `PAD_IDLE`. |

`hdr` packs version and type into one byte because both are small and a whole
byte for each is a byte per packet spent on nothing. Version 1 with a `FLIGHT`
packet is `0x12`.

A receiver **must** reject a packet whose version nibble it does not recognise,
rather than attempting to decode it. Field offsets are not guaranteed stable
across versions.

`seq` exists so a receiver can count losses and report link quality honestly. It
wraps every 256 packets, which is unambiguous at any realistic packet rate.

`t_ms` is zero at the moment of arming, not at power-on and not at launch. It is
a `u32` of milliseconds, giving a range of about 49 days, which is not a
constraint anyone will meet. The zero point is arming rather than launch because
launch detection happens after the fact, and a timebase that only becomes valid
retroactively is a poor timebase.

Before arming there is no elapsed time to report, so `t_ms` is transmitted as 0
in every packet whose `state` is `PAD_IDLE`. It is not the power-on uptime: the
payload's uptime counter is a firmware implementation detail and is never on the
air. A pad packet is therefore identified by `state`, not by a clock, and a run
of `STATUS` packets on the pad all carry `t_ms` of 0.

### Flags

This table is the normative home for the bit assignments. The mask column is
here so that nothing elsewhere has to restate them to be useful.

| Bit | Mask | Name | Meaning when set |
|---|---|---|---|
| 0 | `0x01` | `GNSS_FIX` | The GNSS receiver has a position fix. |
| 1 | `0x02` | `HIGH_G` | The high-g accelerometer is present and healthy. |
| 2 | `0x04` | `BARO_FAULT` | The barometer has failed or is returning implausible values. |
| 3 | `0x08` | `IMU_FAULT` | The IMU has failed or is returning implausible values. |
| 4 | `0x10` | `LOG_FULL` | Onboard storage is full. Logging has stopped. |
| 5 | `0x20` | `LOW_BATT` | Battery below the low threshold. |
| 6 | `0x40` | `SIM` | This packet was produced by a simulation or bench test, not a flight. |
| 7 | `0x80` | reserved | Must be transmitted as 0 and ignored on receive. |

`SIM` is not optional decoration. A bench test transmits real-looking packets,
and a flight log archive that cannot distinguish a bench run from a flight will
eventually publish a bench run as a flight. It is one bit and it prevents a
whole category of quiet data corruption.

Fault flags report an instrument that stopped working. They never cause a field
to be omitted, because omission would change the packet length. A faulted
sensor's fields carry the last value read, and the flag is what tells you not to
trust them. The alternative, transmitting a sentinel, produces a plot with a
spike in it rather than a gap, which is worse.

That rule governs a sensor that is reading badly. Position is the other case:
with no fix there is no last value worth carrying, so `lat_e7` and `lon_e7`
carry `INT32_MIN` in both `BEACON` and `POSITION`, and `GNSS_FIX` says which it
is. A sentinel is right when the quantity is absent and wrong when it is merely
suspect.

### Flight state

Values match `order` in `data/flight-phases.yaml`, which is the same enumeration
the firmware and the log format use.

| Value | State |
|---|---|
| 0 | `PAD_IDLE` |
| 1 | `ARMED` |
| 2 | `BOOST` |
| 3 | `COAST` |
| 4 | `APOGEE` |
| 5 | `DESCENT` |
| 6 | `LANDED` |

Values 7 to 255 are reserved. A receiver encountering one should display the
number rather than guessing, and must not treat it as an error.

## Packet types

| Type | Name | Body | Total | Sent |
|---|---|---|---|---|
| 0x1 | `STATUS` | 3 B | 13 B | On the pad, low rate |
| 0x2 | `FLIGHT` | 9 B | 19 B | Boost through descent, high rate |
| 0x3 | `APOGEE` | 8 B | 18 B | Immediately on apogee detection, repeated |
| 0x4 | `BEACON` | 12 B | 22 B | After landing, low rate |
| 0x5 | `POSITION` | 9 B | 19 B | Track only, interleaved |

Types 0x0 and 0x6 to 0xF are reserved.

### 0x1 STATUS

Sent while the payload is on the pad, so that a ground station shows something
before the flight and so the operator can confirm the link works before walking
away from the rail. Low rate by design.

| Offset | Size | Type | Field | Encoding |
|---|---|---|---|---|
| 8 | 2 | u16 | `pad_pressure_pa_off` | Ground reference pressure, offset pascals. |
| 10 | 1 | u8 | `batt` | Battery, see encoding below. |

`pad_pressure_pa_off` is the establishing pressure reference in whole pascals,
offset so the useful range fills 16 bits exactly: the transmitted value is
`pressure_pa - 50000`, and the 65536 representable values cover 50000 to 115535
Pa. The low end is roughly 5500 m of pressure altitude and the high end is above
any recorded surface pressure, so the band covers any plausible launch site. A
reading below 50000 Pa is transmitted as 0, a reading above 115535 Pa is
transmitted as 65535, and `BARO_FAULT` is set in either case.

The offset is what earns the resolution. Without it a u16 of whole pascals would
stop at 65535 Pa, which is lower than the pressure at any launch site anyone
flies from, so the reference would not be representable at all. With it, the
same two bytes carry 1 Pa steps across the whole band. Whole pascals also match
`calibration.pad_pressure_pa` in the [log format](./log-format.md), so the same
quantity is not on two scales in two specifications. This field is the zero the
entire altitude column is measured against, which is why it is not coarsened to
save arithmetic.

### 0x2 FLIGHT

The main telemetry packet. Sent at the highest rate the radio settings allow
from boost through descent.

| Offset | Size | Type | Field | Encoding |
|---|---|---|---|---|
| 8 | 4 | i32 | `alt_cm` | Fused altitude above the pad, centimetres. |
| 12 | 2 | i16 | `vel_dm_s` | Fused vertical velocity, decimetres per second. |
| 14 | 2 | i16 | `accel_cg` | Vertical acceleration, hundredths of g. |
| 16 | 1 | u8 | `batt` | Battery, see encoding below. |

`alt_cm` is signed, and negative values are legitimate rather than a bug. A
barometric zero taken on the pad reports negative altitude if the rocket lands
below the pad or if pressure rises during the flight. A receiver that clamps
this at zero is hiding a real measurement, and there is a troubleshooting entry
for exactly this confusion.

`vel_dm_s` in decimetres per second covers plus or minus 3276.7 m/s, which is
far more than needed and costs nothing over a tighter scale. Positive is up.

`accel_cg` in hundredths of g covers plus or minus 327.67 g. This is
deliberately wider than a 6-axis IMU can measure, because the field carries the
high-g accelerometer's reading when that part is fitted. When `HIGH_G` is clear,
the value came from the IMU and is subject to saturation: a boost that reads a
flat value at the IMU's full-scale limit is saturation, not a constant
acceleration.

### 0x3 APOGEE

Sent the instant apogee is detected, then repeated a small number of times.

Apogee is the number the entire payload exists to produce, and it is the number
most likely to be lost: it happens once, at the greatest distance from the
receiver, and it is followed by the deployment event that is the most likely
moment for the payload to be damaged. So it gets its own packet type, sent
immediately rather than waiting for the next scheduled transmission, and
repeated so a single dropped packet does not lose it.

| Offset | Size | Type | Field | Encoding |
|---|---|---|---|---|
| 8 | 4 | i32 | `apogee_cm` | Peak altitude above the pad, centimetres. |
| 12 | 4 | u32 | `t_apogee_ms` | Time of apogee, milliseconds since arming. |

`t_apogee_ms` is the estimated time of the apogee event, which is earlier than
the `t_ms` in the header of the packet reporting it. The difference is the
detection lag, and a receiver may display it. Apogee detection requires
confirmation across multiple descending samples, so the lag is real and
non-zero.

TODO(verify): state the detection lag in milliseconds once the confirmation
sample count is chosen from measured barometer noise. A reader is entitled to
know the size of the error in the recorded apogee time.

### 0x4 BEACON

Sent after landing, at a long interval, to conserve battery while remaining
findable.

| Offset | Size | Type | Field | Encoding |
|---|---|---|---|---|
| 8 | 4 | i32 | `lat_e7` | Latitude, degrees times 10^7. `INT32_MIN` if no fix. |
| 12 | 4 | i32 | `lon_e7` | Longitude, degrees times 10^7. `INT32_MIN` if no fix. |
| 16 | 4 | i32 | `apogee_cm` | Peak altitude of the flight just completed. |

Degrees times 10^7 is the u-blox native scaling, so no conversion happens on the
payload, and its resolution is far finer than the position accuracy, which means
the encoding never limits the fix.

A Solo or Link build with no GNSS transmits `INT32_MIN` in both position fields
and leaves `GNSS_FIX` clear. `INT32_MIN` is used rather than zero because zero
is a real coordinate.

The beacon repeats the flight's apogee so that a walkaway recovery still yields
the number even if the onboard log is unreadable afterwards.

### 0x5 POSITION

Track only. Interleaved with `FLIGHT` packets during flight at a lower rate,
because position changes slowly compared to altitude and is not worth the
airtime at the flight packet rate.

| Offset | Size | Type | Field | Encoding |
|---|---|---|---|---|
| 8 | 4 | i32 | `lat_e7` | Latitude, degrees times 10^7. `INT32_MIN` if no fix. |
| 12 | 4 | i32 | `lon_e7` | Longitude, degrees times 10^7. `INT32_MIN` if no fix. |
| 16 | 1 | u8 | `sats` | Satellites used in the fix. Zero when `GNSS_FIX` is clear. |

`POSITION` is transmitted on its scheduled slot whether or not the receiver has
a fix. Suppressing it would make a fix outage indistinguishable from a lost
packet, and `seq` gaps are supposed to mean lost packets and nothing else. When
`GNSS_FIX` is clear, both position fields carry `INT32_MIN` and `sats` carries
zero, for the same reason `BEACON` uses that sentinel: zero is a real
coordinate.

`GNSS_FIX` in the header is authoritative. A receiver **must** check it before
plotting, and **must not** plot a position from a packet whose `GNSS_FIX` is
clear, even if the coordinate fields look plausible. A fix lost under boost is
normal, and the log format records the same outage as a gap rather than as a
coordinate.

## CRC

The last two bytes of every packet are a CRC-16/CCITT-FALSE over all preceding
bytes of the packet, transmitted little-endian.

Parameters: polynomial `0x1021`, initial value `0xFFFF`, no input reflection, no
output reflection, no final XOR.

**Why a CRC when LoRa already has one.** The radio's CRC covers the air
interface and nothing else. It does not cover the serial link between the
receiver module and the host, it does not cover a receiver library that hands
back a truncated buffer, and it does not cover a payload firmware bug that
assembles a malformed packet and transmits it correctly. Two bytes on a
nineteen-byte packet is a tenth of the payload, and it is worth it: a corrupted
altitude that decodes cleanly is indistinguishable from a real one, and this
project's entire claim is that its numbers are true.

TODO(verify): state what those two bytes actually cost in airtime, at the
spreading factor and bandwidth oApogee ends up using. It is not a tenth. LoRa
airtime is quantised into symbols and carries a preamble and header that a
short packet does not amortise, so the fraction has to be computed from the
modem's own timing formula and stated with the settings it assumes.

A receiver **must** discard any packet whose CRC does not match, and **should**
count discards so link quality can be reported.

## Transmission scheduling

Rates are not specified here as absolute numbers, because the achievable rate
depends on the spreading factor, bandwidth and coding rate, and those are
configuration rather than protocol.

The scheduling rules that are normative:

1. `APOGEE` preempts everything. It is queued the moment apogee is detected and
   transmitted at the next opportunity, ahead of any pending packet.
2. `FLIGHT` runs at the fastest sustainable rate from `BOOST` through `DESCENT`.
3. `POSITION` is interleaved at a fraction of the `FLIGHT` rate on Track builds.
4. `STATUS` runs slowly during `PAD_IDLE` and `ARMED`.
5. `BEACON` runs slowly after `LANDED`, and the interval lengthens over time to
   trade update rate for endurance during a long search.

TODO(verify): publish the actual intervals for each state at the shipped radio
configuration, measured, along with the resulting duty cycle, so that anyone
checking regional duty cycle limits can do the arithmetic. This matters for EU
and UK builds in particular.

## Conformance

A conforming receiver:

- **must** reject packets with an unrecognised version nibble
- **must** verify the CRC and discard packets that fail
- **must** treat unknown packet types as unreadable rather than misparsing them
- **must** treat unknown state values as unknown rather than as an error
- **must not** clamp `alt_cm` at zero
- **must not** treat `t_ms` as elapsed time while `state` is `PAD_IDLE`, where it
  is always 0, and must not compute an interval spanning the transition out of
  `PAD_IDLE`
- **must not** plot a position from a packet whose `GNSS_FIX` is clear, in which
  case both coordinate fields carry `INT32_MIN`
- **should** count sequence gaps and CRC failures and expose both
- **must** display the `SIM` flag prominently, and must never publish a `SIM`
  packet to a flight archive as a real flight
- **should** surface fault flags next to the affected values rather than hiding
  them

## Reference decoder

Field by field, no struct overlay, portable to any host:

```python
import struct

MAGIC_VERSION = 1
TYPE_NAMES = {1: "STATUS", 2: "FLIGHT", 3: "APOGEE", 4: "BEACON", 5: "POSITION"}
BODY_LEN = {1: 3, 2: 9, 3: 8, 4: 12, 5: 9}
INT32_MIN = -(2 ** 31)


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def decode(packet: bytes) -> dict:
    if len(packet) < 10:
        raise ValueError("short packet")

    version = packet[0] >> 4
    ptype = packet[0] & 0x0F
    if version != MAGIC_VERSION:
        raise ValueError(f"unsupported version {version}")
    if ptype not in BODY_LEN:
        raise ValueError(f"unknown packet type {ptype}")

    expected = 8 + BODY_LEN[ptype] + 2
    if len(packet) != expected:
        raise ValueError(f"expected {expected} bytes, got {len(packet)}")

    (received_crc,) = struct.unpack_from("<H", packet, len(packet) - 2)
    if crc16_ccitt_false(packet[:-2]) != received_crc:
        raise ValueError("CRC mismatch")

    flags = packet[1]
    out = {
        "type": TYPE_NAMES[ptype],
        "flags": flags,
        "gnss_fix": bool(flags & 0x01),
        "high_g": bool(flags & 0x02),
        "baro_fault": bool(flags & 0x04),
        "imu_fault": bool(flags & 0x08),
        "log_full": bool(flags & 0x10),
        "low_batt": bool(flags & 0x20),
        "simulated": bool(flags & 0x40),
        "seq": packet[2],
        "state": packet[3],
        "t_ms": struct.unpack_from("<I", packet, 4)[0],
    }

    if ptype == 1:  # STATUS
        pad_off, batt = struct.unpack_from("<HB", packet, 8)
        out.update(
            pad_pressure_pa=50000 + pad_off,
            battery_v=2.5 + batt / 100.0,
        )
    elif ptype == 2:  # FLIGHT
        alt_cm, vel_dm_s, accel_cg, batt = struct.unpack_from("<ihhB", packet, 8)
        out.update(
            altitude_m=alt_cm / 100.0,
            velocity_m_s=vel_dm_s / 10.0,
            accel_g=accel_cg / 100.0,
            battery_v=2.5 + batt / 100.0,
        )
    elif ptype == 3:  # APOGEE
        apogee_cm, t_apogee_ms = struct.unpack_from("<iI", packet, 8)
        out.update(apogee_m=apogee_cm / 100.0, t_apogee_ms=t_apogee_ms)
    elif ptype == 4:  # BEACON
        lat_e7, lon_e7, apogee_cm = struct.unpack_from("<iii", packet, 8)
        # GNSS_FIX is authoritative, per Conformance. Deciding from the sentinel
        # alone would plot a coordinate sent with the flag clear, which is the
        # one thing a receiver must not do.
        no_fix = not out["gnss_fix"] or lat_e7 == INT32_MIN or lon_e7 == INT32_MIN
        out.update(
            lat=None if no_fix else lat_e7 / 1e7,
            lon=None if no_fix else lon_e7 / 1e7,
            apogee_m=apogee_cm / 100.0,
        )
    elif ptype == 5:  # POSITION
        lat_e7, lon_e7, sats = struct.unpack_from("<iiB", packet, 8)
        no_fix = not out["gnss_fix"] or lat_e7 == INT32_MIN or lon_e7 == INT32_MIN
        out.update(
            lat=None if no_fix else lat_e7 / 1e7,
            lon=None if no_fix else lon_e7 / 1e7,
            satellites=sats,
        )
    else:
        # Unreachable today. It exists so that adding a type to BODY_LEN without
        # a body branch fails loudly instead of returning a header-only decode
        # that looks like a packet with no body.
        raise ValueError(f"no body decoder for type {ptype}")

    return out
```

## Battery encoding

One byte, `battery_volts = 2.5 + batt / 100`. Range 2.50 V to 5.05 V in 10 mV
steps.

The floor is 2.5 V because a single cell below that is already past the point
its protection circuit should have disconnected, so finer resolution down there
would describe a state that should not occur. The ceiling covers USB present.

## Changing this format

Adding a field to an existing packet type changes its length and breaks every
existing receiver, so it is a version bump, not an addition.

The cheap ways to extend without a bump, in order of preference:

1. Use a reserved flag bit.
2. Use a reserved packet type. Old receivers reject unknown types cleanly, which
   is exactly the designed behaviour, so a new type is additive.
3. Bump the version nibble and publish version 2 alongside version 1.

The version nibble has 16 values. If this format reaches version 16 something
has gone wrong upstream of the format.

## Open questions

Decided: a flag, with the conformance rule strengthened to compensate. A
distinct type is not one type but a parallel type for every packet type that can
be simulated, which doubles the type space to encode a property orthogonal to
what the packet contains, and ten of the sixteen type values are already
reserved for extension. The risk that a receiver ignores the flag is addressed
directly instead: displaying `SIM` prominently is a **must**, not a *should*.

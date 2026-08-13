---
title: Onboard log format specification
description: How oApogee stores a flight on the payload. Directory layout, the self-describing manifest, fixed-width record layouts, and how to read one.
tier: all
difficulty: advanced
time_estimate: null
updated: 2026-08-11
status: draft
spec_version: 1
---

# Onboard log format specification

This is what oApogee writes to its own flash during a flight, and what you get
when you plug it into a laptop. It is published so that anyone can read a log
with their own tools, and so that a log written today is still readable in ten
years by someone who has never seen this project.

This document is normative.

**Version 1. Status: draft.** No flight has been logged. The layout is settled
enough to implement against and no byte of it has been written by real hardware.

## The three decisions this format is built on

**Fixed-width binary records, not CSV.** A flight computer writes samples in an
interrupt-adjacent context on a microcontroller with no floating point to spare
and a flash chip with a finite write bandwidth. Formatting a float to text costs
far more than storing four bytes, and text costs two to three times the space
for the same information. Converting to CSV is trivial on a laptop and expensive
on the payload, so it happens on the laptop.

**Self-describing through a manifest, not through a hardcoded layout.** Each
flight directory carries a `meta.json` that states the record layout in full:
every field, its offset, its type, and its scaling. A third-party tool reads the
manifest and can decode the records without knowing this document, and a log
written by a future firmware with an extra field still parses. The spec version
in the manifest tells you which document to consult when something is ambiguous;
it is not required to read the data.

The cost is a few hundred bytes of JSON per flight, which is nothing against the
records, and the benefit is that this format does not rot.

**One file per sample rate, not one interleaved stream.** Sensor samples come at
a high rate. GNSS fixes come at a low one. Interleaving them means either
padding the GNSS fields into every sensor record, which wastes most of the flash,
or tagging each record with a type, which makes the file no longer a flat array
and defeats the whole point of fixed-width. So they are separate files, each a
plain array of identical records, each loadable in one line:

```python
import numpy as np
records = np.fromfile("flight.bin", dtype=dtype_from_manifest)
```

## Directory layout

Logs live on the onboard flash under LittleFS. One directory per flight.

```
/flights/
  0001/
    meta.json      manifest: schema, calibration, layout, summary
    flight.bin     fixed-width sensor records, high rate
    gnss.bin       fixed-width position fixes, low rate (Track only)
  0002/
    ...
```

Directory names are zero-padded decimal, monotonically increasing, and never
reused. A flight number is assigned when the payload transitions into `ARMED`
and is written before any record is, so a flight that ends in a crash and a
truncated file still has a directory and a manifest.

**LittleFS, specifically, for a reason.** A payload can lose power at any
instant: an impact can break a battery connection, and a hard landing can reset
the microcontroller. A filesystem that corrupts on power loss loses the whole
flight, including the part that was already safely written. LittleFS is designed
to be power-loss resilient and to wear-level, and both properties are load
bearing here rather than nice to have.

## meta.json

Written when the flight directory is created, then rewritten at `LANDED` with
the summary filled in. The `summary` object is always present, including in the
manifest written at creation. Every member is `null` and `landed` is `false`
until the payload reaches `LANDED`. A manifest whose `landed` is still `false`
is itself information: the flight ended before landing was detected.

```json
{
  "spec_version": 1,
  "flight": 1,
  "device": {
    "id": "oapogee-000000000000",
    "tier": "track",
    "path": "board",
    "hw_rev": null,
    "fw_version": "0.1.0",
    "fw_git": "0000000"
  },
  "session": {
    "armed_utc": null,
    "armed_uptime_ms": 128394,
    "simulated": false
  },
  "calibration": {
    "pad_pressure_pa": 101325,
    "pad_pressure_samples": 0,
    "pad_temperature_dc": 0,
    "accel_bias_mg": [0, 0, 0],
    "gyro_bias_cdps": [0, 0, 0]
  },
  "streams": {
    "flight": {
      "file": "flight.bin",
      "record_bytes": 36,
      "endian": "little",
      "nominal_hz": null,
      "fields": [
        { "name": "t_ms",         "offset": 0,  "type": "u32", "scale": 0.001, "unit": "s" },
        { "name": "pressure_pa",  "offset": 4,  "type": "i32", "scale": 1,     "unit": "Pa" },
        { "name": "temp_dc",      "offset": 8,  "type": "i16", "scale": 0.1,   "unit": "degC" },
        { "name": "accel_mg",     "offset": 10, "type": "i16", "count": 3, "scale": 0.001, "unit": "g" },
        { "name": "gyro_cdps",    "offset": 16, "type": "i16", "count": 3, "scale": 0.01,  "unit": "deg/s" },
        { "name": "hg_accel_dg",  "offset": 22, "type": "i16", "count": 3, "scale": 0.1,   "unit": "g" },
        { "name": "alt_cm",       "offset": 28, "type": "i32", "scale": 0.01,  "unit": "m" },
        { "name": "vel_dm_s",     "offset": 32, "type": "i16", "scale": 0.1,   "unit": "m/s" },
        { "name": "state",        "offset": 34, "type": "u8",  "scale": 1,     "unit": "enum" },
        { "name": "flags",        "offset": 35, "type": "u8",  "scale": 1,     "unit": "bits" }
      ]
    },
    "gnss": {
      "file": "gnss.bin",
      "record_bytes": 16,
      "endian": "little",
      "nominal_hz": null,
      "fields": [
        { "name": "t_ms",   "offset": 0,  "type": "u32", "scale": 0.001, "unit": "s" },
        { "name": "lat_e7", "offset": 4,  "type": "i32", "scale": 1e-7,  "unit": "deg" },
        { "name": "lon_e7", "offset": 8,  "type": "i32", "scale": 1e-7,  "unit": "deg" },
        { "name": "alt_m",  "offset": 12, "type": "i16", "scale": 1,     "unit": "m" },
        { "name": "sats",   "offset": 14, "type": "u8",  "scale": 1,     "unit": "count" },
        { "name": "fix",    "offset": 15, "type": "u8",  "scale": 1,     "unit": "enum" }
      ]
    }
  },
  "summary": {
    "apogee_m": null,
    "t_apogee_ms": null,
    "max_accel_g": null,
    "max_velocity_m_s": null,
    "flight_duration_ms": null,
    "landed": false
  }
}
```

**Every value in the example above is illustrative**, including the ones that
look like measurements. `armed_uptime_ms` is there to show the field's type and
magnitude, not to report anything: no payload has run, so no uptime has been
recorded. The example is a shape, not data.

Two values in it are not free to vary, and a writer must emit them as shown: the
`summary` members really are `null` and `landed` really is `false` in the
manifest written at flight-directory creation.

### Fields that are not obvious

`device.id` is derived from the microcontroller's unique identifier. It is stable
across reflashes, which is what makes a flight archive able to group flights by
airframe.

`session.armed_utc` is null on Solo and Link, and set on Track once a GNSS fix
provides UTC. There is no real-time clock on this board, which is a deliberate
omission: a coin cell and a crystal cost mass and money to provide a timestamp
that a laptop supplies for free at offload. Every time in the log is relative to
arming, and absolute time is attached afterwards where it is available.

`session.simulated` is the same idea as the `SIM` flag in the packet spec, and
it is here for the same reason. A bench run produces a complete, plausible log.
Without this field a flight archive will eventually publish one as a flight.

`calibration.pad_pressure_pa` is the zero reference the entire altitude column is
measured against, and publishing it is what makes the log re-derivable. If the
reference was taken badly, an analyst can recompute altitude from
`pressure_pa` rather than discarding the flight.

`calibration.pad_pressure_samples` is how many samples went into that average.
A reference taken from a short settling window is less trustworthy than one taken
over a long pad wait, and the reader deserves to know which they have.

`streams.*.nominal_hz` is the configured rate, not the achieved one. Actual
sample intervals are in the records, and they are what should be used. A log rate
that varies by flight phase makes the nominal figure a hint, not a fact.

`streams` lists exactly the files present in the flight directory, and nothing
else. A build with no GNSS receiver writes no `gnss.bin` and carries no `gnss`
key. The manifest above is a Track log; a Solo or Link log has `flight` only.
This is the one place structure varies by build variant. Within a stream the
record layout never does, which is why `hg_accel_dg` keeps its six bytes on a
build with no high-g part. A reader discovers streams by enumerating `streams`,
never by assuming a name.

## flight.bin record, 36 bytes

| Offset | Size | Type | Field | Unit | Notes |
|---|---|---|---|---|---|
| 0 | 4 | u32 | `t_ms` | ms | Since arming |
| 4 | 4 | i32 | `pressure_pa` | Pa | Raw barometer reading |
| 8 | 2 | i16 | `temp_dc` | 0.1 degC | Barometer die temperature |
| 10 | 6 | i16 x3 | `accel_mg` | mg | IMU accelerometer, X Y Z |
| 16 | 6 | i16 x3 | `gyro_cdps` | 0.01 deg/s | IMU gyroscope, X Y Z |
| 22 | 6 | i16 x3 | `hg_accel_dg` | 0.1 g | High-g accelerometer, X Y Z |
| 28 | 4 | i32 | `alt_cm` | cm | Fused altitude above pad |
| 32 | 2 | i16 | `vel_dm_s` | 0.1 m/s | Fused vertical velocity |
| 34 | 1 | u8 | `state` | enum | Flight state |
| 35 | 1 | u8 | `flags` | bits | Same bit assignments as the packet spec |

**Raw and derived are both stored, deliberately.** `pressure_pa` and the raw IMU
axes are what the sensors said. `alt_cm` and `vel_dm_s` are what the fusion
concluded. Storing only the derived values would make the log unauditable: a
bug in the filter would be invisible, and nobody could reprocess an old flight
with a better filter. Storing only raw would mean the log disagrees with what
the payload transmitted and what it used to detect apogee. Twelve of the
thirty-six bytes buy the ability to check the payload's own work, and that is
the best-value spend in this format.

`hg_accel_dg` is all zeros when no high-g part is fitted, and the `HIGH_G` flag
is clear. It occupies its six bytes regardless, because a record layout that
changes width by build variant is not a fixed-width record.

`gyro_cdps` in hundredths of a degree per second covers plus or minus 327.67
deg/s, which is under one revolution per second. It is the only field in this
record whose container is narrower than the quantity it carries: `accel_mg`
covers plus or minus 32.767 g, and `hg_accel_dg` covers plus or minus 3276.7 g.
A rocket rolling faster than 327.67 deg/s clips this channel, and a clipped gyro
axis reads as a flat plateau for exactly the reason a saturated accelerometer
does.

TODO(verify): state the gyro full-scale range oApogee configures, which is the
same open question recorded against the `imu` part in `data/bom.yaml`, and
compare it against the 327.67 deg/s this encoding can hold.

TODO(confirm-on-hardware): record the peak roll rate on a flight. If either that
figure or the configured range exceeds 327.67 deg/s, widen this field before
anything depends on `spec_version` 1, because `flight.bin` is a fixed-width
record and changing it afterwards is a format break.

The state enumeration is the one in `data/flight-phases.yaml` and the packet
spec: 0 `PAD_IDLE` through 6 `LANDED`.

At 36 bytes per record, a stream running at 100 Hz produces 3.6 kB per second.
A one-minute flight logged entirely at that rate is roughly 216 kB.

TODO(verify): the actual per-phase log rates have not been chosen. Once they
are, publish the resulting file size for a representative flight and use it to
size the flash part, rather than defaulting to 16 MB because it is the reflexive
choice.

## gnss.bin record, 16 bytes

| Offset | Size | Type | Field | Unit |
|---|---|---|---|---|
| 0 | 4 | u32 | `t_ms` | ms since arming |
| 4 | 4 | i32 | `lat_e7` | degrees x 10^7 |
| 8 | 4 | i32 | `lon_e7` | degrees x 10^7 |
| 12 | 2 | i16 | `alt_m` | 1 m, GNSS altitude |
| 14 | 1 | u8 | `sats` | satellites used |
| 15 | 1 | u8 | `fix` | fix type |

`alt_m` is the receiver's own altitude solution and is **not** the same
measurement as `alt_cm` in `flight.bin`. GNSS altitude is referenced to an
ellipsoid, not to the launch pad, and it is considerably less precise than the
barometer for this application. It is stored because disagreement between the
two is diagnostically useful, not because it is a better altitude. Anyone
plotting a flight should use the barometric column.

Whole metres are deliberate. This is an absolute altitude, so unlike `alt_cm`
the field has to hold the site elevation as well as everything the airframe
adds. An i16 of metres covers minus 32768 to 32767 m, which is far outside the
envelope of any airframe this payload is built for. Tenths of a metre in the
same two bytes would cover only plus or minus 3276.7 m, which a mountain site
plus a large flight can reach, and the extra digit would be false precision in
any case, since GNSS vertical accuracy is much coarser than a decimetre. A
solution outside the representable range is clamped to the endpoint rather than
allowed to wrap, because a wrapped value decodes as a plausible altitude with
the wrong sign and nothing marks it as wrong.

Fix types follow the u-blox convention: 0 no fix, 2 two-dimensional, 3
three-dimensional. Other values are passed through unchanged.

A record is only appended when the receiver reports a fix. Gaps in `t_ms` are
real and mean the receiver had no solution, which happens under boost and is
worth being able to see.

## Truncation and partial files

A file may end mid-record. This is normal: it is what a payload that lost power
on impact leaves behind, and that flight's data is still worth having.

A reader **must** treat a trailing partial record as absent, use
`floor(file_size / record_bytes)` records, and not error out. A reader **should**
report that truncation occurred, because a log that ends abruptly at apogee is
telling you something about the flight.

## Export

The payload does not convert anything. Export is a host-side operation over USB,
producing:

- **CSV**, one row per record, columns named from the manifest with units in the
  header, scaling applied. This is what opens in a spreadsheet.
- **JSON**, the manifest plus decoded records, for programmatic use.

Both are lossless with respect to the binary, because scaling factors are exact
powers of ten and the integers are exactly representable.

The binary is the archival copy. CSV and JSON are conveniences derived from it,
and the Flight Log submission format takes the binary plus its manifest, not the
CSV, for exactly that reason.

## Reference reader

Reads any version of this format, because it reads the manifest rather than
this document:

```python
import json
import numpy as np

TYPES = {"u8": "u1", "i8": "i1", "u16": "u2", "i16": "i2", "u32": "u4", "i32": "i4"}


def load(flight_dir, stream="flight"):
    with open(f"{flight_dir}/meta.json") as f:
        meta = json.load(f)

    # A stream is absent when the build did not write it, so name what is here
    # rather than failing on a bare key lookup a caller cannot interpret.
    spec = meta["streams"].get(stream)
    if spec is None:
        raise KeyError(
            f"no {stream!r} stream in this log; present: {sorted(meta['streams'])}"
        )

    order = "<" if spec["endian"] == "little" else ">"

    names, formats, offsets = [], [], []
    for field in spec["fields"]:
        count = field.get("count", 1)
        base = order + TYPES[field["type"]]
        names.append(field["name"])
        formats.append((base, count) if count > 1 else base)
        offsets.append(field["offset"])

    dtype = np.dtype(
        {"names": names, "formats": formats, "offsets": offsets, "itemsize": spec["record_bytes"]}
    )

    # A trailing partial record is normal: it is what a payload that lost power
    # on impact leaves behind. Truncate to whole records rather than failing.
    raw = np.fromfile(f"{flight_dir}/{spec['file']}", dtype=np.uint8)
    n = len(raw) // spec["record_bytes"]
    truncated = len(raw) % spec["record_bytes"] != 0
    records = raw[: n * spec["record_bytes"]].view(dtype)

    scaled = {
        f["name"]: records[f["name"]] * f["scale"] if f["scale"] != 1 else records[f["name"]]
        for f in spec["fields"]
    }
    return meta, scaled, truncated
```

## Conformance

A conforming reader:

- **must** read the layout from `meta.json` rather than assuming this document
- **must** discover streams by enumerating `streams` rather than assuming a
  stream name is present
- **must** truncate a trailing partial record rather than failing
- **must** treat a null summary member, or `landed` false, as not known rather
  than as a measured zero
- **must not** treat `session.simulated` as decorative
- **should** report truncation
- **should** prefer `alt_cm` over `alt_m` when plotting altitude, and should say
  which it used

A conforming writer:

- **must** write `meta.json` before the first record
- **must** write the `summary` object in the manifest it writes before the first
  record, with every member `null` and `landed` `false`
- **must not** list a stream in `streams` that it did not write a file for
- **must** write the calibration reference actually used
- **must** set `simulated` on any run not produced by a real flight
- **should** rewrite `meta.json` with the summary at `LANDED`

## Open questions

Decided: block checksums go in the manifest, which every flight already writes,
rather than interleaved into `flight.bin` or into a third file. Interleaving
would break the flat-array property that makes this format one line to load,
which is most of why it is shaped this way, and a sidecar would add a file when
a suitable one already exists. A reader that does not care about integrity
ignores the manifest exactly as it does today.

TODO(confirm-on-hardware): implement it in the log writer and state the block
size, which should match the flash page size of the part actually fitted rather
than be picked now.

TODO(verify): confirm that LittleFS on the chosen flash part actually survives
power loss mid-write in practice, by testing it: write continuously and cut
power repeatedly, then check that every completed record is intact. This is the
assumption the whole storage design rests on and it should be demonstrated
rather than trusted.

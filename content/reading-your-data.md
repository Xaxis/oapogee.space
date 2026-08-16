---
title: Reading your data
description: What comes off the payload, how to open it, and what every feature of a flight graph means.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-08-11
status: draft
---

# Reading your data

You plugged the payload in and got a file. This page is what it means.

## Getting it off

Connect the payload by USB. It appears as a serial device, and the offload tool
pulls each flight directory across.

Each flight arrives as three things:

- `meta.json`, the manifest: which board, which firmware, the pressure reference
  it used, the record layout, and a summary of the flight.
- `flight.bin`, the sensor records.
- `gnss.bin`, the position fixes, on Track builds.

The offload tool also writes CSV and JSON, because most people want a
spreadsheet rather than a binary file. Both are derived from the binary and
neither loses anything: the scaling factors are exact and the integers are
exactly representable.

**The binary is the archival copy.** If you keep one thing, keep the directory
with the manifest in it. The CSV is a convenience and can always be regenerated;
the binary plus its manifest is the flight.

TODO(confirm-on-hardware): the offload tool does not exist. Document the actual
invocation once it does, for all three operating systems.

## Opening it

**In a spreadsheet.** Open the CSV. Columns are named with units in the header.
Plot altitude against time and you have your flight.

**In Python.** The [log format specification](/reference/log-format) carries a
reference reader, about twenty lines, that reads the manifest and returns arrays.
It works on any version of the format because it reads the layout rather than
assuming it.

**In anything else.** The format is fully specified and deliberately simple:
fixed-width little-endian records described by a manifest. Writing a reader in
another language is an afternoon at most.

## What the graph shows

TODO(verify): this section needs a real flight. It is written as the structure
of the walkthrough, with each feature described in terms of what causes it, and
it must be replaced by an annotated plot of an actual oApogee flight with each
feature marked on the real curve. A synthetic illustration would defeat the
purpose of the page.

Plot altitude against time and you get a shape that is the same on every flight,
with the details differing.

### The launch, and the part before it

The trace starts before the motor lights. That is the pre-arm ring buffer: the
firmware keeps the most recent samples continuously from the moment you arm, and
writes them into the log when it detects launch. Launch detection is inherently
late, so without the buffer every log would start a fraction of a second after
ignition and the moment itself would be missing from all of them.

### Boost

Altitude rises steeply and the acceleration trace is at its highest of the whole
flight.

**If the acceleration is a flat plateau here, look at what value it is flat at.**
A plateau sitting at a constant value means the sensor saturated: it reached its
full-scale range and reported its maximum rather than an error, so real
acceleration above that limit looks like a constant. The `HIGH_G` flag is set
when the dedicated high-g accelerometer is present and healthy, which is also
when the acceleration field carries its reading. Clear means the number came
from the IMU and is subject to saturation, and it does not distinguish a board
built without the part from one whose part has failed.

TODO(verify): state the full-scale range oApogee configures on the IMU, quoted
from its datasheet, and the motor class at which a representative loaded
airframe exceeds it, from simulation. Both are open questions on the `high_g`
part in the bill of materials, and naming a threshold here before they close
would be exactly the guess that note exists to prevent.

Barometric altitude is least trustworthy during boost, because airflow over the
static ports disturbs the pressure the sensor sees. This is why the fusion leans
on integrated acceleration through this phase, and why both the raw pressure and
the fused altitude are in the log.

### Burnout

A sharp kink. Acceleration crosses from strongly positive to negative as thrust
ends and drag and gravity take over. After apogee it is the most visually
obvious feature of a flight graph, and it is the easiest way to check the motor
did what it said on the label.

### Coast

The unpowered climb. The slope of the altitude curve flattens steadily as the
rocket slows. Most of the altitude on a well flown low power rocket is gained
here, which surprises people who expect the motor to be doing the work.

Airflow settles during coast and the barometer becomes the reliable sensor
again.

### Apogee

The rounded top. Vertical velocity passes through zero.

This is the number the whole payload exists to produce, marked in the log with
its own state and timestamped. The recorded apogee time is slightly earlier than
the moment it was detected, because detection waits for confirmation across
several descending samples. [Apogee detection](/firmware#apogee-detection)
explains why.

### Deployment

A jolt. The ejection charge fires, the nose cone leaves, and the parachute
opens, all of which the accelerometer sees clearly.

**oApogee had nothing to do with this.** The charge is fired by the motor's
delay grain, exactly as it would be with no payload fitted. What the payload
gives you is the ability to see how close the deployment landed to apogee, which
is a measure of how well the delay was chosen. A jolt well after apogee means a
long delay; before it means a short one.

### Descent

A near-constant downward slope, which is what a parachute is for. The rate tells
you whether your parachute is the right size: too fast and the rocket is taking
a hard landing, too slow and it drifts further before it comes down.

The log rate drops here, deliberately. Descent contains far less information per
second than boost did, and the battery saved is battery the recovery beacon
needs afterwards.

### Landing

A flat line at the ground, then quiet.

**The flat line is not always at zero, and a small negative value is not a
fault.** Altitude is measured against a pressure reference taken on the pad. If
atmospheric pressure rose during the flight, or the rocket landed lower than the
pad, the reading goes negative and it is correct. oApogee does not clamp it,
because clamping would hide a real measurement.

## The other columns

**Raw pressure** is what the barometer actually reported, before any processing.
It is stored alongside the fused altitude so the payload's own work is
auditable: if the altitude looks wrong, you can recompute it yourself from
pressure and the reference in the manifest, and find out whether the problem was
the sensor, the reference, or the filter.

**The IMU axes** give orientation through the flight. A rocket that corkscrewed,
weathercocked hard, or came off the rail crooked shows it here.

**State** is the flight phase for each record, which is what makes it easy to
select just the boost, or just the descent, without eyeballing the graph.

**Flags** carry the fault bits and the `SIM` bit. Check `SIM` before you treat
any file as a flight: a bench run produces a complete and entirely plausible
log, and the flag is the only thing that distinguishes it.

## Checking the payload's work

The single most useful thing you can do with an oApogee log, and the reason both
raw and derived values are stored:

1. Take `pressure_pa` from the log.
2. Take `pad_pressure_pa` from the manifest.
3. Compute altitude yourself with the barometric formula.
4. Compare it against the `alt_cm` column.

If they agree, the sensor and the reference are consistent and the flight really
was what it says. If they disagree, you have learned something about the fusion,
and this project would like to hear about it.

## A worked flight

TODO(verify): a complete walkthrough of one real oApogee flight, with the actual
data file published alongside so a reader can follow along in their own tools.
Every feature above marked on the real curve. This page is not finished until
that exists, and it will not be faked in the meantime.

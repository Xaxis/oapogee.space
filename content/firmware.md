---
title: Firmware and flashing
description: Drag and drop a file onto a USB drive. Then the source build, the configuration reference, and the calibration procedure.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-08-11
status: draft
---

# Firmware and flashing

The fast path first. You do not need a toolchain, a compiler, or a programmer to
get a working payload.

## Flashing a release

1. Download the current release `.uf2` file for your tier.
2. Hold the boot button on the board.
3. While holding it, connect USB.
4. Release the button. The board appears as a USB drive.
5. Copy the `.uf2` file onto that drive.
6. The board reboots on its own. The drive disappearing is what success looks
   like.

> **Checkpoint.** The status LED indicates the firmware is running, and
> connecting a serial terminal shows the boot banner with the firmware version.
>
> No drive appears: [it powers on but does not appear as a USB drive](/troubleshooting#not-a-drive).
> Try a different cable first. Charge-only USB-C cables are common and look
> identical to data cables.

TODO(confirm-on-hardware): describe the actual LED indication and the boot banner
text once the firmware exists.

This is the whole reason oApogee uses the RP2040 and RP2350 family. Drag and drop
flashing removes the most common place a first-time builder gives up.

### Releases

TODO(confirm-on-hardware): no firmware has been written and no release exists.
When one does, publish per-tier builds, a changelog entry for every release, and
the git commit each build came from, so a flight log can record exactly what was
flying.

## Building from source

For anyone who wants to change something. Nothing about the standard build
requires this.

TODO(confirm-on-hardware): document the toolchain, the checkout, the build
command, and the output path, once the firmware repository exists. Include the
exact toolchain versions the published releases are built with, because "it
builds on my machine" is not a build instruction.

The firmware is published in full, including the calibration constants. A
project named for openness that ships a black box loses the audience it was
built for.

## Configuration

Configuration lives in a plain text file on the board's filesystem, editable
over USB. There is no configuration app to install and nothing that has to run
on a particular operating system.

TODO(confirm-on-hardware): publish the full configuration reference once the
firmware defines it. It must cover, at minimum:

- radio band, spreading factor, bandwidth, coding rate, and transmit power
- log rates per flight phase
- launch detect threshold and confirmation sample count
- apogee confirmation sample count
- landing detect criteria
- buzzer beacon pattern and interval
- callsign, for anyone operating under an amateur licence
- the simulated flag, for bench runs

Every one of those has a default, and every default is a decision that should be
documented with its reasoning rather than presented as a number.

## Calibration

Two calibrations, one of which happens on every flight.

### Pad calibration, every flight

This is the one that matters and it is automatic. When you arm the payload, it
averages barometric pressure over a settling window to establish the zero
reference that every altitude in the flight is measured against.

You do not do anything except arm it and then leave it alone. Bumping the rocket
during the window corrupts the reference, which is why the preflight checklist
puts arming after everything else.

The reference pressure and the number of samples that went into it are both
written into the flight's manifest, so an altitude that looks wrong can be
recomputed from raw pressure afterwards.

TODO(verify): state the settling window duration, and state the altitude error
you get if you arm and walk away immediately.

**Why this exists at all.** A barometric altimeter measures pressure, and
pressure varies with weather as well as with height. Without a local reference
taken now, it would report height above sea level under standard atmosphere
conditions, which is not the number anyone wants and is wrong by tens of metres
on any day with weather. Taking the reference on the pad makes the number mean
"height above where I am standing."

The cost: the reference is taken once. A front moving through during a long pad
wait shifts the actual pressure, and the altitude drifts with it. This is why a
flight that sat on the pad for twenty minutes may land reporting a small
negative altitude, and why that is not a fault.

### Sensor calibration, once per board

Accelerometer and gyroscope bias, measured once and stored on the board.

TODO(confirm-on-hardware): document the procedure once it exists. It will involve
holding the board still in several orientations, and it needs to state what
"still" means and how the procedure tells you it has enough data.

## The flight state machine

The firmware is a state machine, and the states are the same enumeration used in
the [log format](/reference/log-format) and the [packet
format](/reference/telemetry-packet).

`PAD_IDLE` to `ARMED` to `BOOST` to `COAST` to `APOGEE` to `DESCENT` to `LANDED`.

The homepage animates it. The behaviour and transition criteria for each state
live in `data/flight-phases.yaml` and are summarised below.

### Pre-arm ring buffer

From `ARMED`, the firmware continuously keeps the most recent samples in a ring
buffer that is written into the log when launch is detected.

This exists because launch detection is inherently late. It cannot fire until
enough acceleration has accumulated to be distinguishable from somebody knocking
the launch rail, and by then the interesting first fraction of a second is
already past. The ring buffer recovers it. Without one, every log starts shortly
after ignition and the moment of ignition is missing from all of them.

### Sensor fusion

Barometric altitude and integrated vertical acceleration are combined, because
each covers for the other's failure.

The barometer is noisy, and it is actively wrong during the pressure disturbance
around burnout and, on faster flights, through transonic effects. The
accelerometer does not care about air pressure at all, but integrating any
sensor's bias accumulates it, so an accelerometer-only altitude drifts steadily
and without limit.

Fused, the barometer anchors the long term and the accelerometer carries the
short term. Neither alone would be adequate, and saying which one the firmware
trusted at each instant is why both the raw and the fused values are stored in
the log.

TODO(confirm): decide between a complementary filter and a Kalman filter, and
document the choice with its reasoning. A complementary filter is far simpler to
implement, to explain, and to verify by hand, which on a project whose product is
its documentation is a real argument. A Kalman filter is better behaved if the
noise characteristics are known, and they are not yet, because nothing has flown.

### Apogee detection

Apogee is declared when altitude has decreased across several consecutive
samples, not on a single reading.

A single-sample rule would fire early on any flight, because barometric noise
produces a descending pair of samples somewhere near the top of every flight
regardless of what the rocket is doing. Requiring confirmation costs detection
lag, and that lag is a real error in the recorded apogee time, which is why the
packet format carries the estimated event time separately from the time the
packet was sent.

TODO(verify): choose the confirmation count from measured barometer noise, and
publish the resulting lag in milliseconds.

### Adaptive log rate

High through boost and coast, lower on descent, minimal after landing.

Boost and coast contain almost all the information in a flight and last a few
seconds. Descent under a parachute contains very little per second and lasts far
longer. Logging both at the same rate wastes flash and, more importantly, wastes
battery that the recovery beacon needs afterwards.

### Landing and the beacon

On `LANDED`, the buzzer switches to a recovery beacon pattern and, on Link and
Track, the radio stretches its beacon interval to trade update rate for
endurance.

TODO(verify): measure the beacon endurance from landing detect to cell cutoff,
per tier, on a fully charged cell after a full flight. That is the number that
decides whether a payload is still findable the next morning.

## GNSS configuration, on Track

u-blox receivers ship configured with a dynamic platform model that assumes
ground vehicle behaviour. Under rocket acceleration the receiver rejects its own
position solutions and loses lock, at exactly the moment tracking is worth
having.

**The airborne dynamic model must be set explicitly**, at every boot or saved to
the receiver's configuration. This is the single most common reason a
GNSS-equipped model rocket payload comes back with no track, and it is a
configuration problem rather than a hardware one.

Also relevant, though not a practical constraint at the altitudes oApogee
targets: GNSS receivers carry export-control limits on simultaneous high altitude
and high velocity. They are far outside anything a model rocket will reach, and
they will matter to anyone scaling this design up.

TODO(verify): confirm the specific configuration messages required, and confirm
by flight that lock is retained through boost once they are applied.

---
title: Start here
description: What oApogee is, which tier to build, what skills and tools you need, and what it will not do. Read this before buying anything.
tier: all
difficulty: beginner
time_estimate: 10 minutes to read
updated: 2026-09-07
status: draft
---

# Start here

oApogee is a small sensor package that rides in or on a model rocket and records
what happened: how high it went, how hard it accelerated, which way it was
pointing, and when each of those changed. On the larger builds it sends that
down to a receiver on the ground while the rocket is still flying, and reports
its position so you can find it.

You build it yourself. Everything you need is on this site.

## What it will not do

**oApogee is a passive instrumentation payload. It does not fire ejection
charges, control deployment, ignite motors, or command any pyrotechnic device.**

This is a design boundary and it will not change. Your recovery system is the
motor's ejection charge and delay grain, exactly as it was before you fitted a
payload. oApogee watches, records, and reports. It never acts on the rocket.

If electronic deployment is what you want, that belongs to high power rocketry
and to the NAR and Tripoli certification paths. oApogee is not that and will not
become that.

## Where the project is

**Nothing physical exists yet.** No board has been fabricated, assembled,
weighed, priced, or flown, which is why no cost, mass, range or battery life
figure appears anywhere on this site.

What does exist is the design, the parts list, the firmware, the wire formats,
and the safety and regulatory groundwork. That is enough to build the Modules
path from today, and enough to tell us where the design is wrong. It is not
enough to build the custom board from: that is laid out and routed, but its
fabrication package is held back while the board has an open blocker, and it
has one. The [schematic page](/reference/schematic) lists what is open.

## Which tier

Every tier is meant to be the same board and the same firmware, so moving up
means populating footprints that were already there rather than building a new
payload.

- **Solo.** Barometer, IMU, onboard logging, no radio. The cheapest build, and
  the only one with no radio regulations to read first. Its limitation is
  recovery: Solo has a buzzer and nothing else.
- **Link.** Adds a LoRa downlink, so you watch the altitude climb in real time
  and keep a second copy of the flight even if the rocket is never found. It
  needs a [ground station](/ground-station), which is a second build with its
  own parts list, and it puts a transmitter in your rocket, which means reading
  the [radio section](/safety#radio) first.
- **Track.** Adds a GNSS receiver. The valuable output is not the flight track,
  it is the last position fix sent down before the signal is lost, which turns
  a search into a walk. Track is the full build, and cost and mass are the only
  reasons not to choose it.

The [bill of materials](/bom) shows what each tier actually costs you in parts.

## Which build path

Independently of the tier, there are two ways to build the electronics:
off-the-shelf **Modules** wired to a carrier, or the custom **Board**.

Start with Modules. It is the one you can build today, it needs no PCB
fabrication or reflow, and everything you learn transfers. It is heavier and
bulkier, and every inter-board wire is a joint that can fail under boost shock.
The [bill of materials](/bom) gives you a parts list for either.

## What you need to be able to do

Honestly, not much, but not nothing.

**You need to have soldered before.** Not well. This is not the project to learn
on, but it is a reasonable second or third soldering project. If you have
assembled a kit with through-hole parts, you can do the Modules path.

**You do not need to write firmware.** The microcontroller is flashed by
dragging a file onto what appears as a USB drive. There is no toolchain to
install and no compiler to fight. If you want to change the firmware you can,
and the source is published, but nothing about the standard build requires it.

**You need to be able to use a multimeter for continuity and voltage.** The
build guide has a continuity check before first power-on, and skipping it is how
a reversed battery connector becomes a dead board.

**You need access to a 3D printer, or a print service.** The mounting hardware
in both form factors is printed. There is no non-printed option.

**You need to be willing to re-simulate your rocket.** Adding a payload moves
the centre of gravity and changes how the rocket flies. This is not optional and
it is the one step on this whole site with a safety consequence for people
standing nearby.

## What it costs, weighs, and takes

TODO(verify): per tier and per build path, the price of a real cart from a US
distributor at quantity one, the measured mass of an assembled unit including
cell, and the median and slowest build times from people who have soldered
before. The brief's targets are under $60, under 25 g and an evening, and a
target is not a measurement.

## What to expect from a first flight

You will get a file. Opened in a spreadsheet or plotted, it shows the altitude
rising steeply while the motor burns, a sharp kink at burnout, a slower climb
through coast, a rounded top at apogee, a jolt when the ejection charge fires and
the parachute opens, and a slow descent to a flat line at the ground.

That curve is the point of the whole project. [Reading your data](/data) walks
through every feature of it.

You will probably also get something wrong on the first attempt. Three failures
account for most of what this project has seen or been told about: a static port
blocked with glue or paint, a payload that was powered but not armed, and a
rocket that was not re-simulated with the payload fitted. They are on the
[preflight checklist](/preflight) for that reason.

TODO(verify): no ranking is given because there is no data behind one. Put an
ordering here once the community flight log has enough entries to count what
actually goes wrong, rather than what is easiest to imagine going wrong.

## The order to do things in

1. Read [Safety and rules](/safety). Not skim. It covers the lithium cell, the
   radio regulations, and the stability check, and each of those has a
   consequence beyond a lost flight.
2. Pick a tier and a build path.
3. Order from the [bill of materials](/bom).
4. Follow the [build guide](/build).
5. Flash the firmware and calibrate. See [Firmware and flashing](/firmware).
6. Print and fit the [mounting hardware](/mounting), including the static ports.
7. Do the ground tests on the safety page. All four of them, on the bench,
   before it goes near a rocket.
8. Re-simulate your rocket with the payload fitted.
9. Work through the [preflight checklist](/preflight) on the day.

## If something goes wrong

[Troubleshooting](/troubleshooting) is organised by what you saw, not by which
component failed, because when you have a problem you know the symptom and not
the cause. If your problem is not there, or the answer is wrong, telling us is
the most useful thing you can do for this project right now.

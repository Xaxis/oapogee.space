---
title: Start here
description: What oApogee is, which tier to build, what skills and tools you need, and what it will not do. Read this before buying anything.
tier: all
difficulty: beginner
time_estimate: 10 minutes to read
updated: 2026-08-11
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

## Before you read further

**This is a design on paper.** Nothing has been fabricated, assembled, weighed,
priced, or flown. There are no cost figures, no mass figures, no range figures
and no battery life figures anywhere on this site, because none of them have
been measured. The [status page](/status) lists exactly what is verified and
what is not.

What exists today is the design, the parts list, the wire formats, and the
safety and regulatory groundwork. That is genuinely useful if you want to
understand the design or tell us where it is wrong. It is not yet enough to
build from with confidence.

## Which tier

Every tier is the same board and the same firmware. Moving up means populating
footprints that were already there, not building a new payload.

### oApogee Solo

Barometer, IMU, onboard logging. No radio.

Build this if you want a flight profile and you are happy to read it after you
get the rocket back. It is the cheapest tier, the simplest build, and the only
one with no radio regulations to understand before your first flight.

Its limitation is recovery. Solo has a buzzer and nothing else, so if the rocket
drifts a long way you are searching by ear.

### oApogee Link

Adds a LoRa downlink and a ground station.

Build this if you want to watch the altitude climb in real time, or if you fly
somewhere the payload might not come back. The downlink is a second copy of the
data: even if you never find the rocket, you have the flight.

Two things to know before choosing it. It needs a ground station, which is a
second build with its own parts list, so budget for both. And it puts a
transmitter in your rocket, which means reading the [radio
section](/safety#radio) before you fly.

### oApogee Track

Adds a GNSS receiver.

Build this if you fly big fields, windy days, or anything with a long descent.
The valuable output is not the flight track, it is the last position fix sent
down before the signal is lost, which turns a search into a walk.

Track is the full build. There is no reason not to choose it apart from cost and
mass.

## Which build path

Independently of the tier, there are two ways to build the electronics.

**Modules.** Off-the-shelf breakout boards wired to a carrier or protoboard. You
can build this today with no PCB fabrication, no reflow and no stencil. It is
heavier and bulkier than the board, and every inter-board wire is a joint that
can fail under boost shock.

**Board.** The custom oApogee PCB. One part number, a smaller and lighter
payload, and nothing to build until boards are fabricated, which has not
happened yet.

Start with Modules. It exists so that the project is useful before the board
does, and everything you learn transfers.

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

TODO(verify): fill this section in with measured figures. It should carry, per
tier and per build path, the actual price of a real cart from a US distributor
at quantity one, the measured mass of an assembled unit including cell, and the
median and slowest build times from timing several people who have soldered
before. The targets in the project brief are under $60 for the full build, under
25 g flying mass, and an evening of work, but a target is not a measurement and
this site does not publish targets as though they were.

The [bill of materials](/bom) lists every part with its price column empty for
the same reason.

## What to expect from a first flight

You will get a file. Opened in a spreadsheet or plotted, it shows the altitude
rising steeply while the motor burns, a sharp kink at burnout, a slower climb
through coast, a rounded top at apogee, a jolt when the ejection charge fires and
the parachute opens, and a slow descent to a flat line at the ground.

That curve is the point of the whole project. [Reading your data](/data) walks
through every feature of it.

You will probably also get something wrong on the first attempt. The most common
first-flight problems, in order: a static port blocked with glue or paint, a
payload that was powered but not armed, and a rocket that was not re-simulated
with the payload fitted. All three are on the [preflight
checklist](/preflight) for that reason.

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
the cause.

If your problem is not there, or the answer is wrong, telling us is the most
useful thing you can do for this project right now.

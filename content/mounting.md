---
title: Mounting
description: The internal sled and the external pod, static port sizing and placement, the stability check you must do, and what payload mass costs you in altitude.
tier: all
difficulty: intermediate
time_estimate: null
updated: 2026-08-11
status: draft
---

# Mounting

This page is where the claim that oApogee attaches to any model rocket either
becomes true or does not. It covers two form factors, the vent holes without
which the barometer is useless, and the stability check that keeps the rocket
flying straight with a payload on it.

## Two form factors

### Form A: the internal sled

A printed sled that fits inside a payload bay, sized for a 24 mm coupler or a
BT-50 tube.

Use this if your rocket already has a payload section. It is the lighter and
lower-drag option, it does not change the outside of the airframe at all, and it
puts the payload on the vehicle centreline where its mass has the least awkward
effect.

Its limitation is that it only works on rockets that have somewhere to put it.

### Form B: the external pod

A streamlined printed fairing that straps or tapes to the outside of the body
tube, in the manner of a rail button pod.

**This is the one that makes the "any rocket" claim true.** It requires no
modification to the airframe, it works on a rocket you already own and like, and
it comes off again afterwards. Nothing gets drilled, cut, or glued to a rocket
you care about.

It also costs you something, and this page is the wrong place to be shy about
it. A pod on the outside adds drag, adds asymmetric drag, and moves both the
centre of gravity and the centre of pressure in ways that depend entirely on
where you put it. On a small light rocket that is not a rounding error. "No
modification to the airframe" is not the same as "no effect on the rocket."

TODO(verify): measure the altitude penalty of the pod against the same rocket
and motor flown without it, several times each, and publish both the mean and
the spread. This is the number that decides whether the pod is worth it and it
should come from flights rather than from simulation alone.

### Printed files

Both form factors are below, as OpenSCAD source and as rendered STLs, with every
dimension they are built from and where each one came from.

The pod is two parts. A single-piece pod has a cavity that opens somewhere and
every choice is bad: upward leaves the payload uncovered, downward onto the tube
leaves nothing holding the board, and neither prints, because the pod is convex
on top and concave underneath so no orientation puts a flat face on the bed.
Split at the top of the cell, both halves have a large flat face, both print it
face down with no supports, and the saddle ends up facing upward where a concave
surface costs nothing.

TODO(confirm-on-hardware): the board outline is a guess and it is the most
important input to both parts. Fix it in the PCB layout, measure a fabricated
board, and update `data/mechanical.yaml`. Until then these print as fit checks.

## Static ports

A barometric sensor inside a sealed tube measures the tube. It produces a smooth,
confident, entirely fictional altitude curve, which is worse than an obvious
failure because nothing about it looks wrong.

Static ports are the holes that let the sensor see outside air. Getting them
right is the difference between a flight profile and a fiction.

### The rules

1. **More than one hole, evenly spaced around the circumference.** A single hole
   makes the reading depend on which way that hole faces relative to the airflow.
   Three or four spaced evenly average out yaw and roll.
2. **Away from discontinuities.** Keep them clear of the nose cone shoulder, fin
   roots, and any step in the airframe. Air near those places is disturbed by the
   airframe, and disturbed air is not ambient.
3. **Beside the sensor, not far from it.** The volume the sensor shares with the
   ports should be small and directly connected.
4. **Deburr them.** A raised lip around a drilled hole trips the airflow over it,
   which is exactly the thing the port exists to avoid. Take the burr off with a
   knife or a larger drill bit turned by hand.
5. **Check them on every flight day.** Paint, glue, tape and swarf all block
   ports, and a blocked port is on the preflight checklist for that reason.

### Sizing

TODO(verify): publish the port diameter and count oApogee recommends, along with
the reasoning that produced them. The rule of thumb in hobby rocketry relates
total vent area to the enclosed volume, and this site should state the specific
rule it is using, cite where it comes from, and show the arithmetic for the
standard sled and pod volumes rather than quoting a number with no derivation.
Then confirm it by flying the same rocket with two different port
configurations and comparing.

Until that is measured, this page will not print a number. A confident wrong
port size produces confident wrong altitudes for everybody who follows it.

## Stability, which is a safety issue

**Re-simulate your rocket in OpenRocket with the payload mass and position
included, before you fly it.**

Not after. Not on the second flight. A rocket that has flown well ten times
tells you nothing about how it flies with a payload in a new place, because the
thing that makes a rocket stable is the relationship between two points that you
have just moved.

A rocket is stable when its centre of pressure sits behind its centre of
gravity. Adding mass moves the centre of gravity, and which way depends entirely
on where the payload goes relative to the existing balance point: forward of it
moves the centre of gravity forward, aft of it moves it backwards. Too far back
and the rocket is unstable, which does not mean it flies badly, it means it
flies sideways.

Too far forward has a cost too. A large stability margin makes the rocket
weathercock hard into the wind, which loses altitude and can send it downrange
over people.

The external pod makes this easier to get wrong than the sled, because the pod
can be strapped anywhere along the tube and the convenient place is not always
the right one.

### What to aim for

TODO(verify): state the caliber margin oApogee recommends, quoted from NAR or
Tripoli guidance with a citation rather than asserted from memory. This is a
safety number and it does not get a rule of thumb from an anonymous website.

### A worked example

TODO(verify): work a complete example. Take a named, commonly available kit,
state its published stability margin unloaded on a specific motor, then show the
same rocket in OpenRocket with an oApogee pod at two different positions, one
sensible and one badly chosen, with the resulting margins and what each means.
Include the OpenRocket file so a reader can open it rather than trusting a
screenshot.

## Mass budget

Every gram you add is altitude you do not get. How much depends on the motor:
the smaller the motor, the more a fixed payload mass costs you as a fraction.

TODO(verify): publish a table of apogee against payload mass for A, B, C, D and
E motors on a representative airframe, generated from OpenRocket and checked
against at least one real flight per motor class. A reader deciding between Solo
and Track deserves to see what the extra mass costs them before they order.

The qualitative shape, which is safe to state without measuring: a payload that
is a small fraction of the total liftoff mass costs little; one that is a
significant fraction costs a lot; and on the smallest motors, a payload can be a
large enough fraction that the flight is not worth doing. That last case is real
and it is why Solo exists.

## Why the hardware is bright yellow

The board soldermask and the printed pod are hi-vis yellow or orange. This is a
recovery decision, not a styling one.

A payload that comes off the rocket, or a rocket that lands in tall grass, is
found by eye. A bright object in a green or brown field is visible from many
times the distance a black or white one is, and the walk to recover it is
shorter and more likely to succeed. Every recovery aid on this board exists
because a payload you cannot find is a payload whose data you do not have.

Print your pod in the brightest filament you own. Do not print it black because
it looks better on the rocket.

## Securing the payload

Two things must not move: the board, and the cell.

The board moving means joints flex under boost, and a flexing joint fails at the
worst moment. The cell moving is worse: a lithium cell that shifts under
acceleration and lands on a solder joint is a short circuit inside a sealed
plastic box.

Route the wires so that closing the enclosure cannot pinch them. Then shake the
closed assembly hard and listen. Anything you can hear moving in your hand moves
far harder under boost, where the acceleration is many times what your arm can
produce.

## Fitting to the rail

Check that the rocket still slides freely on the launch rail with the pod
fitted. An external pod is the most common thing to catch on a rail button or a
launch lug, and discovering it at the pad with a queue behind you is a bad time
to find out.

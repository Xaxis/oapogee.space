---
title: Safety and rules
description: What oApogee will not do, how to handle the battery, how to ground test, which radio rules apply, and where the model rocketry limits sit.
tier: all
difficulty: beginner
time_estimate: null
updated: 2026-08-11
status: draft
---

# Safety and rules

Read this before you buy parts, not after you have built something.

This page covers four separate things that all get called safety: what oApogee
deliberately cannot do, how to handle the hardware without hurting yourself, how
to test before you fly, and which laws and codes apply to a rocket with a
transmitter in it. They are separate and the page keeps them separate.

Nothing here replaces your club's safety officer or your Range Safety Officer.
If a person standing at the range tells you something that contradicts this
page, they win.

## What oApogee does not do

**oApogee is a passive instrumentation payload. It does not fire ejection
charges, control deployment, ignite motors, or command any pyrotechnic device.**

This is a design boundary, not a missing feature. It will not be added later.

The reason is that the two things have completely different failure
consequences. If an instrumentation payload fails, you lose data. If a
deployment controller fails, the rocket comes in ballistic, or the charge fires
on the pad while somebody is standing at it. Building a payload that can only
observe means the worst outcome of any bug in this project is a disappointing
graph.

Electronic deployment control belongs to high power rocketry, where the flying
is done under a certification programme that exists precisely because those
consequences are real. If deployment control is what you need, oApogee is not
that, and the NAR and Tripoli certification paths are where to look instead.

On a model rocket, your recovery system is the motor's ejection charge and delay
grain, exactly as it was before you added a payload. oApogee does not change
that, does not interact with it, and does not know it happened except as a jolt
in the accelerometer trace.

### One consequence of the boundary, stated plainly

oApogee will never be a reason a rocket recovers safely. Choose your motor delay
the same way you would with no payload, and check it in simulation with the
payload mass included. See stability below.

## Lithium polymer batteries

The cell is the most dangerous object in the kit. Not because it is likely to
hurt you, but because it is the only part whose failure mode is fire.

### The rules

1. **Never fly a damaged cell.** If it is puffed, swollen, dented, punctured,
   or has been crushed, it is finished. Not "probably still fine". Finished.
2. **A puncture can ignite minutes later, not instantly.** If you damage a cell,
   do not put it back in a drawer and get on with the build. Take it outside,
   away from anything flammable, and watch it.
3. **Charge on a hard non-flammable surface, in a LiPo-safe bag or a metal tin,
   and stay in the room.** Charging unattended overnight is how the stories
   start.
4. **Never short the terminals.** The connector on these cells is small, the
   wires are thin, and a dropped pair of tweezers across the pads will do it.
   Keep unattached cells in their bag with the connector taped.
5. **Use a cell with a protection circuit.** Bare unprotected cells are sold and
   are cheaper. The protection board is the thing that disconnects the cell on
   over-discharge and short circuit, and this project assumes it is present.
6. **Store at storage charge, not full.** A cell kept at full charge for months
   degrades faster and is more likely to puff. Charge it the day before you fly.
7. **Do not charge a cold cell, and do not charge a hot one.** Let it come to
   room temperature first.
8. **Dispose of it properly.** Household waste is not proper. Most areas have a
   battery recycling drop-off, and hobby shops often take them.

TODO(verify): state the specific storage voltage per cell that oApogee
recommends, sourced from the cell manufacturer's documentation rather than from
hobby folklore, and state how to reach it with the hardware in this project.

### What charging looks like on oApogee

Charging happens over the USB-C connector, with the cell attached. There is no
separate charger to buy and no balance lead, because a single cell has nothing
to balance.

TODO(confirm-on-hardware): describe the LED behaviour during charge, at charge
complete, and on a fault, once the hardware exists and that behaviour is
implemented.

### Flying with it

The cell flies inside the payload, which means it experiences boost acceleration
and a landing impact. Secure it so it cannot move inside the pod, and route the
wires so they cannot be pinched by the enclosure closing. A cell that shifts
under boost and lands on a solder joint is a short circuit inside a sealed
plastic box.

Inspect the cell after every hard landing.

## Ground testing

The point of ground testing is that a payload which is going to fail should fail
on your bench, where the consequence is an evening, rather than at apogee, where
the consequence is the flight.

Do all four. In this order.

### 1. The bench dry run

Power the payload up and take it through every flight state without leaving the
room. Arm it, simulate launch detection, let it run through to landing detect,
and confirm that it produces a log file you can read.

Expected result: a complete log, on the flash, that exports to CSV and opens.

TODO(confirm-on-hardware): document the exact procedure for simulating launch
detection on the bench, including whether it is a firmware test mode or a
physical motion, once the firmware exists.

### 2. The shake test

Hold the assembled payload, closed up in its pod, and shake it hard. Then listen.

Expected result: nothing rattles, nothing buzzes, nothing shifts. Anything you
can hear moving is a component that will move under boost, where the
acceleration is far higher than anything your arm can produce.

Open it up and check the cell is still where you put it.

### 3. The drop test

Drop the closed pod onto a hard floor from waist height, a few times, on
different faces.

Expected result: it still boots, still logs, and the case has not opened. This
is a deliberately crude approximation of landing, and it is crude on purpose:
if the payload cannot survive a clumsy drop it will not survive a windy landing
on hard ground.

TODO(verify): once hardware exists, establish whether the drop test is
representative by instrumenting an actual landing and comparing peak
accelerations. If a real landing is far harsher, say so and change the test.

### 4. The full arm-to-landing rehearsal

Do the complete flight day procedure at your desk. Charge it, fit it to the
rocket, arm it exactly as you would on the pad, listen for the arm confirmation,
wait as long as a real pad wait, and then take it through a flight.

Expected result: you discover the thing you forgot, at home, where it costs
nothing.

The Preflight checklist page has the flight day version of this.

## Stability is a safety issue

Adding mass to a rocket moves its centre of gravity, and a rocket whose centre
of gravity has moved too far back is unstable. An unstable rocket does not fly
badly, it flies sideways, into the ground, or into the flight line.

**Re-simulate in OpenRocket with the payload mass and position included, before
you fly it.** Not after. Not on the second flight. This is the single most
important sentence on this page for anyone attaching a payload to a rocket they
already own and have flown many times, because the rocket flying well ten times
before tells you nothing about how it flies with 25 g in a new place.

Which direction the centre of gravity moves depends entirely on where the
payload goes. Forward of the current balance point it moves the centre of
gravity forward, which increases stability margin and increases weathercocking.
Aft of it, it moves backwards, which reduces margin, and past a point makes the
rocket unstable.

The external pod makes this easier to get wrong than the internal sled, because
it can be strapped anywhere on the tube and the obvious place is not always the
right one.

TODO(verify): a full worked example, with a named common kit, its published
stability margin unloaded, the same rocket with an oApogee pod at two different
positions, and the resulting margins from OpenRocket. Include the caliber margin
to aim for, quoted from NAR or Tripoli guidance rather than asserted.

The Mounting page carries the full treatment, including static ports and the
mass budget effect on apogee by motor class.

## Radio

This section applies to oApogee Link and oApogee Track. If you are building
Solo, there is no transmitter and none of this applies to you. That is one of
the reasons Solo exists.

**Part 15 and Part 97 are two different legal regimes and this page never mixes
them.** Read the one that applies to you.

### The default: unlicensed operation in the US

oApogee Link and Track default to the 902 to 928 MHz band, operating under FCC
Part 15. **No licence is required and no callsign is needed.**

Two obligations come with that. A Part 15 device must not cause harmful
interference to licensed services, and it must accept any interference it
receives, including interference that stops it working. In practice this band is
busy, and that is a real consideration for a telemetry link rather than a
formality.

The less obvious obligation is equipment authorisation. Using a radio module
that already holds FCC modular approval means the certification work has been
done by the module vendor. Designing your own RF section, or modifying an
approved module's antenna arrangement, can move that responsibility to you.
oApogee recommends a pre-certified module in the Modules path specifically to
keep this simple.

- Primary source: [47 CFR Part 15](https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-15)

TODO(verify): transcribe the specific power and antenna limits that apply to
oApogee's configuration in 902 to 928 MHz from the current text of Part 15,
subpart C, and state them here with the section number. Do not paraphrase them
from memory or from a forum post. Separately, verify whether any provision
restricts airborne operation of Part 15 devices in this band, and state the
answer either way, because "nobody mentioned it" is not a finding.

### Outside the US

The band changes and so do the rules. A 902 to 928 MHz module is not legal to
operate in most of Europe.

For the EU and the UK, the equivalent short range device band is 863 to
870 MHz, which requires a different radio module because the matching network
and antenna are tuned per band. It is not a firmware setting.

TODO(verify): document the EU and UK rules properly rather than gesturing at
them. Specifically: which sub-bands within 863 to 870 MHz are usable, the duty
cycle limits that apply to each, the relevant ETSI harmonised standard, the UK
interface requirement document, and, critically, whether airborne use is
permitted, because several national administrations restrict it. Cite the
primary documents. If the answer is that oApogee cannot be flown legally in a
given country, the site says so.

If you are outside the US, the EU, and the UK, oApogee currently has nothing to
tell you about your regulator and will not guess. Check with your national
authority and with your local rocketry club, and please tell us what you find.

### For licensed amateur radio operators

This subsection is for people who already hold an amateur radio licence. If you
do not, skip it. Nothing in it is available to you, and operating under Part 97
without a licence is a different category of problem from operating a Part 15
device incorrectly.

A licence buys considerably more transmit power and access to other bands,
including the 70 cm and 2 m allocations, which opens up APRS as a tracking path
with existing receiving infrastructure. For a long walkaway that is a genuine
advantage over a private LoRa link.

It also brings obligations that a Part 15 user does not have:

- **Station identification.** Your station must identify with your callsign at
  the intervals the rules require. A telemetry beacon that transmits for an
  entire flight and never identifies is not compliant. This has to be built into
  the firmware, not remembered on the day.
- **No encryption.** Part 97 prohibits messages encoded for the purpose of
  obscuring their meaning. A telemetry format may be compact and binary, since
  that is efficiency rather than obfuscation, but it must be documented and
  decodable by anyone. This is one of the reasons the oApogee packet format is
  published as a specification.
- **Control operator responsibility.** The licensee is responsible for the
  transmission, including one coming from a rocket that has left their immediate
  control.

- Primary source: [47 CFR Part 97](https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97)

TODO(verify): state the station identification interval and the specific rule
sections for identification and for the encryption prohibition, transcribed from
the current text. Then, separately, decide and document whether oApogee firmware
will support a Part 97 mode at all in v1, because shipping a half-built
compliance feature is worse than shipping none.

## Rocketry rules

### The safety codes

Most US club flying happens under the NAR Model Rocket Safety Code or the
Tripoli equivalent, and your club will tell you which. Read the actual code
rather than a summary of it, including this one.

- [National Association of Rocketry](https://www.nar.org/)
- [Tripoli Rocketry Association](https://www.tripoli.org/)

The parts of a safety code that a payload most often interacts with are the
limits on total liftoff weight, the requirement that the rocket be stable, and
the rules about launch site dimensions and recovery. Adding a payload touches
all three.

### The federal rules

In the US, amateur rockets are covered by FAA regulations that divide them into
classes. Class 1 is the model rocket class and carries the fewest requirements.
Which class a rocket falls into is determined by things like propellant mass and
total vehicle weight, which means **adding a payload is an action that can move
a rocket toward a class boundary.** On a low power rocket with a payload in this
mass range that is very unlikely to be the operative constraint, but it is worth
understanding which direction you are moving in.

- Primary source: [14 CFR Part 101](https://www.ecfr.gov/current/title-14/chapter-I/subchapter-F/part-101)

TODO(verify): transcribe the Class 1 criteria from the current text of Part 101
Subpart C, with the section number, including the propellant mass limit, the
total weight limit, and the construction requirements. State them as a quotation
with a retrieval date rather than a paraphrase. Then state plainly where the
boundary into Class 2 sits and what that entails, so a reader scaling up knows
what changes.

TODO(verify): confirm whether a transmitter in the payload has any bearing on
airspace notification requirements. The expected answer is no at Class 1, but
expected is not verified.

## Recovery

### Fire

Dry grass and a hot motor casing are a real combination, and a field fire caused
by a rocket flight is the sort of event that ends a club's access to a site
permanently. Know the fire conditions on the day. If the field is dry and the
wind is up, the answer is to not fly, and that answer is available to you.

The payload contributes a lithium cell to this picture. A cell that is damaged
in a hard landing, in dry grass, is a worse outcome than a lost payload.

### Permission

A rocket that lands on someone else's property does not give you permission to
go and get it. Ask. If the answer is no, the answer is no.

**Do not trespass to retrieve a payload.** It is a cheap board and you can build
another one. Standing crops in particular: walking into a field of wheat to find
a small yellow box does real damage to someone's income, and it is why the pod
is a bright colour and why the buzzer exists.

This is not a moral aside. Access to flying sites in this hobby depends almost
entirely on landowners who tolerate rocketry, and that tolerance is spent by
individuals one field at a time.

### Livestock, roads, and people

Do not climb fences into pasture. Do not walk onto a road. Do not retrieve
anything from a tree using a method you would not describe to somebody.

## Before you use this page as authority

This page is `status: draft`. It has been written carefully and it has not been
reviewed by anyone with formal expertise in the regulations it describes. Every
`TODO(verify)` above is a place where a specific number or rule section has been
deliberately left out rather than guessed at.

Where a claim matters legally or physically, the primary source is linked and
the primary source is what governs. Read it.

If you find something on this page that is wrong, that is the most useful bug
report this project can receive.

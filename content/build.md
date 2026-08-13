---
title: Build guide
description: Step by step assembly of an oApogee payload, with an observable checkpoint at every stage and a link to the failure branch when a checkpoint does not pass.
tier: all
difficulty: intermediate
time_estimate: null
updated: 2026-08-11
status: draft
---

# Build guide

Every step here ends with something you can see, hear, or measure. If you see
the wrong thing, the step tells you where to go before you build anything on top
of the mistake.

**This guide is a skeleton.** The structure, the checkpoints, and the failure
branches are written. The specific values, photographs, and anything that
depends on a physical board are marked and missing, because the board does not
exist yet. Do not attempt to build from this expecting every number to be there.

## Before you start

Read [Safety and rules](/safety) first, particularly the lithium cell section.
The cell is the only part of this project whose failure mode is fire.

Have the [bill of materials](/bom) open. Check every part against it before you
heat the iron, including the ones you already own, because a substitution you
made months ago is not a substitution you will remember today.

Work somewhere you can leave things out. Putting a half-finished board in a
drawer loses more than it saves.

TODO(verify): say how long this build actually takes, and whether it is
realistically one sitting or two, once `build_time_estimate` in
`data/tiers.yaml` is filled from timing people who have soldered before. The
brief's "an evening" is a target, not a measurement.

## Stage 0: Sort and inspect

1. Lay every part out and identify it against the bill of materials.
2. Check the battery connector polarity against the board silkscreen.
   **Do not skip this.** Lithium cells ship with both polarities on the same
   connector, and reversing one can destroy the board instantly.
3. Inspect the cell for swelling, dents, and damaged wire insulation. Set it
   aside, in its bag, with the connector taped. It does not come out again until
   Stage 5.

> **Checkpoint 0.** Every part on the list is present and identified, and you
> know which pin of your cell's connector is positive. If the polarity does not
> match the silkscreen, stop and read
> [it runs on USB but dies immediately on battery](/troubleshooting#short-battery)
> before going further.

TODO(photo): the full parts layout for a Track build on the Modules path, top
down, every part labelled, on a plain background. This is the photo people
screenshot to check their order arrived complete.

## The path fork

The rest of this guide splits once, here, and rejoins at Stage 4.

- **Modules path**: Stages 1M to 3M. Breakout boards wired to a carrier.
- **Board path**: Stages 1B to 3B. The custom oApogee PCB.

Everything from Stage 4 onward is identical. Firmware, mounting, calibration,
testing and flying do not care which way you built it.

If you are not sure, build Modules. The board is not fabricated yet.

---

## Modules path

### Stage 1M: The microcontroller carrier

1. Fit the microcontroller module to the carrier board or protoboard.
2. Solder the power and ground connections first, and nothing else yet.
3. Inspect every joint under magnification before applying power.

TODO(confirm-on-hardware): the specific module has not been selected. This stage
cannot be written concretely until it is. See the verify note on `mcu_module` in
the bill of materials for the selection criteria.

> **Checkpoint 1M.** With the multimeter in continuity mode and no power
> connected, there is no continuity between 3V3 and ground. If there is, you have
> a bridge, and finding it now costs a minute rather than a board.

4. Connect USB. The module should enumerate.

> **Checkpoint 1M-b.** The board appears to your computer. Holding the boot
> button while plugging in should present it as a USB drive. Releasing and
> replugging normally should present it as a serial device.
>
> Nothing appears: [it powers on but does not appear as a USB drive](/troubleshooting#not-a-drive).
> Nothing at all happens: [nothing happens when I plug it in](/troubleshooting#no-power).
> Try a different cable before anything else. Charge-only USB-C cables look
> identical to data cables and are the most common cause of both symptoms.

### Stage 2M: Sensors onto their buses

The sensors do not share one bus. The barometer sits on I2C, the IMU and the
high-g accelerometer share an SPI bus with a chip select each, and the GNSS
receiver sits on a UART. Fit the barometer alone first and confirm it, so one
part is proven before a second bus exists to complicate the picture.

1. Fit the barometer breakout.
2. Wire power, ground, SDA and SCL.
3. **Check the I2C pull-up resistors.** The barometer is the only part on this
   bus, so there should be exactly one pull-up pair on SDA and SCL. Count any
   the carrier or the microcontroller module already provides, not just the
   pair the breakout brings. Most breakouts have a jumper or a solder bridge to
   remove theirs.

TODO(confirm-on-hardware): this guide does not rank the Modules-path failure
modes by how often they happen, because ranking them needs reports from builds
that have actually happened.

> **Checkpoint 2M.** The serial console reports the barometer present and prints
> a pressure. Check it against the station pressure your local weather service
> publishes, not against a fixed band: standard sea level is 101325 Pa and the
> reading falls as elevation rises, so a correct sensor at a mile-high field
> reads far below any sea level figure. What tells you the part works is a value
> that is stable, plausible for where you are, and that rises slightly when you
> carry the board downstairs. A reading of zero, or a part that never appears at
> all, means [the barometer never appears on the bus](/troubleshooting#baro-missing).

4. Fit the IMU on the SPI bus: SCK, MOSI, MISO, and a chip select of its own.
   Confirm it before continuing.
5. Fit the high-g accelerometer if you are building one, on the same SPI bus,
   with a second chip select that is not shared with anything. Confirm it.
6. Fit the GNSS receiver if you are building Track, on a UART, with TX and RX
   crossed. Confirm it.

> **Checkpoint 2M-b.** Every sensor you fitted reports present, and the values
> move sensibly when you move the board: tilting changes the accelerometer axes,
> rotating changes the gyroscope, and lifting it changes the pressure very
> slightly. Intermittent dropouts as you add parts mean
> [sensors drop out intermittently](/troubleshooting#intermittent-sensor).

### Stage 3M: Radio, recovery aids, and power

1. Fit the radio module if you are building Link or Track.
2. **Attach the antenna before you ever transmit.** Transmitting into a
   disconnected antenna is bad for the radio.
3. Fit the buzzer and the status LED.
4. Fit the battery connector, checking polarity one more time.

> **Checkpoint 3M.** The buzzer sounds and the LED lights on command from the
> console. The radio reports present. You have not transmitted yet.

---

## Board path

TODO(confirm-on-hardware): this entire path depends on a fabricated PCB. Nothing
below can be written concretely until boards exist, and none of it has been
performed.

### Stage 1B: Power section

1. Solder the regulator, its passives, and the USB connector.
2. Solder the charge controller and its programming resistor.

> **Checkpoint 1B.** With no power applied, no continuity between 3V3 and
> ground. With USB applied and nothing else populated, 3V3 measures within
> tolerance at the regulator output.

TODO(verify): state the acceptable voltage range at this checkpoint, from the
chosen regulator's datasheet.

### Stage 2B: Microcontroller and flash

1. Solder the microcontroller.
2. Solder the QSPI flash.
3. Solder the crystal and its load capacitors if the design uses one.

> **Checkpoint 2B.** Holding the boot button and connecting USB presents the
> board as a USB drive. Copying a known-good test firmware onto it causes the
> status LED to light.

### Stage 3B: Sensors, radio, and connectors

1. Solder the barometer, the IMU, and the high-g accelerometer if fitting one.
2. Solder the radio and the GNSS receiver if fitting them.
3. Solder the battery connector, the buzzer, and the LED.

> **Checkpoint 3B.** Every populated part reports present over the serial
> console, and the sensor values move sensibly when you move the board.

TODO(photo): a completed board, top and bottom, at high enough resolution to
check joint quality against.

---

## Stage 4: First real firmware

Both paths rejoin here.

1. Follow [Firmware and flashing](/firmware) to load the current release.
2. Run the self-test from the serial console.

> **Checkpoint 4.** Self-test passes for every part you fitted, and reports
> absent for every part you did not. A part you fitted that reports absent is a
> soldering problem, not a firmware problem. A part you did not fit reporting
> present means the firmware is misconfigured for your tier.

3. Run the pad calibration procedure.

> **Checkpoint 4-b.** Reported altitude sits near zero and stays there, drifting
> only slowly. Large or fast drift at this point means the pressure reference did
> not settle. A large negative starting value means it was taken somewhere else:
> see [my altitude reads negative](/troubleshooting#altitude-negative).

## Stage 5: Battery

1. Connect the cell, having checked the polarity for the third time.
2. Confirm the board runs from the cell with USB disconnected.
3. Confirm charging works with USB connected.

> **Checkpoint 5.** It runs on the cell alone and charges when plugged in. If it
> runs on USB and dies on the cell, stop and read
> [it runs on USB but dies immediately on battery](/troubleshooting#short-battery).

TODO(confirm-on-hardware): document the LED behaviour for charging, charge
complete, and charge fault, once that behaviour is implemented.

## Stage 6: Into the enclosure

Follow [Mounting](/mounting) for the sled or the pod, whichever form factor you
are using. That page also covers the static ports, which are not optional and
are the most commonly botched part of the whole build.

1. Print the enclosure for your form factor and airframe diameter.
2. Secure the board so it cannot move.
3. Secure the cell so it cannot move, and route the wires so the enclosure
   closing cannot pinch them.
4. Drill and deburr the static ports.

> **Checkpoint 6.** Shake the closed assembly hard and listen. Nothing rattles,
> nothing buzzes, nothing shifts. Anything you can hear moving in your hand will
> move much harder under boost. Open it and confirm the cell is still where you
> put it.

## Stage 7: Ground testing

Do all four tests on the [safety page](/safety#ground-testing), in order, before
this goes anywhere near a rocket: the bench dry run, the shake test, the drop
test, and the full arm-to-landing rehearsal. A payload that is going to fail
should fail on your bench, where it costs an evening.

> **Checkpoint 7.** A complete log file, on the flash, that exports to CSV and
> opens. If you can read a bench run end to end, you can read a flight.

## Stage 8: The rocket

1. Re-simulate your rocket in OpenRocket with the payload mass and position
   included. This is not optional and it is the step with a consequence for
   people other than you.
2. Fit the payload.
3. Confirm the rocket still slides freely on the rail, with the pod fitted. An
   external pod is the most common thing to catch on a rail button.

> **Checkpoint 8.** Your simulated stability margin with the payload fitted is
> within the range on the [safety page](/safety#stability-is-a-safety-issue), and the rocket
> moves freely on the rail.

Then work through the [preflight checklist](/preflight) on the day.

## Ground station

If you built Link or Track, you also need a receiver. That is a separate build:
see [Ground station](/ground-station).

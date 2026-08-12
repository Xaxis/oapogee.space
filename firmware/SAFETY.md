# The passive boundary, as it applies to code

oApogee is a passive instrumentation payload. It does not fire ejection charges,
control deployment, ignite motors, or command any pyrotechnic device. It has no
mechanism to do any of those things.

That sentence appears on the homepage, in `data/tiers.yaml`, and in the safety
page. This file is about the part of it that lives in the firmware, and about the
two artifacts that make it checkable instead of merely stated.

Nothing described here has run on hardware. No board has been fabricated. These
are properties of the source, enforced by checks that run at build time, and they
are worth exactly what a property of source code is worth: they say what the code
can do, not what a soldering iron can do to a board.

## What the boundary means in code

Three claims, each of which has to be true of the source and not just of the
documentation.

**There is exactly one class of thing the firmware can drive, and there are two
of them.** A piezo buzzer and a status LED. Both exist to help a person find a
rocket or to tell a person what state the payload is in. They are declared in
`port/include/oapogee_port/oa_out.h` and listed in `port/outputs.allowlist`.

**No decision-making code can drive anything.** The state machine, the fusion
filter, the scheduler and the packet builders are in `core/`, and `core/` cannot
name an output function at all.

**There is no path from the air into the firmware.** The radio transmits and
never receives. There is no receive function anywhere in the port layer, no
packet parser anywhere in core, and no packet type in the format that commands
the vehicle. A ground station cannot arm this payload, cannot trigger anything,
and cannot deploy anything, because there is nothing to send to.

## The two enforcement artifacts

Both are plain text. Both are read by a build target. Both fail the build rather
than printing a warning, because a boundary that produces a warning is a boundary
somebody scrolls past.

```bash
cmake --build firmware/build --target oa-check
```

### port/outputs.allowlist

Every physical output the firmware may drive, one per line, with its schematic
reference designator, the pin label it lands on, and what it is for. Two lines.

This is the artifact a reviewer reads to confirm the boundary without reading any
C. That is its whole job. Somebody who does not write C, and somebody who does
but has ten minutes, can open one file and see the complete list of things this
payload can make happen in the physical world.

The `check-outputs` target compares it, name for name, against the `oa_out_t`
enumerators in `oa_out.h`.

**What it catches.** An output added to the C without a line in the list a
reviewer reads. A line in the list with no implementation behind it. Any change
to the count away from exactly two. A rename on one side but not the other. In
each case the build fails with a message naming both lists.

**What it does not catch.** It does not know what a pin is wired to. If somebody
fabricates a board that connects the buzzer pin to something other than a buzzer,
no software check will notice, and no software check could. It also does not
police the bus lines: the firmware drives I2C, SPI, UART and QSPI, and those are
declared in `oa_port.h` as buses because they carry bytes to parts soldered to
the board. The property this list protects is that no general purpose output
exists that could be connected to an igniter, and that the two that do exist are
wired, on the schematic, to a piezo buzzer and an LED.

A third entry appearing in both files is not a firmware change that happens to
pass. It is a change to what oApogee is, and it should arrive as a project
decision made in the open, with the reasoning written down.

### core/allowed-undefined.txt

The complete list of external symbols `core/` objects may reference. Four
entries: `memcpy`, `memset`, `memmove`, `memcmp`.

The `check-undefined` target runs `nm` over every core object and fails if any
undefined symbol is not on that list. It also fails if it finds no objects at
all, because a check that inspects nothing must not report success.

**What it catches, and why this is a safety artifact rather than a hygiene one.**
The functions that drive the buzzer and the LED live in `port/`. A call to
`oa_out_buzzer_set` from anywhere in `core/` would appear as an undefined symbol
that is not on this list, and the build would fail naming the object file and the
symbol. So the flight state machine, the fusion filter, the scheduler and the
packet builders cannot drive anything at all. Not by convention, and not because
a reviewer would have noticed: by construction.

There is a second fence behind it. `core/` is compiled without `port/include` on
its include path, so core cannot include `oa_out.h` and cannot even name those
functions. The symbol check is what catches somebody declaring them by hand.

The same check catches the quieter things. `malloc` or `printf` in core. A Pico
SDK call that crept in. A compiler runtime helper pulled in by floating point on
a part whose FPU nobody has characterised, or by a 64-bit divide in the sample
path. Each of those is worth knowing about in code that runs between sensor
reads.

**What it does not catch.** Nothing about correctness. Nothing about `port/` or
an application, both of which are expected to reference an SDK. It fences core
and only core.

**How the list changes.** By somebody deciding to, in a commit that says why. A
symbol turning up that is not on the list is a fact about the code, and the first
question is always whether core should be doing that, not whether the list should
be longer. If it grows past a handful of entries, core has stopped being the
thing this project claims it is.

## Where else the boundary shows up in the source

These are not separate checks. They are places the same property is visible, and
they are listed so a reader can verify it themselves.

`oa_out.h` has no generic `oa_out_write(which, value)`. A generic driver would
make the two outputs interchangeable at a call site and imply a class of thing
that can be extended. There are two named functions and an all-off, and a
`_Static_assert` that the count is two.

`oa_port.h` has no receive function. Not a stub, not a commented-out one, not one
that returns an error. The absence is the property.

`oa_packet.h` declares builders and no decoder. The payload builds packets and
transmits them, and never interprets a packet it did not build, because there is
nothing on the air it should obey.

`oa_state.h` has no entry point that sets a state. The machine advances through
`oa_state_step` on sensor samples, and the only thing outside the payload that
can reach it is the arming switch on a GPIO. Apogee detection is a measurement
and produces a number, not a command.

The scheduler can only emit the five downlink packet types. There is no queue
entry that means anything else and no inbound path that could add one.

## The thing this cannot promise

None of this has been run on a board, because no board exists. The checks confirm
properties of the source. They say nothing about a soldered assembly, about what
a builder connects to a header, or about firmware somebody modifies and flashes
themselves.

The honest statement is narrow and it is worth keeping narrow: as written, this
firmware has two outputs, they are a buzzer and an LED, the parts of it that make
decisions cannot reach either one, and there is no way to send it anything.

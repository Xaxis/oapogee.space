# oApogee firmware

The software that runs on the oApogee payload: the flight state machine, the
sensor fusion, the packet builders, the log writer, and the transmit scheduler.

**No build in this directory has ever run on hardware.** No board has been
fabricated, no image has been flashed, no packet has been transmitted, and no
flight has been logged. What exists here is the contract: headers, the field
tables the specs are encoded in, the build system, and two artifacts that
enforce the passive payload boundary mechanically. The implementations come
next, and they will still not have run on hardware when they arrive.

## What is here and what is not

Implemented:

| Part | State |
|---|---|
| Wire format tables, packets and log records | Written, checked against the specs at compile time |
| Public interfaces for every core module | Written |
| Flight state enumeration | Written by hand to the shape the generator will produce |
| Host build, warnings as errors | Builds on macOS clang and on Linux gcc |
| Output allowlist and undefined-symbol checks | Written and running |

Not implemented:

| Part | State |
|---|---|
| Every function body in `core/src/` | Stubs. The files contain an include and a comment |
| Everything in `port/` | Declarations only. There is no `port/src` |
| The application that ties them together | Does not exist |
| Tests | Do not exist. The build picks up `tests/` when it appears |
| The target build | Refuses to configure, with a message saying why |
| `tools/gen-states.mjs` | Does not exist. `gen/oa_states.h` is a hand-written copy of what it will emit |

## Why C

Three reasons, in order of weight.

The payload writes samples in an interrupt-adjacent context on a
microcontroller, into a fixed-width binary format, with no allocator and a
bounded stack. C is the language where that is the ordinary thing to do rather
than the thing you fight the runtime for.

The RP2350 ecosystem is C. The Pico SDK, the vendor sensor code a builder might
crib from, and every example a beginner will find are C, and a payload written
in something else makes every one of those a translation exercise for the person
least equipped to do it.

The documentation is the product. Someone who has never used this project should
be able to read `oa_packet_fields.def` and see the wire format, without knowing a
build system or a language feature. C is small enough that the field table is
the interesting part of the file.

The costs are real. There is no type system helping here, no bounds checking,
and no ownership model. What this codebase does instead is keep the dangerous
surface small: no allocation anywhere in core, no global mutable state, buffers
owned by callers, and a warning set that treats every implicit conversion as an
error, because a silent narrowing in a packing function produces a number that is
wrong and plausible.

## The core and port split

```
firmware/
  core/           No SDK. No libc beyond string.h, stdint.h, stdbool.h, stddef.h.
    include/      The interfaces. This is the contract.
    src/          Implementations. Stubs today.
    allowed-undefined.txt
  gen/            Generated from data/. Do not hand-edit.
  port/           The hardware surface. Declarations only.
    include/
    outputs.allowlist
  CMakeLists.txt
```

`core/` holds everything that decides anything: the state machine, the
complementary filter, the barometric conversion, the pad reference accumulator,
the ring buffer, the scheduler, the packet builders, the log packers and the
configuration validator. It has no idea what a sensor is. It takes numbers in
and returns numbers out.

`port/` is every function that touches a peripheral, declared in two headers and
implemented nowhere yet. `oa_port.h` is the whole hardware surface: time,
sensors, storage, radio, console, watchdog. `oa_out.h` is separate and short,
because it is the list of physical outputs the firmware may drive and it is
meant to be read on its own.

**Core is the only part that can be tested today, and that is the point.** It
compiles with nothing installed but CMake and a compiler, on a laptop, with no
board in the room. So the interesting behaviour, the transitions, the fusion, the
apogee detection lag, the byte layout of every packet, is testable now rather
than after a board arrives. A test can drive an entire flight through the state
machine from a table of samples, including the flights that only happen when a
sensor fails, which is a category of test nobody runs by flying rockets.

The seam that makes this work is `oa_sink_t`. Core writes bytes into a sink and
knows nothing else: not LittleFS, not a flash part, not a radio. Tests point a
sink at a buffer and compare against the spec tables; the firmware points the
same code at flash. There is no second implementation of the packing to keep in
step.

## The field tables are the important part

`core/include/oapogee/oa_packet_fields.def` and `oa_log_fields.def` are X-macro
tables: one line per field, with its type and byte offset. Everything that needs
to know the layout expands the same table. The packet type enumeration, the
length constants, the body structs, the builder declarations and the conformance
test that dumps the layout all come from one list.

A field that moves moves in one place. Everything that disagreed with it either
stops compiling or fails a test. The stated body and total lengths from the spec
are checked against the field offsets by `_Static_assert`, so a body length
edited without its total length fails the build rather than the flight.

The log table carries each field's scale and unit as string literals, and the
manifest writer copies them verbatim into `meta.json`. That is deliberate: the
log format is self-describing, so a manifest that disagreed with the packer would
produce files that decode wrongly and look fine. Emitting both from one table
makes that impossible rather than unlikely. Strings also mean there is no float
formatter on the payload turning an exact scale factor like `1e-7` into an
approximate one, in the one file that tells a reader how to interpret every other
byte.

## There are no tuning constants in this firmware

Every flight threshold, interval, rate and window is unmeasured. There is no
hardware, nothing has flown, and no sensor noise floor has been characterised.

So none of them is a literal in a source file. They all live in `oa_config_t`,
they all start unset, and each one carries a string saying what measurement would
settle it. `oa_config_is_flightworthy()` refuses to arm while a required one is
missing, and the payload can print exactly which ones and what would close them.

A payload that will not arm is a payload telling the truth about what it knows.
That is the correct behaviour at this stage, and it is how this firmware avoids
shipping a guess that would fly.

Constants from the specs are not tunables and are in code where they belong: the
CRC polynomial, the field offsets, the record widths, the 50000 Pa pressure
offset, the 2.5 V battery offset. The test is whether the number would change if
somebody measured something.

## Building

```bash
cmake -S firmware -B firmware/build
cmake --build firmware/build
cmake --build firmware/build --target oa-check
```

Nothing but CMake 3.16 and a C11 compiler. Warnings are errors:
`-Wall -Wextra -Werror -Wconversion -Wshadow -Wvla -Wdouble-promotion`.

Linking `liboa_core.a` currently prints a warning that the archive defines no
symbols. That is correct: the sources are stubs.

`oa-check` runs the two enforcement checks. See [SAFETY.md](./SAFETY.md) for what
each one catches and why it exists.

The target build is declared and refuses to configure:

```bash
cmake -S firmware -B firmware/build-target -DOA_BUILD_TARGET=ON
# fatal error, with a message explaining that it needs the Pico SDK
```

## Where the specifications live

The firmware conforms to these. It never redefines them. Where the firmware and
one of these documents disagree, the firmware is wrong.

- `docs/spec/telemetry-packet.md`, the downlink wire format
- `docs/spec/log-format.md`, the onboard log
- `data/flight-phases.yaml`, the flight state machine

## Safety

oApogee is a passive instrumentation payload. It does not fire ejection charges,
control deployment, ignite motors, or command any pyrotechnic device.
[SAFETY.md](./SAFETY.md) describes how that boundary is enforced in code rather
than asserted in a comment.

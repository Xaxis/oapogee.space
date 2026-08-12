# oApogee

An open source telemetry payload for model rockets, and the documentation that
teaches you to build one. Site at **[oapogee.space](https://oapogee.space)**.

oApogee records altitude, acceleration and orientation through a flight, logs it
to onboard flash, and optionally downlinks it live over LoRa and reports GNSS
position for recovery. It attaches to a rocket you already own, in a printed pod
that needs no modification to the airframe.

**oApogee is a passive instrumentation payload. It does not fire ejection
charges, control deployment, ignite motors, or command any pyrotechnic device.**
That is a design boundary and it will not change. See
[content/safety.md](content/safety.md).

## Project status

**This is a design on paper.** Nothing has been fabricated, assembled, weighed,
priced, or flown. Every cost, mass, range and endurance figure on the site is
missing rather than estimated, and each gap records what evidence would close
it. See [TODO-VERIFY.md](TODO-VERIFY.md), which is generated from the content
files and checked in CI.

## The rule everything else follows from

**Never publish a number that has not been measured or sourced.**

The documentation is the product. The hardware is commodity: barometer, IMU,
microcontroller, radio, all of it available to anyone. What makes this project
worth existing is that a motivated beginner can follow the site start to finish
and end up with a working payload and a real flight graph. That only works if
every number on it is true.

So there are no estimates dressed as measurements. A figure that has not been
verified is written as a marker saying what evidence would close it, and the
build fails if the index of those markers goes stale. A site full of confident
fabricated specs is worse than useless in a hobby where people's build decisions
depend on them.

Two consequences you will notice immediately: the bill of materials has no
prices in it, and the homepage does not have a flight graph.

## Layout

```
content/     Prose pages, Markdown with YAML frontmatter
data/        Structured source: bom.yaml, tiers.yaml, glossary.yaml
docs/        Working documents, including the page map and the format specs
apps/web/    Next.js 15 App Router site, one renderer of content/ and data/
tools/       Checks: prose rules, data cross-references, marker index
```

`content/` and `data/` are the product. `apps/web/` renders them. Anything that
appears in more than one place on the site lives in `data/` as YAML, never as a
hand-written table inside a Markdown page, because two hand-written tables drift
and nothing catches it.

## Commands

Everything is a `make` target, and CI runs the same ones. `make` lists them.

```bash
make install     # dependencies, pinned to match CI
make dev         # the site, locally
make check       # everything CI runs
make check-fast  # the same without the site build
make todos       # regenerate TODO-VERIFY.md after editing content
```

Run `make todos` after adding or closing a verification marker and commit the
result. `make check-todos` fails otherwise.

## Contributing

The most useful contribution to this project right now is telling us where it is
wrong. The safety and regulatory pages in particular are written carefully and
have not been reviewed by anyone with formal expertise in the rules they
describe.

Read [CONTENT-STYLE.md](CONTENT-STYLE.md) before writing anything. It covers the
accuracy rule, the voice, and the naming conventions, all of which are enforced
in CI.

## Naming

The project is **oApogee**: lowercase `o`, capital `A`, one word. The `o` stands
for open.

**It is never shortened to "Apogee."** Apogee Components is the dominant
retailer in model rocketry and owns that word in this hobby completely. There is
no short form. oApogee is not affiliated with Apogee Components.

The three tiers are oApogee Solo, oApogee Link, and oApogee Track.

## Licence

Three, because this repository holds three different kinds of thing.

| What | Licence | SPDX |
|---|---|---|
| Hardware design files | CERN Open Hardware Licence v2, Strongly Reciprocal | `CERN-OHL-S-2.0` |
| Firmware and software | Apache License 2.0 | `Apache-2.0` |
| Documentation and content | Creative Commons Attribution-ShareAlike 4.0 | `CC-BY-SA-4.0` |

Full texts in [LICENSES/](LICENSES), and [LICENSE](LICENSE) says which applies
to which files and why. Reciprocal on the board because the plausible bad
outcome is a closed volume clone; permissive on the firmware because that is the
part worth having other people embed everywhere.

**The design is open. The name is not.** Build it, sell it, fork it, call it
something else. Licences do not stop somebody shipping a clone under this name;
a trademark does.

A project named for openness that ships a black box loses the audience it was
built for, so the intent is to publish the KiCad project rather than a PDF
schematic, the parametric CAD source rather than only STLs, and the full
firmware including calibration constants. Anything that cannot be opened will be
declared explicitly rather than quietly omitted.

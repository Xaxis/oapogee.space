# Contributing to oApogee

The most useful contribution right now is telling us where this is wrong.

Nothing has been built, so there is no hardware to test and no firmware to
debug. What there is: a design, a parts list, two wire format specifications,
and a set of safety and regulatory pages written carefully by people who are not
lawyers, licensed amateur radio operators, or NAR and Tripoli officials.

If you are any of those, the safety and radio pages are where being wrong has
consequences beyond embarrassment, and where a correction is worth the most.

## The rule everything else follows from

**Never publish a number that has not been measured or sourced.**

No price, mass, range, current draw, battery life, or measured altitude goes on
this site unless it came from an invoice, a scale, a datasheet, or a flight. A
figure you do not have is written as a marker recording what evidence would
close it:

```
TODO(verify): typical mass of the Solo build with a 150 mAh cell, measured on a
0.1 g scale, three assembled units.
```

A marker with no closing condition is not a marker, it is a shrug.

This is not pedantry. People in this hobby spend money and fly hardware over
other people's heads based on what a site like this tells them. A page full of
confident fabricated specifications is worse than an empty one.

If you contribute a number, say where it came from in the pull request. If it
came from a measurement, say how it was measured and how many times.

## Before you write anything

Read [CONTENT-STYLE.md](CONTENT-STYLE.md). It is short and it covers the
accuracy rule, the voice, the layering pattern, and the naming conventions.
Several of those are enforced by the build, so a pull request that ignores them
fails rather than gets a comment.

Two that catch people:

- **Never use em dashes.** Use a comma, a colon, parentheses, or two sentences.
- **Never shorten oApogee to "Apogee."** Apogee Components owns that word in
  model rocketry. There is no short form.

## Getting set up

```bash
make install     # dependencies, pinned to match CI
make dev         # the site, locally
make check       # everything CI runs
make check-fast  # the same without the site build
```

`make` on its own lists every target.

## What the build checks

Each of these is a claim the repository makes about itself, and each fails the
build rather than producing a warning nobody reads.

| Target | Claim |
|---|---|
| `make prose` | No em dashes, no emoji, and the name is never shortened |
| `make data` | Part, tier and glossary cross-references resolve; frontmatter is complete; a null price carries a marker; no page claims `verified` with open markers |
| `make links` | Every internal link resolves, including its anchor |
| `make check-todos` | `TODO-VERIFY.md` matches the markers actually in the sources |
| `make check-schematic` | The committed diagram matches `data/system.yaml` |

After editing content, run `make todos` and commit the regenerated
`TODO-VERIFY.md`. After editing `data/system.yaml`, run `make schematic` and
commit the regenerated SVG. Both are checked, so a stale one cannot merge.

## Where things live

```
content/     Prose pages, Markdown with YAML frontmatter
data/        Structured source: bom, tiers, glossary, preflight, troubleshooting
docs/spec/   The two wire format specifications
apps/web/    The site, which is one renderer of content/ and data/
tools/       The checks above
```

`content/` and `data/` are the product. The site renders them and holds no
content of its own.

**Anything that appears in more than one place belongs in `data/` as YAML**, not
as a table written by hand inside a Markdown page. Two hand-written tables drift
apart and nothing catches it. The bill of materials, the glossary, the preflight
checklist, the tier comparison, the troubleshooting index and the system diagram
are all structured data for this reason.

## Page status

Frontmatter carries a `status`, and it is a promise to the reader about process
rather than about quality:

- `draft`, written, checked by nobody.
- `needs-review`, the author believes it is right and wants a second opinion.
- `verified`, somebody with the relevant expertise checked every number and every
  step on real hardware.

Only the maintainer moves a page to `verified`, and the build refuses to let a
page claim it while unresolved markers remain.

## Pull requests

- One subject per pull request. A parts change and a rewrite of the safety page
  are two pull requests.
- Say what evidence backs any number you added.
- Run `make check` before pushing. CI runs exactly the same targets, so a green
  local run is a green CI run.
- Corrections to safety, radio, or regulatory content are welcome without a
  pull request. An issue saying "this is wrong, here is the primary source" is
  more useful than a polished patch that guesses.

## Reporting something wrong

Open an issue at
[github.com/Xaxis/oapogee.space/issues](https://github.com/Xaxis/oapogee.space/issues).

The most valuable reports, in order:

1. A safety or regulatory claim that is wrong, with the primary source.
2. A number that is stated without evidence, which should be a marker instead.
3. A build step that cannot be followed as written.
4. A wire format ambiguity that two implementers could read differently.

## Scope

**oApogee is a passive instrumentation payload. It does not fire ejection
charges, control deployment, ignite motors, or command any pyrotechnic device.**

This is a design boundary, not a missing feature, and contributions that move
toward deployment control will be declined regardless of quality. The boundary
is what keeps the worst outcome of any bug in this project a disappointing
graph.

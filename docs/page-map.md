# oApogee page map

Proposed information architecture, for approval before any page beyond Safety
and rules is written. Section 6 of the brief is the basis. Where this deviates
or adds, it is marked **[added]** or **[changed]** with the reasoning.

## The organising decision, and the one thing to approve first

You answered "both, breakouts first" for the build path. That produces a
two-axis product, and the page map has to hold both axes without doubling every
page:

- **Three tiers**, which are about capability. Solo, Link, Track.
- **Two build paths**, which are about fabrication. The same electrical design,
  assembled two ways.

Proposed path names, since the brief does not supply them:

| Path | Name | What it is | Who it is for |
|---|---|---|---|
| M | **Modules** | Off-the-shelf breakout boards on a carrier or protoboard, wired by hand | Anyone, today, before a PCB exists |
| B | **Board** | The custom oApogee PCB, one part number, reflow or hand-soldered | Once boards are available, and for anyone building more than one |

Tier and path are orthogonal, so "oApogee Link, Modules path" is a legitimate
build and the site must be able to describe it without a combinatorial mess.
The way to avoid the mess: **the tier owns the capability content, the path owns
only the assembly content.** Firmware, mounting, safety, preflight, data format,
and troubleshooting are identical across paths and are written once. Only the
Build Guide splits, and it splits at one clearly-marked fork rather than being
two separate pages.

The Modules path is what makes the site useful before hardware exists, which,
given that this is currently a paper design, is the difference between a live
site and a landing page. It also carries an honest cost: the module build will
be heavier and bulkier than the target mass budget, and the site must say so
rather than quietly quoting the board figure everywhere.

If you want a different split, this is the piece to change now, because the BOM
data model encodes it.

## Routes

Fifteen pages from the brief, plus four additions. Routes are flat where a
reader might type them and nested where they are genuinely subordinate.

| # | Page | Route | Source | Renders data |
|---|---|---|---|---|
| 1 | Home | `/` | `content/home.md` + components | `tiers`, one real flight |
| 2 | Start here | `/start` | `content/start-here.md` | `tiers` |
| 3 | Bill of materials | `/bom` | generated page | `bom`, `tiers`, `tools` |
| 4 | Build guide | `/build` | `content/build/*.md` | `bom`, `checkpoints` |
| 5 | Firmware and flashing | `/firmware` | `content/firmware.md` | `config-reference` |
| 6 | Mounting | `/mounting` | `content/mounting.md` | `models` |
| 7 | Preflight checklist | `/preflight` | generated page | `preflight` |
| 8 | Ground station | `/ground-station` | `content/ground-station.md` | `bom` (GS section) |
| 9 | Reading your data | `/data` | `content/reading-your-data.md` | one real flight |
| 10 | Flight log | `/flights` | generated index | `flights/*` |
| 11 | Troubleshooting | `/troubleshooting` | generated page | `troubleshooting` |
| 12 | Reference | `/reference` | `content/reference/*.md` | `pinout`, specs |
| 13 | Safety and rules | `/safety` | `content/safety.md` | `glossary` |
| 14 | FAQ | `/faq` | `content/faq.md` | |
| 15 | About and license | `/about` | `content/about.md` | |
| 16 | Glossary **[added]** | `/glossary` | generated page | `glossary` |
| 17 | Specs **[added]** | `/reference/telemetry-packet`, `/reference/log-format` | `docs/spec/*.md` | |
| 19 | Changelog **[added]** | `/changelog` | `CHANGELOG.md` | |

### The four additions, and why

**Glossary at `/glossary` [added].** The brief already requires a glossary data
file for consistent term linking. Once it exists as data, giving it a page costs
nothing and gives every inline term definition somewhere to link to. It is also
the single best page for the educator, who needs vocabulary before a lesson.

**Specs as real routes [added].** The brief asks for `docs/spec/telemetry-packet.md`
and `docs/spec/log-format.md` as repository deliverables. A wire format that
third parties are invited to implement needs a stable citable URL, not a path
inside a git repo. These render at `/reference/...` from the same source file
that lives in `docs/spec/`, so there is exactly one copy.

**Changelog at `/changelog` [added].** Errata for a flight computer are safety
information. If a firmware release changes an apogee detection threshold,
somebody flying the old build needs to find that out.

## Page by page

Ordered as the brief orders them, not as they appear in navigation.

### 1. Home, `/`

One sentence a stranger understands, then the four numbers that decide it: cost,
mass, what data you get, what it will not do. Lead with a real annotated flight
graph, not a hero illustration.

Blocked: there is no real flight graph yet. Until there is one, the hero slot
carries a labelled statement of project status rather than a synthetic graph
dressed up as data. A fabricated hero graph on the homepage of a project whose
entire pitch is honest data would be the single worst decision available.

### 2. Start here, `/start`

Tier and path comparison, honest prerequisites (tools, skills, time), what to
expect, and a prominent statement of what oApogee does not do. This is where the
passive-instrumentation boundary gets its clearest statement, because it is the
page a newcomer actually reads.

### 3. Bill of materials, `/bom`

Fully generated from `data/bom.yaml`. Per tier, per path, with part numbers,
supplier links, quantities, running totals, substitution flags, and a minimum
tools list. Every price starts as `null` with a verification marker attached.

### 4. Build guide, `/build`

Sectioned, not one long page: preparation, the path fork, per-tier module
sections, first power-on, bring-up checks. Every step has an observable
checkpoint and a link into Troubleshooting for its failure branch. Photo slots
marked throughout.

### 5. Firmware and flashing, `/firmware`

UF2 drag and drop first, source build second, config file reference third,
calibration procedure fourth. The beginner never has to scroll past a toolchain
to get a working board.

### 6. Mounting, `/mounting`

Form A internal sled and Form B external pod, both with printable models and
parametric source. Static ports get a full treatment here, including sizing,
count, placement relative to the sensor, and deburring. Stability and CG get a
worked OpenRocket example. Mass budget effect on apogee by motor class.

The hi-vis soldermask and pod colour reasoning lives here, where it is a
recovery decision, not on a design page where it would read as decoration.

### 7. Preflight checklist, `/preflight`

Generated from `data/preflight.yaml`. Screen version, print stylesheet, and a
plain text export. Designed to be laminated and used on a range table with cold
hands.

### 8. Ground station, `/ground-station`

Assembly, pairing, antenna guidance, and range expectations with honest numbers
including the disappointing ones. WebSerial browser receiver, pending your
confirmation of that approach.

### 9. Reading your data, `/data`

File format, import paths, and one complete worked flight with every feature of
the graph named: burn, burnout kink, coast, transonic noise if present, apogee,
deployment jolt, descent rate, landing. This page cannot be written honestly
until a flight exists.

### 10. Flight log, `/flights`

Per your answer, v1 designs the submission format and ships your flights only.
The format is specified now, as data, so that community ingest later is a
mechanism change rather than a redesign.

### 11. Troubleshooting, `/troubleshooting`

Symptom-first, generated from `data/troubleshooting.yaml`, so that build steps
can link directly to a symptom anchor. "It never detects launch." "My altitude
reads negative." "The radio drops at 200 m."

### 12. Reference, `/reference`

Schematic, pinout, mechanical drawings, packet spec, log format spec, changelog,
errata. No hand-holding. The engineer reaches this from the top of every page.

### 13. Safety and rules, `/safety`

Written first, per the brief. LiPo handling, ground testing, radio law with
Part 15 and Part 97 firewalled from each other, NAR and Tripoli codes, FAA
Part 101 Subpart C Class 1 limits, recovery ethics, and the passive payload
boundary.

### 14. FAQ, `/faq`

Real questions only. Anything asked twice in the wild gets added. No invented
questions used as a copywriting device.

### 15. About and license, `/about`

What the project is, the naming story in two sentences, pronunciation, and the
licensing decision with its tradeoff presented rather than assumed. This is also
where any part of the design that cannot be opened must be declared explicitly.

## Navigation

Three groups, because the three readers arrive with different intent:

- **Build:** Start here, Bill of materials, Build guide, Firmware, Mounting,
  Ground station
- **Fly:** Preflight, Reading your data, Flight log, Troubleshooting, Safety
- **Reference:** Reference, Specs, Glossary, Status, Changelog, About

Safety appears in Fly and is also linked persistently in the footer of every
page.

## Data files this map requires

| File | Feeds |
|---|---|
| `data/tiers.yaml` | Home, Start here, BOM, Build guide |
| `data/bom.yaml` | BOM, Build guide, Ground station |
| `data/tools.yaml` | BOM, Start here |
| `data/glossary.yaml` | Glossary, inline term links everywhere |
| `data/preflight.yaml` | Preflight |
| `data/troubleshooting.yaml` | Troubleshooting, Build guide failure branches |
| `data/checkpoints.yaml` | Build guide |
| `data/flights/*.yaml` | Flight log, Home, Reading your data |

## What I need from you

1. Approve or change the tier and path split, and the Modules and Board names.
2. Approve the four added pages, or cut them.
3. Confirm the navigation grouping.

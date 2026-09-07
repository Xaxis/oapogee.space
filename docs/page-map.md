# oApogee page map

What the site is, page by page, and which file each page is rendered from. This
describes the site as built. It replaced a proposal document that was still
asking for approval of decisions taken months earlier, and that named four files
which never existed.

`apps/web/lib/routes.ts` is canonical for the route table, the navigation
grouping and the reading order. If this document and that file disagree, the
file is right and this one is stale.

## The two axes

Three **tiers**, which are about capability: Solo, Link, Track. Two **build
paths**, which are about fabrication: Modules and Board, the same electrical
design assembled two ways.

They are orthogonal, so "oApogee Link, Modules path" is a legitimate build and
the site has to describe it without a page per combination. It does not have
one, because **the tier owns the capability content and the path owns only the
assembly content**. Firmware, mounting, safety, preflight, the wire formats and
troubleshooting are identical across paths and are written once. Only the build
guide splits, at a single marked fork, and it rejoins at Stage 4.

That split is encoded in `data/bom.yaml`, where every part carries an `applies`
list of tier and path pairs. Changing it means changing the data model.

## Routes

| Route | Rendered from | Data it reads |
|---|---|---|
| `/` | `apps/web/app/page.tsx` | `tiers`, `flight-phases`, `system` diagram |
| `/start` | `content/start-here.md` | |
| `/bom` | `apps/web/app/(docs)/bom/page.tsx` | `bom`, `tiers`, `suppliers` |
| `/build` | `content/build.md` | |
| `/firmware` | `content/firmware.md` | |
| `/mounting` | `content/mounting.md` | `mechanical` |
| `/ground-station` | `content/ground-station.md` | |
| `/preflight` | `apps/web/app/(docs)/preflight/page.tsx` | `preflight` |
| `/safety` | `content/safety.md` | |
| `/data` | `content/reading-your-data.md` | |
| `/troubleshooting` | `apps/web/app/(docs)/troubleshooting/page.tsx` | `troubleshooting` |
| `/flights` | `apps/web/app/(docs)/flights/page.tsx` | `flights` |
| `/reference` | `content/reference.md` | |
| `/reference/schematic` | `apps/web/app/(docs)/reference/schematic/page.tsx` | `pcb-status`, generated SVGs |
| `/reference/telemetry-packet` | `docs/spec/telemetry-packet.md` | |
| `/reference/log-format` | `docs/spec/log-format.md` | |
| `/glossary` | `apps/web/app/(docs)/glossary/page.tsx` | `glossary` |
| `/faq` | `content/faq.md` | |
| `/changelog` | `CHANGELOG.md` | |
| `/about` | `content/about.md` | |

Content routes are shorter than their filenames on purpose: `/data` is what
somebody types, `reading-your-data.md` is what makes sense in a directory
listing. `routes.ts` is the one place the two are reconciled.

Three routes render a file that lives outside `content/` because it is a
repository deliverable first and a page second. The two wire format
specifications are in `docs/spec/` because firmware implementers read them from
a checkout, and they need stable citable URLs because third parties are invited
to implement against them. `CHANGELOG.md` is at the repository root because that
is where a contributor looks for it, and it renders because errata for a flight
computer are safety information.

## Navigation

Three groups, in the order somebody does things rather than alphabetically.

- **Build:** Start here, Bill of materials, Build guide, Firmware, Mounting,
  Ground station
- **Fly:** Preflight, Safety and rules, Reading your data, Troubleshooting,
  Flight log
- **Reference:** Reference, Schematic, Packet format, Log format, Glossary, FAQ,
  Changelog, About

The sidebar, the breadcrumb and the previous/next links all read `FLAT_NAV` in
`routes.ts`, so they cannot disagree about what the site contains. Reading order
crosses group boundaries deliberately: somebody who finishes the last page of
Build is ready for the first page of Fly.

Safety is in Fly and is also linked from the footer of every page.

## Data files

| File | Renders on |
|---|---|
| `data/bom.yaml` | Bill of materials, ground station section |
| `data/tiers.yaml` | Home, and the tier picker on the bill of materials |
| `data/glossary.yaml` | Glossary, and inline term links on every prose page |
| `data/preflight.yaml` | Preflight checklist |
| `data/troubleshooting.yaml` | Troubleshooting |
| `data/system.yaml` | The system block diagram on the homepage |
| `data/flight-phases.yaml` | The homepage hero, and the state machine table |
| `data/mechanical.yaml` | Mounting, printed parts |
| `data/suppliers.yaml` | Every "search this distributor" link |
| `data/flights.yaml` | Flight log, and the submission format it specifies |

Anything that renders in more than one place lives here rather than in prose.
Two hand-written tables drift and nothing catches it; `make data` checks that
part ids, tier ids and glossary cross-references resolve.

## Pages that cannot be finished yet

Three pages are structure waiting for evidence, and each says so in place rather
than reading as complete:

- **Reading your data** needs one real flight, with the data file published
  alongside it. Every feature of the graph is described by what causes it; none
  of it is drawn from a recorded flight, and a synthetic illustration would
  defeat the purpose of the page.
- **The build guide's Board path** needs a fabricated PCB. Nothing in it has
  been performed.
- **The flight log** is empty because nothing has flown. The submission format
  is specified anyway, so opening it to the community later is a mechanism
  change rather than a redesign.

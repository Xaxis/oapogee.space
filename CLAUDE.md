# oApogee

Open source telemetry payload for model rockets, plus the documentation that
teaches a beginner to build one. Site at **oapogee.space**, deployed on Vercel.
The GitHub repo is `Xaxis/oapogee.space` and the local checkout is
`~/Projects/oapogee.space`.

Stack conventions follow `~/Projects/nband`: Yarn workspaces monorepo, Next.js 15
App Router, React 19, Tailwind v4, TypeScript, flat-file content through
unified/remark/rehype, everything driven by `make`.

## The rule everything else follows from

**Never publish a number that has not been measured or sourced.**

The documentation is the product; the hardware is commodity. The entire value of
this project is that a beginner can follow the site and end up with a working
payload, and that collapses the moment a confident wrong number appears in it.
People spend money and fly hardware over other people's heads based on what this
site says.

In practice:

- No price, mass, range, current draw, battery life, or measured altitude
  appears unless it came from an invoice, a scale, a datasheet, or a flight.
- A missing figure is written as `TODO(verify): <what evidence would close it>`,
  never estimated. A marker with no closing condition is not a marker.
- `TODO(confirm-on-hardware)` for steps that need a physical board.
  `TODO(confirm)` for open decisions. `TODO(photo)` for image slots, describing
  what the photo must show.
- Manufacturer part numbers are asserted only when confident. Breakout board
  product numbers and supplier links are `confidence: unverified` because they
  change without notice.
- Rounded physics is not invention. "A heavier payload costs altitude" is fine.
  "It costs 31 m" needs OpenRocket output behind it.

If a change would make the site appear to know more than it does, it is wrong
even if the writing is good.

## Layout

```
content/     Prose pages, Markdown + YAML frontmatter
data/        Structured source: bom.yaml, tiers.yaml, glossary.yaml
docs/        Page map, and eventually docs/spec/ for the wire formats
apps/web/    Next.js site, one renderer of content/ and data/
tools/       check-prose, check-data, collect-todos
```

`content/` and `data/` are canonical. The site renders them and holds no content
of its own.

## Commands

```bash
make check       # everything CI runs
make check-fast  # the same without the site build
make todos       # regenerate TODO-VERIFY.md
make dev         # the site, locally
```

Run `make todos` after touching any verification marker and commit the result.
`make check-todos` fails otherwise, so a stale index cannot merge.

## Things that will bite you

**Never shorten the name to "Apogee."** Apogee Components is the dominant
retailer in model rocketry and owns that word in this hobby completely. There is
no short form: it is oApogee in headings, prose, alt text, code comments, commit
messages, and on the silkscreen. `make prose` fails on `Apogee board`,
`Apogee firmware`, and similar. The bare word is fine when it means the flight
event, which is most of the time.

**Never use em dashes, and no emoji in content.** Also enforced by `make prose`.
Use a comma, a colon, parentheses, or two sentences.

**Anything rendering in more than one place lives in `data/`, not in prose.**
Two hand-written tables drift and nothing catches it. `make data` checks that
part ids, tier ids and glossary cross-references resolve.

**`data/tiers.yaml` `scope.does_not` is load-bearing.** It is the passive payload
boundary, it renders on the homepage, and `make data` fails if the words
ejection, deploy, ignite, or pyrotechnic disappear from it. oApogee does not fire
charges, control deployment, ignite motors, or command pyrotechnics, and no page
may imply otherwise.

**A page cannot be `status: verified` while it still contains a TODO marker.**
Enforced in `make data`. `verified` means a human checked every number on real
hardware, and it is the maintainer's call alone.

**Tier metadata is in `data/tiers.yaml`, cost and mass targets are in
`data/bom.yaml`.** They were duplicated once and split deliberately. Do not add
tier names back to the BOM.

**Content lives at the repository root, the site is two levels down.** Reads go
through `apps/web/lib/repo.ts`, never `process.cwd()`, which differs between
`next dev`, `next build`, and the Vercel build container.

**There is no database.** The site is static, rendered from flat files. Supabase
was in the inherited `.env` and has been removed. Do not provision one until the
community flight log submission mechanism actually needs it.

## Environment

`.env` at repo root, never committed, documented by `.env.example`. It holds
`NEXT_PUBLIC_SITE_URL` and a Vercel CLI token, and that is all it should hold.

## Style

Second person, imperative in procedures. Direct and unhyped. Sentence case
headings. Short paragraphs, numbered steps for procedures, tables for reference.
Define jargon on first use and add the term to `data/glossary.yaml`. Comments and
prose explain *why*, especially where a non-obvious constraint drove the design.
Full rules in `CONTENT-STYLE.md`.

Open questions and disagreements with the brief go in `NOTES-FOR-WIL.md` rather
than being silently resolved.

# Notes for Wil

Running list of design concerns, disagreements with the brief, and open
questions. Per the brief, I put things here rather than silently substituting my
judgement for yours.

Newest sections at the bottom within each category. Resolved items get struck
through with the resolution, not deleted, so the reasoning survives.

---

## Blocking, needs a decision from you

### 1. The homepage hero cannot be a real flight graph yet

The brief says lead with a real annotated flight graph, not a hero illustration.
I agree completely, and it is not currently possible: no board exists, so no
flight exists.

The wrong answers are a synthetic graph that looks real, a stock photo of
someone else's rocket, and a "coming soon" page. A project whose entire pitch is
honest data cannot open with fabricated data, even prettily labelled.

**Recommendation:** the hero slot carries a plainly labelled statement of
project status and a link to `/status`, until a flight exists. If you want
something visual there sooner, the honest option is an OpenRocket simulation
output, clearly and permanently labelled as a simulation, with the real graph
replacing it the day it exists. Confirm which you want.

### 2. The high-g accelerometer is in the wrong place in the tier structure

The brief puts the high-g accelerometer at "tier 2+, optional". Tier 2 is Link,
which is defined by adding a radio.

Those two things are unrelated. The high-g part exists because a 16 g IMU
saturates during boost, and whether it saturates depends on the motor, not on
whether there is a transmitter on the board. You told me the target is A through
E now with headroom for F and G. A Solo builder flying a D or an E motor has
exactly the same saturation problem as a Link builder, and Solo is the tier
whose whole value is the logged flight profile.

**Recommendation:** make the high-g accelerometer an optional populate on every
tier, driven by motor class rather than tier, and say on the BOM page "add this
if you fly above a C". That is one extra row in the parts table and it removes a
category of silently wrong data from the cheapest build.

I have left it as the brief specifies in `data/bom.yaml` pending your call, but
the `why` text there already argues the case.

### 3. Two mechanical targets are in tension and I cannot resolve them on paper

The brief asks for under 25 g flying mass including battery, and a fit inside a
24 mm body tube.

TODO(verify) on both, but flagging the interaction now because it drives the
board outline: a 24 mm tube has an internal diameter meaningfully smaller than
24 mm once wall thickness and a coupler are accounted for, and the payload has
to contain a PCB, a cell, a buzzer, and an antenna in that cross-section. That
is achievable, but it constrains the board to a narrow strip, which pushes it
longer, which interacts with the sled length.

**What I need:** confirm which of the two is the hard constraint. If it is the
tube diameter, the mass follows from whatever fits. If it is the mass, the tube
may need to be BT-55 for the internal sled and the 24 mm case becomes
external-pod-only.

### 4. WebSerial for the ground station excludes iOS entirely

The brief asks me to confirm the approach before building it out, so: WebSerial
is not supported in Safari and is not available on iOS in any browser, because
iOS browsers all use the same underlying engine. That is a large share of the
phones at a launch.

**Recommendation:** WebSerial anyway, with the browser requirement stated
prominently rather than in a footnote, plus a documented plain serial terminal
fallback so nobody is fully locked out. The alternative, a native app per
platform, will not get installed, and a local server the user runs is a worse
first-run experience than the entire rest of the project.

Confirm and I will build it that way.

### 5. Licensing, with the tradeoff rather than a pick

You answered "docs now, kits later", which changes this materially. Here is the
tradeoff.

**Hardware, CERN-OHL-S versus CERN-OHL-P.**

`-S` is reciprocal: anyone who makes and distributes a product based on the
design must publish their modified sources. `-P` is permissive: they may not.

If oApogee stays documentation-only forever, `-P` costs nothing and maximises
adoption. Since you intend to sell kits later, `-S` is the better fit, for a
specific reason: the plausible bad outcome for a project like this is a
manufacturer taking the design, producing it at volume, and shipping it as a
closed product against which you cannot compete on price. `-S` does not prevent
that, and is not meant to, but it does require that their improvements come
back. Given the name promises openness, taking the licence that keeps
downstream open is the consistent choice.

**Cost of `-S`, stated honestly:** it makes the design harder to incorporate
into a commercial product, which cuts both ways. Some hobby vendors will avoid
it. If your goal is maximum ubiquity, `-P` gets there faster.

**Recommendation: CERN-OHL-S 2.0 for hardware.**

**Firmware, MIT versus Apache 2.0.**

Both permissive and both fine. Apache 2.0 adds an express patent grant and a
patent retaliation clause, which matters more once money is involved, and it is
the better default for anything with a commercial future. MIT is shorter and
more familiar to hobbyists.

**Recommendation: Apache 2.0**, specifically because you plan to sell. If the
firmware is going into a product you ship, the patent grant is worth the extra
paragraphs.

Note the asymmetry this creates: reciprocal hardware, permissive firmware. That
is deliberate and it is a common pattern. The board is the thing worth
protecting from a closed fork; the firmware is the thing worth having other
people embed everywhere.

**Documentation, CC BY-SA 4.0.** Straightforward, and it matches the `-S`
posture on hardware. The one thing to be aware of: CC BY-SA means a competitor
can reproduce your build guide commercially as long as they attribute and share
alike. That is the deal, and given the documentation is the product, it is worth
saying out loud before you agree to it. If that bothers you, CC BY-NC-SA exists,
but it is not an open licence by the usual definition and it would sit badly
under a name whose first letter means open.

**Recommendation: CC BY-SA 4.0**, and accept the consequence.

**Trademark, which is the part that actually protects the kit business.**
Licences do not stop someone selling a clone called oApogee. A trademark does.
Since you plan to sell, decide now whether to register the mark, and in the
meantime put a short trademark policy on the About page: the design is open, the
name is not, and clones need a different name. This is the standard open
hardware posture and it is the only one that survives contact with a
manufacturer.

I have not created any LICENSE files yet, because the brief says present the
tradeoff rather than pick. Tell me and I will add them.

---

## Disagreements with the brief, non-blocking

### 6. "Attaches to any model rocket" needs a qualifier from day one

The external pod is genuinely the differentiator and I think the brief is right
about that. But "no modification to the airframe" is not the same as "no effect
on the rocket", and a beginner will read it as the latter.

A pod strapped to the outside adds drag, adds asymmetric drag, and moves the
centre of gravity and the centre of pressure in ways that depend on where it
goes. On a small light rocket that is not a rounding error.

I have written the Safety page to say this plainly. I would go further and put
the qualifier in the Home page one-liner rather than only on Mounting, because
the one-liner is what gets quoted elsewhere.

### 7. "Build time under two hours" is a claim about people, not hardware

It is listed with the other target numbers, alongside mass and cost, and it is a
different kind of number. Mass is measurable once. Build time varies by a factor
of three across people who have all "soldered before".

**Recommendation:** publish it as a range with the sample described, for example
the median and slowest of some number of real builders, rather than a single
figure. Anything else sets a beginner up to feel slow. Marked accordingly in
`data/tiers.yaml`.

### 8. The BOM has two prices, not one, and quoting one number is misleading

A Link build is not usable without a ground station, which is a second bill of
materials with its own MCU, radio, antenna, and enclosure. Any headline "Link
costs $X" that omits it is the kind of thing that erodes trust on the first
order somebody places.

I have structured `data/bom.yaml` so the ground station is a separate block and
cannot be silently folded into a tier total. The BOM page should show both
numbers side by side.

### 9. `/status` as a first-class page

Added to the page map, flagged here because it is an addition rather than an
interpretation. Given that the project is currently paper, a reader deserves to
find out how finished it is in one click rather than by noticing that every
number is missing. It also makes `TODO-VERIFY.md` a build artifact instead of a
file somebody has to remember to update.

---

## Technical concerns to check

### 10. RP2350 errata

Before committing to RP2350 over RP2040, check the current errata list for the
silicon revision you can actually buy. There has been at least one widely
discussed input-related erratum on early RP2350 silicon with implications for
GPIO used as inputs with internal pull-downs.

TODO(verify): read the current RP2350 datasheet errata section, determine
whether any of it touches the pins oApogee uses, and record the finding. If it
does and the workaround is external components, RP2040 becomes more attractive
and the decision should be documented rather than assumed.

### 11. Battery sizing is currently backwards

The brief specifies 150 to 250 mAh. That is a reasonable guess, and it is a
guess, because the endurance requirement has not been stated.

The two numbers that actually set it are the pad wait, which at a busy club
launch can be long, and the post-landing beacon duration, which is the whole
point of the beacon. Both are requirements you can state today. Measure current
draw per flight state once hardware exists, then size the cell.

### 12. Flash density

16 MB is the reflexive choice and is probably several times more than a flight
log needs. Compute the actual figure from the record size and the adaptive log
rate once the log format spec is written, then size the part. This is a real
cost and mass line on the cheapest build.

### 13. I2C bus sharing on the Modules path

Three or four breakout boards on one I2C bus each bring their own pull-up
resistors, and in parallel those can pull the bus too hard. This is a classic
first-build failure that presents as intermittent sensor dropouts, which is
exactly the symptom a beginner will misdiagnose as a bad solder joint.

Either specify boards whose pull-ups are removable, or document the cut-trace
step, or put the sensors on SPI where practical. This needs deciding before the
Modules build guide is written, and it is in the BOM verify notes.

---

## Naming and positioning

### 14. The capital A will get mangled and that is fine

`oApogee` with a mid-word capital will be typed as `oapogee`, `OApogee`, and
`Oapogee` by users, search engines, and package managers. Prose consistency is
worth enforcing, and I have put it in `CONTENT-STYLE.md` and will enforce it in
CI. Fighting how other people type it is not worth it.

Practical follow-ups: make sure the site responds to any capitalisation of the
domain, and if you register social handles, take the lowercase forms.

### 15. SEO, agreeing with the brief and adding one thing

The brief is right that the bare word "apogee" is unwinnable and should not be
chased. The target phrases it lists are the right ones.

The addition: the highest-value search traffic for this project is not
"telemetry" queries at all, it is problem queries. "why does my altimeter read
negative", "model rocket altimeter vent hole size", "gps loses lock in rocket".
Those are low volume, extremely high intent, and nobody has written good answers
to them. The Troubleshooting page and the static ports section are the SEO
strategy, not the marketing copy. That is convenient, because they are also the
pages most worth writing well.

---

## Housekeeping

### 16. The `.env` situation

The `.env` in this repo was a verbatim copy of nband's, including that project's
Supabase service role key, database password, provisioning tokens, and
`NBAND_FUZZ_SALT`.

Checked: `.env` was never committed here, and `.gitignore` now covers it. So
nothing leaked into git history, and no rotation is required on that account.

I have replaced it with an oApogee-specific file and written `.env.example` to
document the shape. The nband values are gone from this directory.

One thing to decide: oApogee does not currently need Supabase at all. The site
is flat-file content, structured data, and static generation. The flight log is
your flights only in v1, which is a data file. **Recommendation: do not
provision a database until the community submission mechanism actually needs
one.** Adding Supabase now means a second thing to keep in sync for no current
benefit.

### 17. Technical accuracy review

I assumed you are the reviewer, and `status: verified` requires you. Worth
considering: for the radio law and the FAA sections specifically, a licensed
amateur and somebody active in a NAR or Tripoli club would each catch things
neither of us will. Those two pages are where being wrong has consequences
beyond embarrassment.

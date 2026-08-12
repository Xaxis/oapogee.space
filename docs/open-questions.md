# Open questions

Design decisions that are not settled, with the arguments on each side. Kept in
the open because a project that only publishes its conclusions is asking to be
trusted rather than checked, and because most of these are questions somebody
with more flying experience than us can answer in a sentence.

Numbers that need measuring rather than deciding are not here. Those live as
markers in the content files and are listed at
[oapogee.space/status](https://oapogee.space/status).

Settled decisions and their reasoning move to [CHANGELOG.md](../CHANGELOG.md).

## Should the high-g accelerometer be tied to a tier or to a motor class

Currently it is specified at Link and above, which is where the original design
brief put it.

That is arguably wrong. Link is defined by adding a radio, and whether a general
purpose IMU saturates during boost depends on the motor, not on whether there is
a transmitter on the board. A Solo builder flying a D or an E motor has exactly
the same saturation problem, and Solo is the tier whose entire value is the
logged flight profile.

**The alternative:** make it an optional populate at every tier, driven by motor
class, and say on the bill of materials "add this if you fly above a C". One
extra row in the parts table, and it removes a category of silently wrong data
from the cheapest build.

**Against:** it complicates the tier story, which is currently three clean steps.

## Are 25 g and a 24 mm tube both hard constraints

The design targets are under 25 g flying mass including battery, and a fit
inside a 24 mm body tube.

These interact. A 24 mm tube has an internal diameter meaningfully smaller than
24 mm once wall thickness and a coupler are accounted for, and the payload has
to contain a board, a cell, a buzzer and an antenna in that cross-section. That
is achievable, and it constrains the board to a narrow strip, which pushes it
longer, which interacts with the sled length.

If the tube diameter is the hard constraint, the mass follows from whatever
fits. If the mass is the hard constraint, the internal sled may need to be
BT-55 and the 24 mm case becomes external-pod only.

## WebSerial for the ground station excludes iOS entirely

WebSerial is not supported in Safari and is not available in any browser on iOS,
because they all use the same underlying engine. That is a large share of the
phones at a launch.

**Current plan:** WebSerial anyway, with the browser requirement stated
prominently rather than in a footnote, plus a documented plain serial terminal
fallback so nobody is fully locked out.

**Why not the alternatives:** a native app per platform will not get installed,
and a local server the user runs first is a worse first-run experience than the
entire rest of the project.

## Complementary filter or Kalman filter

A complementary filter is far simpler to implement, to explain, and to verify by
hand. On a project whose product is its documentation, that is a real argument
rather than a lazy one.

A Kalman filter is better behaved when the noise characteristics are known, and
they are not known, because nothing has flown. That may itself be the answer:
start complementary, and revisit once there is data to characterise.

## Should a bench run be a packet type rather than a flag

The telemetry format marks simulated data with a flag bit. A distinct packet
type would make it harder to ignore by accident.

**For the flag:** every field means the same thing in both cases, and it costs
nothing.

**For the type:** a receiver that forgets to check a flag publishes a bench run
as a flight, and a receiver that does not recognise a packet type rejects it
cleanly, which is the designed behaviour.

## Should the log carry block checksums

Flash bit rot and a partial page write on power loss can corrupt a record in the
middle of a file undetectably.

**Against:** a checksum interleaved with records breaks the flat-array property
that makes the log format one line to load, which is most of why it is designed
the way it is.

**A middle path:** a sidecar file of block checksums keeps both, at the cost of a
third file per flight.

## RP2350 or RP2040

RP2350 is specified. Before committing, the current errata for the silicon
revision that is actually purchasable should be read, and any item touching the
pins oApogee uses recorded here.

If an erratum does affect them and the workaround is external components, RP2040
becomes more attractive, and that decision should be written down rather than
quietly made.

## Trademark registration

The policy is published and stands either way: the design is open, the name is
not. Whether to formally register the mark is open, and it decides how
enforceable the policy is rather than what it says.

## Copyright holder

Currently "William Neeley and the oApogee contributors". If a company should
hold it instead, that is a one-line change and it is easier now than later.

## How to weigh in

Open an issue at
[github.com/Xaxis/oapogee.space/issues](https://github.com/Xaxis/oapogee.space/issues).

The most useful contributions to this page are from people who have flown enough
to know which of these questions does not matter in practice.

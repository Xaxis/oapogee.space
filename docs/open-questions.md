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

**Decided: optional populate at every tier, including Solo.**

The tiers are defined by radio capability, Solo to Link to Track, and the high-g
part has nothing to do with radio. Tying it to Link put it behind a capability
it is unrelated to and left the cheapest build, whose entire value is the logged
flight profile, as the one most likely to record a boost phase that is silently
wrong. The three clean steps survive because they were never about this part.

What the bill of materials cannot yet say is which motor makes it necessary.
That needs the IMU's configured full-scale range from its datasheet and the peak
acceleration of a loaded airframe per motor class from simulation, both of which
are open on the `high_g` part. Until they close, the part is offered everywhere
and the threshold is not stated.

## Are 25 g and a 24 mm tube both hard constraints

The design targets are under 25 g flying mass including battery, and a fit
inside a 24 mm body tube.

These interact. A 24 mm tube has an internal diameter meaningfully smaller than
24 mm once wall thickness and a coupler are accounted for, and the payload has
to contain a board, a cell, a buzzer and an antenna in that cross-section. That
is achievable, and it constrains the board to a narrow strip, which pushes it
longer, which interacts with the sled length.

**Decided: the 24 mm case is external-pod only, and the sled is BT-55.**

The geometry settled it rather than a preference, and it has settled harder
since. When this was written the board was narrow enough to sit inside a BT-50
bore of 24.13 mm with a sliver of rail on each side of the sled: a slicer will
emit that and a landing will break it. The board has grown twice since then to
give the autorouter room, and it is now wider than that bore, so a BT-50 sled is
not a thin-walled part, it is not a part.

This costs nothing, because the pod already covers the small-tube case and
covers it better. A pod needs no payload bay at all, which is the entire reason
it exists, while a sled needs a rocket that has one, and a rocket with a payload
section is a bigger rocket. Both parts are parametric, so a builder with a
different airframe changes one number and re-renders.

The mass target remains unverified and is not a constraint anything is being
designed against yet, because nothing has been weighed.

## WebSerial for the ground station excludes iOS entirely

WebSerial is not supported in Safari and is not available in any browser on iOS,
because they all use the same underlying engine. That is a large share of the
phones at a launch.

**Decided: WebSerial, with the browser requirement stated prominently and a
documented plain serial terminal fallback so nobody is locked out.**

A native app per platform will not get installed, and a local server the user
runs before their first packet is a worse first-run experience than the entire
rest of the project. Neither trade is worth iOS support that WebSerial cannot
give us anyway.

This is reversible and cheaply so: the wire format is specified independently of
any receiver, so an iOS client is something somebody else can build without
asking. That is most of why the format is a published specification.

## Complementary filter or Kalman filter

A complementary filter is far simpler to implement, to explain, and to verify by
hand. On a project whose product is its documentation, that is a real argument
rather than a lazy one.

A Kalman filter is better behaved when the noise characteristics are known, and
they are not known, because nothing has flown.

**Decided: complementary for v1, revisit when there is flight data.**

The argument is not that a Kalman filter is worse. It is that tuning one
requires noise characteristics nobody has, so choosing it now would mean
inventing covariances, and inventing numbers is the one thing this project does
not do. The firmware implements the complementary filter today. Revisit once
several flights exist to characterise the barometer and the IMU against.

## Should a bench run be a packet type rather than a flag

The telemetry format marks simulated data with a flag bit. A distinct packet
type would make it harder to ignore by accident.

**For the flag:** every field means the same thing in both cases, and it costs
nothing.

**For the type:** a receiver that forgets to check a flag publishes a bench run
as a flight, and a receiver that does not recognise a packet type rejects it
cleanly, which is the designed behaviour.

**Decided: the flag, with the conformance rule strengthened.**

A distinct type is not one type, it is a parallel type for every packet type
that can be simulated, which doubles the type space to encode a property
orthogonal to what the packet contains. There are sixteen type values and ten
are already reserved for extension.

The risk it was meant to address is real, so it is addressed directly instead: a
conforming receiver **must** display the `SIM` flag prominently, raised from
*should*. A receiver that ignores a must-level rule is not a conforming
receiver, and no packet type can save a reader from one that does.

## Should the log carry block checksums

Flash bit rot and a partial page write on power loss can corrupt a record in the
middle of a file undetectably.

**Against:** a checksum interleaved with records breaks the flat-array property
that makes the log format one line to load, which is most of why it is designed
the way it is.

**A middle path:** a sidecar file of block checksums keeps both, at the cost of a
third file per flight.

**Decided: block checksums in the manifest, not interleaved and not a third
file.**

Every flight already writes a manifest beside `flight.bin`. Putting the block
checksums there keeps the flat-array property that makes the log one line to
load, which is most of why the format is shaped the way it is, and it costs no
file that does not already exist. A reader that does not care about integrity
ignores the manifest exactly as it does today.

TODO(confirm-on-hardware): implement it in the log writer and state the block
size, which should be chosen to match the flash page size of the part actually
fitted rather than picked now.

## RP2350 or RP2040

RP2350 is specified. Before committing, the current errata for the silicon
revision that is actually purchasable should be read, and any item touching the
pins oApogee uses recorded here.

If an erratum does affect them and the workaround is external components, RP2040
becomes more attractive, and that decision should be written down rather than
quietly made.

## Arming switch: mechanical or magnetic

**Decided: mechanical, a slide or screw switch.**

The case for a reed or Hall sensor is that it arms through the wall and keeps
the enclosure sealed. That does not survive contact with this design, because
the pod is deliberately vented: it has static ports cut through it and must
have, or the barometer measures the box. Sealing was never available.

What is left is a trade of an inspectable state for one that can be changed by
anything magnetic in a range box, and arm state is exactly what an operator has
to confirm by eye at the pad. A slide switch shows its position from across a
table.

Consequence: the pod needs an aperture for it, which it does not have yet.

## Should the watchdog be enabled in flight

**Decided: yes, with the peak altitude committed to flash as it updates.**

The two arguments look symmetric and are not, once the mitigation is on the
table. A hung payload logs nothing, beacons nothing, and cannot be found. A
reset keeps the beacon alive, and a rocket you recover can have its log read
afterwards while a rocket you never find yields nothing at all. Recovery is the
higher-value function because everything else depends on it.

The cost of the reset, losing the in-memory peak, is the part that is fixable,
so it gets fixed rather than accepted as the price.

## Should the firmware know which radio regime it is in

**Decided: yes, an explicit Part 15 / Part 97 mode in the configuration.**

The safety page says station identification "has to be built into the firmware,
not remembered on the day". That is only true if the firmware knows which regime
it is operating under, so a missing callsign can be an arming error in the mode
where it is one rather than a silent omission in both.

## Can the manifest say that calibration never ran

**Decided: add a calibration flag in spec_version 2.**

All zeros in the bias fields currently means both "no calibration has been run"
and "calibration ran and found no bias", and a reader cannot tell which. That is
precisely the failure this project refuses everywhere else: a value that is
confidently wrong and indistinguishable from a real one. It waits for a version
bump because the manifest is a published format.

## Does the high-g accelerometer get health checks

**Decided: yes, the same treatment as every other sensor.**

`HIGH_G` means "present and healthy". A health module with no opinion about the
second half makes the flag a claim that nothing checks, which is worse than not
having the flag at all. The limits are expressed against the part's full-scale
range, so they land when that number does.

## Where do the sensor health limits live

**Decided: in OA_CONFIG_FIELDS, with the second table deleted.**

This looked like tidiness and was not. The configuration parser only ever knew
`OA_CONFIG_FIELDS`, and six of the nine health thresholds lived in a separate
table in `oa_health.h` that the parser had never heard of. Nothing could set
them. Every check that needed one was permanently unset, `BARO_FAULT` and
`IMU_FAULT` could never be raised, and the `baro_valid` gate in the state
machine could never fire from a real sensor fault.

Three baro thresholds were already in the configuration, which is what made the
split arbitrary rather than principled. They are all in one table now, marked
optional, because an unset limit means the check is not performed and that is a
state the firmware reports rather than refuses.

## Who clamps a GNSS altitude into the log

**Decided: core, in `oa_log_clamp_altitude_m`, called by every port.**

The log format says an altitude outside the range of an i16 of metres is clamped
at the endpoint rather than wrapped, because a wrapped value decodes as a
plausible altitude with the wrong sign. By the time a record reaches core the
narrowing has already happened, so the rule was stated as a comment in the port
header for every future port author to honour.

Ports are per-board and will be written by different people. A rule that depends
on each of them remembering it is the kind of rule this project makes mechanical
everywhere else, so core provides the conversion and the port calls it.

## Trademark registration

The policy is published and stands either way: the design is open, the name is
not.

**Decided: not registering now.** Registration costs money and buys
enforceability against a problem that does not exist yet, since nothing has been
fabricated and nobody is selling anything called oApogee. The unregistered
policy is what shapes behaviour in practice, and it is already published.

Revisit when either of two things happens: somebody ships a product under the
name, or this project starts selling hardware itself. Both are visible events
rather than a date, which is why this is a trigger and not a deadline.

## Copyright holder

**Decided: "William Neeley and the oApogee contributors", with no copyright
assignment and no contributor licence agreement.** This is what `LICENSE`
already says.

Contributors keep their copyright and licence their work under the three
licences the repository uses. That makes relicensing later effectively
impossible without asking everyone, which is a real cost and the intended one: a
project whose first letter means open should not keep the option of quietly
closing.

If a company is ever formed to hold it, that is a one-line change plus the
agreement of anyone who has contributed by then, and it gets harder with every
contributor rather than easier.

## A payload identifier in the telemetry header

**Open.** The header is eight bytes and carries no way to tell two payloads
apart. The log written to flash carries a stable device identity, derived from
the microcontroller's unique number, but that is only readable after the flight.

For adding one: several payloads on one field is not an exotic case, it is a
club launch. Without an identifier a ground station cannot separate two
airborne payloads, sequence numbers appear to skip, and the link quality figure
becomes meaningless in exactly the situation where somebody most wants it. The
current answer, agree different frequencies beforehand, works right up until
somebody forgets.

Against: every byte is paid for on every packet, by every user, forever, to
solve a problem most flights do not have. A one byte identifier is a 12 percent
increase on the header and collides among any 256 payloads that meet, so it
needs a coordination scheme to be reliable, and a scheme nobody administers is
one people get wrong. A four byte identifier does not collide in practice and
is a real cost to airtime and therefore to range.

Worth noting that the alternatives are not free either. Filtering by frequency
pushes the coordination to the humans, which is where model rocketry already
puts frequency coordination, and it costs nothing on the air.

This one is genuinely undecided and it should be decided by somebody who has
run a ground station at a busy launch, not from a desk.

## The radio's RF front end

**Open, and it is the single thing between this board and a fabricator.**

The SX1262's transmit output and its receive input are different pins, and
between them and one antenna the reference design puts an RF switch, a
differential to single ended network on the receive side, and a matching and
harmonic filter network on the transmit side. Roughly fourteen components.

The topology is published. Figure 14-2 of the Semtech datasheet shows every
part and how they connect, including that the switch is a PE4259 and that the
radio drives it from DIO2. That much is transcribed and settled.

**The values are not published.** No inductance or capacitance is printed for
any component in that matching network in either revision of the datasheet, and
the datasheet says why in as many words: "The application schematics presented
here are for information only. Always refer to the latest reference designs
posted on www.semtech.com." Matching is band specific, so a network for 868 MHz
is not a network for 915 MHz.

Three ways forward, none of them free:

- **Take the values from Semtech's reference design files** for the band. This
  is the intended path and it is a download, not a derivation. It has not been
  done because it needs a Semtech account.
- **Design the matching network** from the impedances in the datasheet. This is
  real RF work and it needs a vector network analyser to confirm, which is not a
  tool most people building this project own.
- **Use a module with the front end already on it**, which is what the Modules
  path does and part of why that path exists.

Until one of those happens this board is not orderable, and this project will
not publish Gerbers for it. A payload whose transmitter is matched to a guess
would pass every check here, look correct on the bench at arm's length, and lose
somebody's rocket.

## How to weigh in

Open an issue at
[github.com/Xaxis/oapogee.space/issues](https://github.com/Xaxis/oapogee.space/issues).

The most useful contributions to this page are from people who have flown enough
to know which of these questions does not matter in practice.

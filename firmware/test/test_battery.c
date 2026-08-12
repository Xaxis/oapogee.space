/*
 * Conformance tests for oa_battery.
 *
 * The battery byte is one byte in every STATUS and FLIGHT packet, and
 * docs/spec/telemetry-packet.md defines it by the receiver's expression:
 * battery_volts = 2.5 + batt / 100, range 2.50 V to 5.05 V in 10 mV steps.
 * These tests are written against that expression rather than against the
 * constants in oa_battery.h, so that a change to either one has to be a
 * deliberate change to both.
 *
 * The whole encodable range is checked exhaustively. There are 256 codes and
 * 2551 millivolt inputs, so an exhaustive test costs nothing and rules out the
 * entire class of off by one scaling bugs, which is the class most likely to
 * produce a battery reading that is wrong and plausible.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware, and no
 * cell has been measured.
 */

#include <assert.h>
#include <stdio.h>

#include "oapogee/oa_battery.h"
#include "oapogee/oa_config.h"

/* CLAIM: "One byte, battery_volts = 2.5 + batt / 100."
 *
 * In the integer units this firmware uses throughout, that is
 * millivolts = 2500 + 10 * batt. Checked for all 256 codes against the
 * arithmetic written out longhand, not against the named constants, because
 * the named constants are what would be wrong. */
static void test_decode_matches_the_spec_expression(void)
{
    int code;

    for (code = 0; code <= 255; code++) {
        assert(oa_battery_decode_mv((uint8_t)code) == 2500 + (10 * code));
    }
}

/* CLAIM: "Range 2.50 V to 5.05 V in 10 mV steps." The endpoints are the two
 * numbers a receiver author will check first. */
static void test_range_endpoints(void)
{
    assert(oa_battery_decode_mv(0u) == 2500);
    assert(oa_battery_decode_mv(255u) == 5050);
    assert(oa_battery_decode_mv(1u) - oa_battery_decode_mv(0u) == 10);
}

/* CLAIM, from oa_battery.h: "The encode and decode functions are exact
 * inverses on every representable value and the conformance test checks all
 * 256 of them." */
static void test_encode_decode_round_trip_on_every_code(void)
{
    int code;

    for (code = 0; code <= 255; code++) {
        assert(oa_battery_encode_mv(oa_battery_decode_mv((uint8_t)code)) == (uint8_t)code);
    }
}

/* CLAIM: "Below OA_BATTERY_MIN_MV encodes as 0 and above OA_BATTERY_MAX_MV
 * encodes as 255. Clamping rather than failing, because a packet that is not
 * sent carries no information at all."
 *
 * The extreme inputs are here because a reading far outside the range is what
 * a disconnected divider or an unconfigured ADC produces, and that must clamp
 * rather than wrap into a plausible voltage. */
static void test_both_clamps(void)
{
    assert(oa_battery_encode_mv(2499) == 0u);
    assert(oa_battery_encode_mv(2500) == 0u);
    assert(oa_battery_encode_mv(0) == 0u);
    assert(oa_battery_encode_mv(-1) == 0u);
    assert(oa_battery_encode_mv(INT32_MIN) == 0u);

    assert(oa_battery_encode_mv(5050) == 255u);
    assert(oa_battery_encode_mv(5051) == 255u);
    assert(oa_battery_encode_mv(100000) == 255u);
    assert(oa_battery_encode_mv(INT32_MAX) == 255u);
}

/* CLAIM, from oa_battery.h: "Rounds to nearest rather than truncating, so that
 * a decoded value is never more than 5 mV from the reading and the error is
 * centred rather than always pessimistic."
 *
 * Both halves are checked: the bound across the whole range, and the specific
 * behaviour at the midpoint of a step, which is where truncation and rounding
 * differ and where an implementation that got it wrong would still pass a
 * round trip test on the exact step values. */
static void test_rounds_to_nearest(void)
{
    int32_t mv;

    assert(oa_battery_encode_mv(2504) == 0u);
    assert(oa_battery_encode_mv(2505) == 1u);
    assert(oa_battery_encode_mv(2506) == 1u);
    assert(oa_battery_encode_mv(2509) == 1u);
    assert(oa_battery_encode_mv(2510) == 1u);

    for (mv = 2500; mv <= 5050; mv++) {
        int32_t decoded = oa_battery_decode_mv(oa_battery_encode_mv(mv));
        int32_t error   = decoded - mv;

        if (error < 0) {
            error = -error;
        }

        assert(error <= 5);
    }
}

/* CLAIM, from oa_battery.h: "*out_unset is set true when batt_low_mv is not
 * configured, and the function returns false. A caller must check it: an
 * unconfigured threshold reported as 'not low' would look identical to a
 * healthy cell, and the flag would simply never appear on any flight."
 *
 * This is the whole reason LOW_BATT is not a guess. The threshold depends on a
 * measured discharge curve and measured sag under transmit, neither of which
 * exists, so a fresh configuration must report the absence rather than an
 * answer. */
static void test_is_low_reports_an_unconfigured_threshold(void)
{
    oa_config_t cfg;
    bool        unset = false;

    oa_config_init(&cfg);

    assert(oa_battery_is_low(&cfg, 3000, &unset) == false);
    assert(unset == true);

    /* A NULL configuration is the same answer: the threshold is not known. It
     * is not "battery fine". */
    unset = false;
    assert(oa_battery_is_low(NULL, 3000, &unset) == false);
    assert(unset == true);
}

/* CLAIM: LOW_BATT means "Battery below the low threshold", from the flags
 * table in docs/spec/telemetry-packet.md. Below, so a reading exactly at the
 * threshold is not low. */
static void test_is_low_compares_strictly_below(void)
{
    oa_config_t cfg;
    bool        unset = true;

    oa_config_init(&cfg);

    /* 3600 mV is not a measurement and is not a default. It is an arbitrary
     * value supplied by this test to check the comparison, in the same way a
     * flight would supply one measured on real hardware. No threshold ships in
     * the firmware. */
    assert(oa_config_set(&cfg, OA_CFG_BATT_LOW_MV, 3600) == OA_OK);

    assert(oa_battery_is_low(&cfg, 3599, &unset) == true);
    assert(unset == false);

    assert(oa_battery_is_low(&cfg, 3600, &unset) == false);
    assert(unset == false);

    assert(oa_battery_is_low(&cfg, 3601, &unset) == false);
    assert(unset == false);
}

int main(void)
{
    test_decode_matches_the_spec_expression();
    test_range_endpoints();
    test_encode_decode_round_trip_on_every_code();
    test_both_clamps();
    test_rounds_to_nearest();
    test_is_low_reports_an_unconfigured_threshold();
    test_is_low_compares_strictly_below();

    printf("test_battery: all checks passed\n");
    return 0;
}

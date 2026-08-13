/*
 * Conformance tests for oa_health.
 *
 * Two specifications meet in this module. From the flags table in
 * docs/spec/telemetry-packet.md: BARO_FAULT means "the barometer has failed or
 * is returning implausible values", IMU_FAULT means the same for the IMU, and
 * "fault flags report an instrument that stopped working. They never cause a
 * field to be omitted, because omission would change the packet length." From
 * oa_health.h: a check whose threshold is unset is not performed, and the output
 * says which checks were not performed.
 *
 * The second of those is what most of this file is about. A health monitor that
 * reported "no fault" when it had not looked would be worse than no health
 * monitor, because somebody would believe it, so the tests check the unset path
 * as carefully as the fault path.
 *
 * EVERY THRESHOLD IN THIS FILE IS SUPPLIED BY THE TEST. None of them is a
 * measurement, none is a proposal, and none of them ships. They exist to drive
 * the comparisons, in the same way a flight would supply numbers measured on
 * real hardware. Nothing has been measured, no sensor noise floor has been
 * characterised, and no sensor has ever failed in front of this code.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_config.h"
#include "oapogee/oa_flags.h"
#include "oapogee/oa_health.h"

/* ---------------------------------------------------------------------------
 * Fixtures.
 * ------------------------------------------------------------------------ */

/* A barometer band wide enough that the range check is not what fires in tests
 * about something else. The endpoints are the ones the packet format can carry,
 * from docs/spec/telemetry-packet.md: the offset u16 covers 50000 to 115535 Pa.
 * They are used here because they are stated in a specification, not because
 * anyone has measured a barometer. */
static void cfg_with_band(oa_config_t *cfg)
{
    oa_config_init(cfg);
    assert(oa_config_set(cfg, OA_CFG_BARO_PLAUSIBLE_MIN_PA, 50000) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_BARO_PLAUSIBLE_MAX_PA, 115535) == OA_OK);
}

static void input_init(oa_health_input_t *in, uint32_t t_ms, int32_t pressure_pa)
{
    memset(in, 0, sizeof *in);
    in->t_ms        = t_ms;
    in->baro_ok     = true;
    in->pressure_pa = pressure_pa;
    in->imu_ok      = true;
}

/* ---------------------------------------------------------------------------
 * Limits.
 * ------------------------------------------------------------------------ */

/* CLAIM, from oa_health.h: "Every limit OA_UNSET. This is the only correct
 * starting state and it is the state a payload with no measurements is in, which
 * is all of them." */
static void test_limits_start_unset(void)
{
    oa_health_limits_t limits;
    int                i;

    memset(&limits, 0x7F, sizeof limits);
    oa_health_limits_init(&limits);

    for (i = 0; i < (int)OA_HEALTH_LIM_COUNT; i++) {
        oa_tunable_t v = 0;

        assert(oa_health_limit_get(&limits, (oa_health_limit_t)i, &v) == OA_OK);
        assert(v == OA_UNSET);
        assert(OA_IS_SET(v) == false);
    }

    oa_health_limits_init(NULL);
}

/* CLAIM: the table carries a name, a unit and the measurement that would close
 * each limit, "so a payload can say which number is missing and what would
 * settle it rather than just refusing to check". Every entry has all three, and
 * an index that is not a limit gets NULL rather than a plausible string. */
static void test_limit_metadata(void)
{
    int i;

    for (i = 0; i < (int)OA_HEALTH_LIM_COUNT; i++) {
        const oa_health_limit_t limit = (oa_health_limit_t)i;
        int                     j;

        assert(oa_health_limit_name(limit) != NULL);
        assert(oa_health_limit_unit(limit) != NULL);
        assert(oa_health_limit_why(limit) != NULL);

        assert(oa_health_limit_name(limit)[0] != '\0');
        assert(oa_health_limit_unit(limit)[0] != '\0');

        /* The reason has to be a sentence someone can act on, not a label. Two
         * of the six say "As above" and point at the entry before them, which is
         * still a sentence and still names a measurement. */
        {
            const char  *why = oa_health_limit_why(limit);
            const size_t n   = strlen(why);

            assert(n > 15u);
            assert(why[n - 1u] == '.');
        }

        for (j = 0; j < i; j++) {
            assert(strcmp(oa_health_limit_name(limit),
                          oa_health_limit_name((oa_health_limit_t)j)) != 0);
        }
    }

    assert(oa_health_limit_name((oa_health_limit_t)OA_HEALTH_LIM_COUNT) == NULL);
    assert(oa_health_limit_unit((oa_health_limit_t)-1) == NULL);
    assert(oa_health_limit_why((oa_health_limit_t)999) == NULL);

    {
        oa_health_limits_t limits;
        oa_tunable_t       v = 0;

        oa_health_limits_init(&limits);
        assert(oa_health_limit_get(NULL, OA_HEALTH_LIM_BARO_STALE_MS, &v) == OA_ERR_NULL);
        assert(oa_health_limit_get(&limits, OA_HEALTH_LIM_BARO_STALE_MS, NULL) == OA_ERR_NULL);
        assert(oa_health_limit_get(&limits, (oa_health_limit_t)OA_HEALTH_LIM_COUNT, &v) ==
               OA_ERR_RANGE);
    }
}

/* CLAIM: "A stuck count below two would fault every sensor on its first sample,
 * and a negative interval or magnitude is not a quantity. Checked here, once, at
 * startup... An unset limit is never an error: unset means the check is not
 * performed, which is a state this firmware is entirely comfortable with." */
static void test_limits_check(void)
{
    oa_health_limits_t limits;
    oa_health_limit_t  offender = OA_HEALTH_LIM_COUNT;

    /* Everything unset passes. This is the state of every payload today. */
    oa_health_limits_init(&limits);
    assert(oa_health_limits_check(&limits, &offender) == OA_OK);

    /* A stuck count of one calls the first reading of any run a repeat. */
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 1;
    assert(oa_health_limits_check(&limits, &offender) == OA_ERR_RANGE);
    assert(offender == OA_HEALTH_LIM_BARO_STUCK_SAMPLES);

    oa_health_limits_init(&limits);
    limits.imu_stuck_samples = 0;
    assert(oa_health_limits_check(&limits, &offender) == OA_ERR_RANGE);
    assert(offender == OA_HEALTH_LIM_IMU_STUCK_SAMPLES);

    /* Two is the smallest count that means anything. */
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 2;
    limits.imu_stuck_samples  = 2;
    assert(oa_health_limits_check(&limits, &offender) == OA_OK);

    /* Negative intervals and magnitudes. */
    oa_health_limits_init(&limits);
    limits.baro_stale_ms = -1;
    assert(oa_health_limits_check(&limits, &offender) == OA_ERR_RANGE);
    assert(offender == OA_HEALTH_LIM_BARO_STALE_MS);

    oa_health_limits_init(&limits);
    limits.imu_accel_max_mg = -1;
    assert(oa_health_limits_check(&limits, &offender) == OA_ERR_RANGE);
    assert(offender == OA_HEALTH_LIM_IMU_ACCEL_MAX_MG);

    /* The FIRST offending limit, in table order, so the one reported is the one
     * an operator reading the list from the top comes to first. */
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 1;
    limits.imu_gyro_max_cdps  = -5;
    assert(oa_health_limits_check(&limits, &offender) == OA_ERR_RANGE);
    assert(offender == OA_HEALTH_LIM_BARO_STUCK_SAMPLES);

    assert(oa_health_limits_check(NULL, &offender) == OA_ERR_NULL);
    assert(oa_health_limits_check(&limits, NULL) == OA_ERR_RANGE);
}

/* CLAIM: "Short name of a single check bit... NULL if check is not exactly one
 * defined bit, under the same rule as oa_flag_name: a caller must not be able to
 * print something that looks like a check name for a value that is not one." */
static void test_check_names(void)
{
    static const oa_health_check_t all[] = {
        OA_HEALTH_CHECK_BARO_RANGE,      OA_HEALTH_CHECK_BARO_RATE,
        OA_HEALTH_CHECK_BARO_STUCK,      OA_HEALTH_CHECK_BARO_STALE,
        OA_HEALTH_CHECK_IMU_ACCEL_RANGE, OA_HEALTH_CHECK_IMU_GYRO_RANGE,
        OA_HEALTH_CHECK_IMU_STUCK,       OA_HEALTH_CHECK_IMU_STALE
    };
    size_t i;
    size_t j;

    for (i = 0u; i < (sizeof all / sizeof all[0]); i++) {
        assert(oa_health_check_name(all[i]) != NULL);
        for (j = 0u; j < i; j++) {
            assert(strcmp(oa_health_check_name(all[i]), oa_health_check_name(all[j])) != 0);
        }
    }

    assert(oa_health_check_name((oa_health_check_t)0) == NULL);
    assert(oa_health_check_name((oa_health_check_t)(OA_HEALTH_CHECK_BARO_RANGE |
                                                   OA_HEALTH_CHECK_BARO_RATE)) == NULL);
    assert(oa_health_check_name((oa_health_check_t)(1u << 8)) == NULL);
    assert(oa_health_check_name((oa_health_check_t)OA_HEALTH_CHECKS_BARO) == NULL);

    /* The two masks together cover the eight checks and nothing else. */
    assert((OA_HEALTH_CHECKS_BARO & OA_HEALTH_CHECKS_IMU) == 0u);
    assert((OA_HEALTH_CHECKS_BARO | OA_HEALTH_CHECKS_IMU) == 0xFFu);
}

/* ---------------------------------------------------------------------------
 * The sample path.
 * ------------------------------------------------------------------------ */

/* CLAIM: "A check whose threshold is unset is not performed, and the output says
 * which checks were not performed, in checks_unset. That distinction is the
 * point: a health monitor that reported 'no fault' when it had not looked would
 * be worse than no health monitor, because somebody would believe it."
 *
 * With nothing measured, which is the state of this project today, all eight
 * checks report themselves as not performed, and faults is empty because nothing
 * looked, not because everything is well. */
static void test_nothing_measured_means_nothing_checked(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    oa_config_init(&cfg); /* Band unset too: it is a tunable like the rest. */
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 10u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);

    assert(out.faults == 0u);
    assert(out.flags == 0u);
    assert(out.checks_unset == (OA_HEALTH_CHECKS_BARO | OA_HEALTH_CHECKS_IMU));

    /* And this is exactly the state in which an empty `faults` must not be read
     * as a healthy payload. The header says so, and the bits are how a caller
     * finds out. */
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_RANGE) != 0u);
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_IMU_STALE) != 0u);
}

/* CLAIM: "Feeds oa_state_input_t.baro_valid and the fusion filter's baro_valid.
 * ... False before the first successful read, because 'no measurement yet' is
 * not the same thing as 'measured and fine'." */
static void test_validity_is_false_before_the_first_reading(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 0u, 101325);
    in.baro_ok = false;
    in.imu_ok  = false;

    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.baro_valid == false);
    assert(out.imu_valid == false);
    assert(out.faults == 0u); /* Nothing has failed. Nothing has been read. */

    /* One successful read of each, and both become valid. */
    input_init(&in, 10u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.baro_valid == true);
    assert(out.imu_valid == true);
}

/* CLAIM: "stuck: the same reading, repeated. A live sensor has noise."
 *
 * The count is a count of identical readings including the first, so a limit of
 * three fires on the third identical reading and not the second or the fourth.
 * The off by one here would be invisible on a plot and would make a measured
 * threshold mean something other than what was measured. */
static void test_baro_stuck(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 3;
    assert(oa_health_limits_check(&limits, NULL) == OA_OK);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 10u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_STUCK) == 0u);

    input_init(&in, 20u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    input_init(&in, 30u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_STUCK);
    assert((out.flags & (uint8_t)OA_FLAG_BARO_FAULT) != 0u);
    assert(out.baro_valid == false);

    /* One pascal of movement is a live sensor. The run restarts at one. */
    input_init(&in, 40u, 101326);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert(out.flags == 0u);
    assert(out.baro_valid == true);

    /* And a failed read does not extend the run, because there was no reading to
     * repeat. Two identical readings either side of a dropout are two, not
     * three. */
    input_init(&in, 50u, 101326);
    in.baro_ok = false;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    input_init(&in, 60u, 101326);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    input_init(&in, 70u, 101326);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_STUCK);
}

/* CLAIM: "As above, for all six IMU axes at once."
 *
 * All six together, not any one of them: a single axis can legitimately read the
 * same value twice while the others move, and a payload lying still on the pad
 * would fault on the first rule. */
static void test_imu_stuck_needs_all_six_axes(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;
    int                i;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.imu_stuck_samples = 2;
    assert(oa_health_init(&health, 0u) == OA_OK);

    /* One axis repeating while another moves is not stuck. */
    for (i = 0; i < 5; i++) {
        input_init(&in, (uint32_t)(10 * (i + 1)), 101325 + i);
        in.accel_mg[0]  = 100;                  /* Repeats. */
        in.accel_mg[1]  = (int16_t)(200 + i);   /* Moves. */
        in.gyro_cdps[2] = (int16_t)(-i);
        assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
        assert((out.faults & (uint32_t)OA_HEALTH_CHECK_IMU_STUCK) == 0u);
    }

    /* All six identical is the thing that does not happen to a live part. */
    input_init(&in, 100u, 101400);
    in.accel_mg[0] = 100;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert((out.faults & (uint32_t)OA_HEALTH_CHECK_IMU_STUCK) == 0u);

    input_init(&in, 110u, 101401);
    in.accel_mg[0] = 100;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_IMU_STUCK);
    assert((out.flags & (uint8_t)OA_FLAG_IMU_FAULT) != 0u);
    assert((out.flags & (uint8_t)OA_FLAG_BARO_FAULT) == 0u);
    assert(out.imu_valid == false);
    assert(out.baro_valid == true);
}

/* CLAIM: "out of range: a reading outside the band the part can physically
 * produce", using baro_plausible_min_pa and baro_plausible_max_pa from
 * oa_config_t through oa_baro_is_plausible. One tunable, one home. */
static void test_baro_range(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 10u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_RANGE) == 0u);

    /* Well below the band. A barometer that has lost its bus can return zero,
     * and zero is not weather. */
    input_init(&in, 20u, 0);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_RANGE);
    assert((out.flags & (uint8_t)OA_FLAG_BARO_FAULT) != 0u);
    assert(out.baro_valid == false);

    /* Well above it. */
    input_init(&in, 30u, 200000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_RANGE);

    /* An unconfigured band is reported as a check that was not performed, and it
     * must not read as a sensor that is fine. */
    {
        oa_config_t bare;

        oa_config_init(&bare);
        input_init(&in, 40u, 0);
        assert(oa_health_step(&health, &bare, &limits, &in, &out) == OA_OK);
        assert((out.faults & (uint32_t)OA_HEALTH_CHECK_BARO_RANGE) == 0u);
        assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_RANGE) != 0u);
    }
}

/* CLAIM, from oa_config.h on baro_max_rate_pa_s: "A jump detector for a
 * barometer that starts returning garbage without leaving its plausible band.
 * Unset means no rate check is applied." */
static void test_baro_rate(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    oa_health_limits_init(&limits);

    /* Unset: not performed, and said so. */
    cfg_with_band(&cfg);
    assert(oa_health_init(&health, 0u) == OA_OK);
    input_init(&in, 0u, 100000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_RATE) != 0u);
    input_init(&in, 1000u, 60000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* Set: a step larger than the ceiling over the elapsed interval fires. */
    cfg_with_band(&cfg);
    assert(oa_config_set(&cfg, OA_CFG_BARO_MAX_RATE_PA_S, 1000) == OA_OK);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 0u, 100000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u); /* The first reading is the reference. */
    assert((out.checks_unset & (uint32_t)OA_HEALTH_CHECK_BARO_RATE) == 0u);

    /* Exactly at the ceiling is not exceeding it. */
    input_init(&in, 1000u, 101000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* One pascal past it, over the same interval, is. */
    input_init(&in, 2000u, 102001);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_RATE);
    assert((out.flags & (uint8_t)OA_FLAG_BARO_FAULT) != 0u);

    /* Downward is the same magnitude of jump. */
    input_init(&in, 3000u, 101000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_RATE);
}

/* CLAIM, from the oa_health_t comment on baro_rate_ref_pa: the rate check's
 * reference is "held separately from baro_last_pa because a reading rejected by
 * the range check must not become the point the next one is measured against:
 * one spike would otherwise trip the rate check twice, once going out and once
 * coming back."
 *
 * This is the test that tells the two designs apart. With the reference held
 * correctly the return to a normal pressure is a zero change from the last good
 * reading; with the reference poisoned it is a jump of ninety kilopascals. */
static void test_a_rejected_reading_does_not_become_the_rate_reference(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    assert(oa_config_set(&cfg, OA_CFG_BARO_MAX_RATE_PA_S, 1000) == OA_OK);
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 0u, 100000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* Out of band. The range check fires, once. */
    input_init(&in, 1000u, 10000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_BARO_RANGE);

    /* Back to where it was. Nothing has changed since the last good reading, so
     * nothing fires. A poisoned reference would report a jump here. */
    input_init(&in, 2000u, 100000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert(out.baro_valid == true);
}

/* CLAIM: two samples inside the same millisecond do not produce a rate fault.
 * The interval is not resolvable at the clock's resolution, and dividing a real
 * pressure change by an interval of zero would call any change at all a jump.
 * A sample rate above 1 kHz is not proposed here; it is simply not this module's
 * place to fault a payload for having one. */
static void test_rate_ignores_a_zero_interval(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    assert(oa_config_set(&cfg, OA_CFG_BARO_MAX_RATE_PA_S, 1000) == OA_OK);
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 500u, 100000);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);

    input_init(&in, 500u, 100500);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* And the reference stayed put, so the pair is measured against the next
     * reading that has an interval: 100500 is 500 Pa from the reference over a
     * full second, which is inside the ceiling. */
    input_init(&in, 1500u, 100500);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
}

/* CLAIM: "stale: no successful read for too long", and, from the limit table,
 * "how long without a successful read before the barometer is declared dead". */
static void test_staleness(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.baro_stale_ms = 100;
    limits.imu_stale_ms  = 100;
    assert(oa_health_limits_check(&limits, NULL) == OA_OK);

    /* The clocks start at oa_health_init, not at zero, so a payload that has
     * been powered for a while does not see its first sample arrive stale. */
    assert(oa_health_init(&health, 100000u) == OA_OK);

    input_init(&in, 100050u, 101325);
    in.baro_ok = false;
    in.imu_ok  = false;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* Exactly at the limit is not yet too long. */
    input_init(&in, 100100u, 101325);
    in.baro_ok = false;
    in.imu_ok  = false;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    input_init(&in, 100101u, 101325);
    in.baro_ok = false;
    in.imu_ok  = false;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults ==
           ((uint32_t)OA_HEALTH_CHECK_BARO_STALE | (uint32_t)OA_HEALTH_CHECK_IMU_STALE));
    assert((out.flags & (uint8_t)OA_FLAG_BARO_FAULT) != 0u);
    assert((out.flags & (uint8_t)OA_FLAG_IMU_FAULT) != 0u);

    /* One successful read clears it. A driver that recovers is not a dead part. */
    input_init(&in, 100200u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert(out.flags == 0u);
}

/* CLAIM, from the imu_accel_max_mg entry: "Note that a reading AT full scale is
 * saturation and not a fault: the log format says a saturated axis reads as a
 * flat plateau, and this limit exists to catch a part reporting past its own
 * range."
 *
 * Strictly greater than, therefore, and on the magnitude, so a negative axis is
 * judged the same as a positive one. INT16_MIN is included because it is the
 * value a saturated axis actually returns and because negating it in an int16
 * would overflow. */
static void test_imu_ranges(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.imu_accel_max_mg  = 16000;
    limits.imu_gyro_max_cdps = 20000;
    assert(oa_health_limits_check(&limits, NULL) == OA_OK);
    assert(oa_health_init(&health, 0u) == OA_OK);

    /* At full scale, positive and negative. Saturation, not a fault. */
    input_init(&in, 10u, 101325);
    in.accel_mg[0]  = 16000;
    in.accel_mg[1]  = -16000;
    in.gyro_cdps[2] = 20000;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
    assert(out.imu_valid == true);

    /* One count past it, on any axis. */
    input_init(&in, 20u, 101326);
    in.accel_mg[2] = 16001;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE);
    assert((out.flags & (uint8_t)OA_FLAG_IMU_FAULT) != 0u);

    input_init(&in, 30u, 101327);
    in.accel_mg[1] = INT16_MIN;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE);

    input_init(&in, 40u, 101328);
    in.gyro_cdps[0] = -20001;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == (uint32_t)OA_HEALTH_CHECK_IMU_GYRO_RANGE);

    /* Both at once, both reported, one flag. */
    input_init(&in, 50u, 101329);
    in.accel_mg[0]  = 30000;
    in.gyro_cdps[1] = 30000;
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == ((uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE |
                          (uint32_t)OA_HEALTH_CHECK_IMU_GYRO_RANGE));
    assert(out.flags == (uint8_t)OA_FLAG_IMU_FAULT);
}

/* CLAIM, from docs/spec/telemetry-packet.md: "Fault flags report an instrument
 * that stopped working. They never cause a field to be omitted, because omission
 * would change the packet length. A faulted sensor's fields carry the last value
 * read, and the flag is what tells you not to trust them."
 *
 * The structural half of that claim is enforced by oa_health_output_t having no
 * member that could mean "skip this field", which is a property of the header
 * rather than something a test can execute. The half a test can check is that
 * this module never touches the caller's sample, so the last value read is still
 * there to be packed into a full-length record and a full-length packet.
 *
 * It is also the check that oa_health_step "never writes through any other
 * pointer", which is what makes it safe to call from the sample loop. */
static void test_a_fault_never_disturbs_the_sample(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_input_t  before;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 2;
    limits.imu_stuck_samples  = 2;
    limits.imu_accel_max_mg   = 16000;
    assert(oa_health_init(&health, 0u) == OA_OK);

    input_init(&in, 10u, 101325);
    in.accel_mg[0] = 20000; /* Out of range. */
    memcpy(&before, &in, sizeof before);

    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults != 0u);
    assert(memcmp(&before, &in, sizeof before) == 0);

    /* The same reading again, now also stuck and still out of range. Still
     * untouched, and the fault is still only ever a flag. */
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(memcmp(&before, &in, sizeof before) == 0);
    assert((out.flags & (uint8_t)OA_FLAG_IMU_FAULT) != 0u);

    /* And the flags byte carries the two fault bits and nothing else. The other
     * six bits of that byte belong to the caller, and this module knows nothing
     * about them. */
    assert((out.flags & (uint8_t) ~((unsigned)OA_FLAG_BARO_FAULT |
                                    (unsigned)OA_FLAG_IMU_FAULT)) == 0u);
}

/* CLAIM: "Samples on which each fault flag was raised, since init. Reported in
 * the flight summary so that a flag which appeared for two samples at transonic
 * can be told apart from one that was set for the whole flight."
 *
 * Counted per sample and not per check, so a sample on which two barometer
 * checks fire counts once. */
static void test_fault_counters(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;
    int                i;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.baro_stuck_samples = 2;
    limits.baro_stale_ms      = 1;
    assert(oa_health_init(&health, 0u) == OA_OK);

    assert(oa_health_samples(&health) == 0u);
    assert(oa_health_baro_fault_samples(&health) == 0u);
    assert(oa_health_imu_fault_samples(&health) == 0u);

    /* Four identical readings. The first starts the run, the three after it are
     * stuck. The staleness limit of one millisecond never fires, because every
     * one of these reads succeeded and a successful read is never stale, which
     * is worth having in the same test: the counter must count faulted samples
     * and not checks that could have fired. */
    for (i = 0; i < 4; i++) {
        input_init(&in, (uint32_t)(1000 * (i + 1)), 101325);
        assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    }

    assert(oa_health_samples(&health) == 4u);
    assert(oa_health_baro_fault_samples(&health) == 3u);
    assert(oa_health_imu_fault_samples(&health) == 0u);

    /* The counters only go up. There is no way to clear one, because a payload
     * that could clear its own fault history could produce a flight summary that
     * disagrees with the flags in its own log records. */
    input_init(&in, 5000u, 101400);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(oa_health_baro_fault_samples(&health) == 3u);
    assert(oa_health_samples(&health) == 5u);

    assert(oa_health_samples(NULL) == 0u);
    assert(oa_health_baro_fault_samples(NULL) == 0u);
    assert(oa_health_imu_fault_samples(NULL) == 0u);
}

/* CLAIM: "Returns OA_OK or OA_ERR_NULL", and every context this firmware uses
 * starts as a zeroed struct in a caller's static storage, so a NULL has to be
 * reported rather than dereferenced. */
static void test_null_arguments(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    assert(oa_health_init(&health, 0u) == OA_OK);
    input_init(&in, 10u, 101325);

    assert(oa_health_init(NULL, 0u) == OA_ERR_NULL);
    assert(oa_health_step(NULL, &cfg, &limits, &in, &out) == OA_ERR_NULL);
    assert(oa_health_step(&health, NULL, &limits, &in, &out) == OA_ERR_NULL);
    assert(oa_health_step(&health, &cfg, NULL, &in, &out) == OA_ERR_NULL);
    assert(oa_health_step(&health, &cfg, &limits, NULL, &out) == OA_ERR_NULL);
    assert(oa_health_step(&health, &cfg, &limits, &in, NULL) == OA_ERR_NULL);
}

/* CLAIM: the millisecond clock is a u32 and the packet spec puts its range at
 * about 49 days. A payload left powered long enough to wrap it must not fault
 * every instrument at the wrap, which is what a signed interval or a plain
 * subtraction into a wider type would do. */
static void test_the_clock_may_wrap(void)
{
    oa_config_t        cfg;
    oa_health_limits_t limits;
    oa_health_t        health;
    oa_health_input_t  in;
    oa_health_output_t out;

    cfg_with_band(&cfg);
    oa_health_limits_init(&limits);
    limits.baro_stale_ms = 100;
    limits.imu_stale_ms  = 100;

    assert(oa_health_init(&health, UINT32_MAX - 50u) == OA_OK);

    input_init(&in, UINT32_MAX - 10u, 101325);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);

    /* Ten milliseconds later, on the other side of the wrap. */
    input_init(&in, 9u, 101326);
    assert(oa_health_step(&health, &cfg, &limits, &in, &out) == OA_OK);
    assert(out.faults == 0u);
}

int main(void)
{
    test_limits_start_unset();
    test_limit_metadata();
    test_limits_check();
    test_check_names();
    test_nothing_measured_means_nothing_checked();
    test_validity_is_false_before_the_first_reading();
    test_baro_stuck();
    test_imu_stuck_needs_all_six_axes();
    test_baro_range();
    test_baro_rate();
    test_a_rejected_reading_does_not_become_the_rate_reference();
    test_rate_ignores_a_zero_interval();
    test_staleness();
    test_imu_ranges();
    test_a_fault_never_disturbs_the_sample();
    test_fault_counters();
    test_null_arguments();
    test_the_clock_may_wrap();

    printf("test_oa_health: all checks passed\n");
    return 0;
}

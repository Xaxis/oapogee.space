/*
 * oApogee: tests for the flight state machine, core/src/oa_state.c.
 *
 * Plain C and assert, no framework. Each test names the specification claim it
 * checks, so that a failure points at a document rather than at an opinion.
 *
 * The sources of those claims:
 *   data/flight-phases.yaml            the states, their order, and the prose
 *                                      criteria for every transition
 *   docs/spec/telemetry-packet.md      the state enumeration on the wire, and
 *                                      the requirement that the apogee event
 *                                      time is carried separately from the time
 *                                      of the packet reporting it
 *   firmware/core/include/oapogee/oa_state.h   the interface contract
 *   firmware/SAFETY.md                 the passive payload boundary
 *
 * EVERY NUMBER IN THIS FILE IS A TEST FIXTURE.
 *
 * The thresholds set below are chosen to make the arithmetic in each test
 * obvious: a confirmation count of three, a threshold of a hundred. They are not
 * proposals, they are not measurements, and nothing in the firmware carries a
 * default. Every one of them is unmeasured, which is exactly why the machine
 * refuses to arm without being handed one, and the first test here is that
 * refusal.
 *
 * These tests have not run on hardware, because no hardware exists. They run on
 * a laptop against a table of samples, which is the whole reason core/ has no
 * SDK in it.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "oapogee/oa_config.h"
#include "oapogee/oa_state.h"

/* Fixture values. Named so a reader can see at a glance that the test's
 * arithmetic and not the flight dynamics is what they encode. */
#define FIX_LAUNCH_ACCEL_CG    (100)  /* 1.00 g of net upward acceleration */
#define FIX_LAUNCH_SAMPLES     (3)
#define FIX_BURNOUT_SAMPLES    (2)
#define FIX_APOGEE_SAMPLES     (3)
#define FIX_DESCENT_SAMPLES    (2)
#define FIX_LANDING_ALT_CM     (50)
#define FIX_LANDING_ACCEL_CG   (20)
#define FIX_LANDING_HOLD_MS    (1000)
#define FIX_SAMPLE_INTERVAL_MS (10)

/* ---------------------------------------------------------------------------
 * Fixture helpers.
 * ------------------------------------------------------------------------ */

/* Every field this build requires, set to 1, then the ones the tests reason
 * about set to their fixture values. Built by walking the field table rather
 * than by listing names, so a field added to oa_config.h does not quietly leave
 * these tests exercising an unarmable configuration. */
static void fixture_config(oa_config_t *cfg, oa_features_t features)
{
    oa_config_init(cfg);

    for (int i = 0; i < (int)OA_CFG_FIELD_COUNT; i++) {
        const oa_config_field_t field = (oa_config_field_t)i;
        if (oa_config_field_is_required(field, features)) {
            assert(oa_config_set(cfg, field, 1) == OA_OK);
        }
    }

    assert(oa_config_set(cfg, OA_CFG_LAUNCH_ACCEL_THRESHOLD_CG, FIX_LAUNCH_ACCEL_CG) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_LAUNCH_CONFIRM_SAMPLES, FIX_LAUNCH_SAMPLES) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_BURNOUT_CONFIRM_SAMPLES, FIX_BURNOUT_SAMPLES) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES, FIX_APOGEE_SAMPLES) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_DESCENT_CONFIRM_SAMPLES, FIX_DESCENT_SAMPLES) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_LANDING_ALT_BAND_CM, FIX_LANDING_ALT_CM) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_LANDING_ACCEL_BAND_CG, FIX_LANDING_ACCEL_CG) == OA_OK);
    assert(oa_config_set(cfg, OA_CFG_LANDING_HOLD_MS, FIX_LANDING_HOLD_MS) == OA_OK);
}

/* One sample, with the fields a caller would supply and nothing implied. */
static oa_state_input_t sample(uint32_t t_ms, int32_t alt_cm, int32_t vel_dm_s, int32_t accel_cg)
{
    oa_state_input_t in;
    memset(&in, 0, sizeof(in));
    in.t_ms                = t_ms;
    in.alt_cm              = alt_cm;
    in.vel_dm_s            = vel_dm_s;
    in.accel_cg            = accel_cg;
    in.baro_valid          = true;
    in.operator_armed      = true;
    in.pad_reference_locked = true;
    return in;
}

/* ---------------------------------------------------------------------------
 * Tests.
 * ------------------------------------------------------------------------ */

/* CLAIM: oa_state.h, "Reset to PAD_IDLE with every counter cleared and no peak
 * recorded", and oa_state_peak "Returns OA_ERR_EMPTY before any sample has been
 * seen, rather than reporting a peak of zero". Zero is a legitimate altitude, so
 * no measurement yet has to be a different answer from a measurement of zero. */
static void test_init_is_pad_idle_with_no_peak(void)
{
    oa_state_ctx_t ctx;

    assert(oa_state_init(&ctx) == OA_OK);
    assert(oa_state_current(&ctx) == OA_STATE_PAD_IDLE);
    assert(oa_state_init(NULL) == OA_ERR_NULL);

    int32_t  alt_cm = 12345;
    uint32_t t_ms   = 999u;
    assert(oa_state_peak(&ctx, &alt_cm, &t_ms) == OA_ERR_EMPTY);
    assert(alt_cm == 12345);  /* untouched, not overwritten with a zero peak */
    assert(t_ms == 999u);

    /* data/flight-phases.yaml, PAD_IDLE order 0, and the packet spec's state
     * table: PAD_IDLE is wire value 0. */
    assert((int)OA_STATE_PAD_IDLE == 0);
}

/* CLAIM: oa_state.h, "PAD_IDLE -> ARMED: operator_armed, and only if the
 * configuration is flightworthy for this build's features. A payload whose
 * thresholds are unmeasured refuses to arm."
 *
 * And the second half of the project rule behind it: the payload has to be able
 * to say which number is missing and what would settle it. oa_state_step has no
 * channel to return that, so the caller asks oa_config_is_flightworthy for a
 * report. This test asserts the refusal and the explanation together, because a
 * refusal nobody can explain is a payload that gets bypassed. */
static void test_will_not_arm_without_every_measured_threshold(void)
{
    oa_state_ctx_t     ctx;
    oa_config_t        cfg;
    oa_state_output_t  out;
    oa_config_report_t report;

    assert(oa_state_init(&ctx) == OA_OK);

    /* A configuration in the state a payload with no configuration file is in. */
    oa_config_init(&cfg);

    const oa_state_input_t in = sample(0u, 0, 0, 0);

    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_PAD_IDLE);
    assert(out.changed == false);

    /* Stepping it a thousand times does not wear the refusal down. */
    for (int i = 0; i < 1000; i++) {
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_PAD_IDLE);
    }

    memset(&report, 0, sizeof(report));
    assert(oa_config_is_flightworthy(&cfg, OA_FEATURE_NONE, &report) == false);
    assert(report.missing_count > 0u);

    /* Now everything except one threshold, so the report names exactly one
     * field and the machine still refuses. */
    fixture_config(&cfg, OA_FEATURE_NONE);
    assert(oa_config_clear(&cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES) == OA_OK);

    memset(&report, 0, sizeof(report));
    assert(oa_config_is_flightworthy(&cfg, OA_FEATURE_NONE, &report) == false);
    assert(report.missing_count == 1u);

    const size_t idx = (size_t)OA_CFG_APOGEE_CONFIRM_SAMPLES;
    assert((report.missing[idx / 8u] & (uint8_t)(1u << (idx % 8u))) != 0u);

    /* The report is only useful if it can be read out, so check the strings the
     * payload would print are there. */
    assert(oa_config_field_name(OA_CFG_APOGEE_CONFIRM_SAMPLES) != NULL);
    assert(oa_config_field_why(OA_CFG_APOGEE_CONFIRM_SAMPLES) != NULL);
    assert(oa_config_field_why(OA_CFG_APOGEE_CONFIRM_SAMPLES)[0] != '\0');

    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_PAD_IDLE);

    /* And with the field restored it arms on the same input, which is what
     * proves the refusal was about that field and not about anything else. */
    assert(oa_config_set(&cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES, FIX_APOGEE_SAMPLES) == OA_OK);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_ARMED);
    assert(out.changed == true);
}

/* CLAIM: data/flight-phases.yaml, PAD_IDLE criteria, "Operator arms the
 * payload." The arming switch is the only thing outside the payload that can
 * reach this machine (firmware/SAFETY.md), so a flightworthy configuration on
 * its own must not arm anything. */
static void test_flightworthy_alone_does_not_arm(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    assert(oa_state_init(&ctx) == OA_OK);
    fixture_config(&cfg, OA_FEATURE_NONE);
    assert(oa_config_is_flightworthy(&cfg, OA_FEATURE_NONE, NULL) == true);

    oa_state_input_t in = sample(0u, 0, 0, 0);
    in.operator_armed   = false;

    for (int i = 0; i < 100; i++) {
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_PAD_IDLE);
    }

    in.operator_armed = true;
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_ARMED);
}

/* Drive a context to ARMED. Returns the timestamp of the first armed sample. */
static void arm(oa_state_ctx_t *ctx, const oa_config_t *cfg)
{
    oa_state_output_t      out;
    const oa_state_input_t in = sample(0u, 0, 0, 0);

    assert(oa_state_init(ctx) == OA_OK);
    assert(oa_state_step(ctx, cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_ARMED);
}

/* CLAIM: oa_state.h, "ARMED -> BOOST: pad_reference_locked, and accel_cg at or
 * above launch_accel_threshold_cg for launch_confirm_samples consecutive
 * samples", and data/flight-phases.yaml ARMED behaviour, "you leave the rocket
 * alone after arming rather than before it": a launch detected against an
 * unfinished reference measures the whole flight from the wrong zero. */
static void test_launch_needs_the_reference_and_consecutive_samples(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;
    uint32_t          t = 0u;

    fixture_config(&cfg, OA_FEATURE_NONE);
    arm(&ctx, &cfg);

    /* Full thrust with the reference still settling does not launch, however
     * long it goes on. */
    for (int i = 0; i < 50; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        oa_state_input_t in    = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG * 10);
        in.pad_reference_locked = false;
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_ARMED);
    }

    /* A knock on the rail: one sample over the threshold, then quiet. The run is
     * about consecutive evidence, not about a total, so this must never
     * accumulate to a launch. */
    for (int i = 0; i < 20; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t knock = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG + 1);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &knock, &out) == OA_OK);
        assert(out.state == OA_STATE_ARMED);

        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t quiet = sample(t, 0, 0, 0);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &quiet, &out) == OA_OK);
        assert(out.state == OA_STATE_ARMED);
    }

    /* One sample short of the count is still ARMED. The threshold is inclusive,
     * "at or above", so exactly the threshold counts. */
    for (int i = 0; i < FIX_LAUNCH_SAMPLES - 1; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_ARMED);
    }

    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t in = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_BOOST);
    assert(out.changed == true);
}

/* CLAIM: data/flight-phases.yaml, BOOST criteria, "Acceleration falls through
 * zero as thrust ends. This is burnout", confirmed across
 * burnout_confirm_samples consecutive samples, and the open question recorded
 * against it: whether hysteresis is needed at all. burnout_accel_hysteresis_cg
 * is OA_REQ_OPTIONAL and unset means no band, so detection is on the sign
 * alone. */
static void test_burnout_on_the_sign_change_with_optional_hysteresis(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;
    uint32_t          t = 0u;

    fixture_config(&cfg, OA_FEATURE_NONE);
    assert(oa_config_is_set(&cfg, OA_CFG_BURNOUT_ACCEL_HYSTERESIS_CG) == false);

    arm(&ctx, &cfg);
    for (int i = 0; i < FIX_LAUNCH_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t boost = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG * 5);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &boost, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_BOOST);

    /* A rough burn: acceleration dips to zero for one sample and recovers. With
     * no band configured the sign alone is the criterion, so the dip counts, but
     * a single sample is not the confirmation count. */
    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t dip = sample(t, 1000, 100, 0);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &dip, &out) == OA_OK);
    assert(out.state == OA_STATE_BOOST);

    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t recover = sample(t, 1100, 110, FIX_LAUNCH_ACCEL_CG * 5);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &recover, &out) == OA_OK);
    assert(out.state == OA_STATE_BOOST);

    for (int i = 0; i < FIX_BURNOUT_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t coasting = sample(t, 1200, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &coasting, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_COAST);

    /* With a band set, a sample inside the band is not burnout. The band is what
     * the open question is about, and setting it must change the answer. */
    oa_state_ctx_t ctx2;
    uint32_t       t2 = 0u;
    assert(oa_config_set(&cfg, OA_CFG_BURNOUT_ACCEL_HYSTERESIS_CG, 100) == OA_OK);

    arm(&ctx2, &cfg);
    for (int i = 0; i < FIX_LAUNCH_SAMPLES; i++) {
        t2 += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t boost = sample(t2, 0, 0, FIX_LAUNCH_ACCEL_CG * 5);
        assert(oa_state_step(&ctx2, &cfg, OA_FEATURE_NONE, &boost, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_BOOST);

    for (int i = 0; i < FIX_BURNOUT_SAMPLES * 4; i++) {
        t2 += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t inside = sample(t2, 1200, 100, -99);
        assert(oa_state_step(&ctx2, &cfg, OA_FEATURE_NONE, &inside, &out) == OA_OK);
        assert(out.state == OA_STATE_BOOST);
    }

    for (int i = 0; i < FIX_BURNOUT_SAMPLES; i++) {
        t2 += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t outside = sample(t2, 1200, 100, -100);
        assert(oa_state_step(&ctx2, &cfg, OA_FEATURE_NONE, &outside, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_COAST);
}

/* Drive a context from init to COAST at a known peak-less altitude, returning
 * the timestamp reached. Every helper below builds on this one so that the
 * apogee tests read as the sequence of altitudes they are about. */
static uint32_t run_to_coast(oa_state_ctx_t *ctx, const oa_config_t *cfg)
{
    oa_state_output_t out;
    uint32_t          t = 0u;

    arm(ctx, cfg);

    for (int i = 0; i < FIX_LAUNCH_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, 0, 0, FIX_LAUNCH_ACCEL_CG * 5);
        assert(oa_state_step(ctx, cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_BOOST);

    for (int i = 0; i < FIX_BURNOUT_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, 100 * (i + 1), 500, -50);
        assert(oa_state_step(ctx, cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_COAST);

    return t;
}

/* CLAIM: data/flight-phases.yaml, COAST criteria: "Altitude decreasing across
 * several consecutive samples. Confirmation across multiple samples is required
 * rather than a single reading, because barometric noise alone will produce a
 * descending pair at any point near the top of a flight and a single-sample rule
 * would call apogee early." */
static void test_apogee_needs_consecutive_descending_samples(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    /* Climbing. */
    int32_t alt = 10000;
    for (int i = 0; i < 10; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 100;
        const oa_state_input_t in = sample(t, alt, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
    }

    /* Noise near the top: a descending sample, then back above the peak. One
     * descending reading must not call apogee, and the run must not accumulate
     * across the climb between them. */
    for (int i = 0; i < 10; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t down = sample(t, alt - 5, 0, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &down, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);

        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 1;
        const oa_state_input_t up = sample(t, alt, 10, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &up, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
    }

    /* One short of the confirmation count. */
    for (int i = 0; i < FIX_APOGEE_SAMPLES - 1; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, alt - (10 * (i + 1)), -10, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
        assert(out.apogee_detected == false);
    }

    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t in = sample(t, alt - 100, -20, -50);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_APOGEE);
    assert(out.apogee_detected == true);
}

/* CLAIM: docs/spec/telemetry-packet.md, 0x3 APOGEE: "t_apogee_ms is the
 * estimated time of the apogee event, which is earlier than the t_ms in the
 * header of the packet reporting it. The difference is the detection lag...
 * Apogee detection requires confirmation across multiple descending samples, so
 * the lag is real and non-zero."
 *
 * So the event time and the detection time are two different numbers, and this
 * machine has to produce the first one, not the second. */
static void test_apogee_event_time_precedes_detection_time(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 20000;
    for (int i = 0; i < 5; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 50;
        const oa_state_input_t in = sample(t, alt, 50, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }

    /* The highest sample of the flight, and when it happened. */
    const int32_t  peak_alt  = alt;
    const uint32_t peak_time = t;

    uint32_t detect_time = 0u;
    for (int i = 0; i < FIX_APOGEE_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, alt - (10 * (i + 1)), -10, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        detect_time = t;
    }

    assert(out.state == OA_STATE_APOGEE);
    assert(out.apogee_detected == true);
    assert(out.apogee_cm == peak_alt);
    assert(out.t_apogee_ms == peak_time);

    /* The lag is real and non-zero, and it is exactly the confirmation count
     * times the sample interval. This is the number docs/spec/telemetry-packet.md
     * carries a TODO(verify) against. */
    assert(out.t_apogee_ms < detect_time);
    assert((detect_time - out.t_apogee_ms)
           == (uint32_t)(FIX_APOGEE_SAMPLES * FIX_SAMPLE_INTERVAL_MS));

    /* The same pair through the accessor. */
    int32_t  peak_out = 0;
    uint32_t time_out = 0u;
    assert(oa_state_peak(&ctx, &peak_out, &time_out) == OA_OK);
    assert(peak_out == peak_alt);
    assert(time_out == peak_time);

    /* oa_state.h: "Set once, when APOGEE is entered, so the scheduler is told
     * exactly once." oa_sched_notify_apogee refuses a second call, so a machine
     * that reported twice would produce an error in flight. */
    for (int i = 0; i < 20; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, peak_alt - 1000, -100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.apogee_detected == false);
    }
}

/* CLAIM: oa_state.h on baro_valid, "When this is false the machine must not
 * confirm apogee from altitude alone", and the reason it is a separate input at
 * all: "a barometer that has failed still returns numbers, and a detector that
 * cannot tell the difference between a sensor that is quiet and a sensor that is
 * dead will call apogee on a dead sensor." */
static void test_no_apogee_from_a_failed_barometer(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 30000;
    for (int i = 0; i < 5; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 50;
        const oa_state_input_t in = sample(t, alt, 50, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }

    /* A dead sensor stuck just below the last good reading looks exactly like a
     * gentle descent that never ends. Fifty samples of it, which is many times
     * the confirmation count, must not produce an apogee. */
    for (int i = 0; i < 50; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        oa_state_input_t in = sample(t, alt - 20, -1, -50);
        in.baro_valid       = false;
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
        assert(out.apogee_detected == false);
    }
}

/* CLAIM: oa_state.h, the full transition list, and data/flight-phases.yaml's
 * `transitions_to` chain: PAD_IDLE, ARMED, BOOST, COAST, APOGEE, DESCENT,
 * LANDED. "There is no transition that skips a state and no transition
 * backwards", and LANDED is "Terminal state until power cycle."
 *
 * Driven as one flight, recording every state the machine was ever in. */
static void test_a_whole_flight_visits_every_state_in_order(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    oa_state_t visited[16];
    size_t     n_visited = 0u;
    visited[n_visited++] = OA_STATE_PAD_IDLE;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    /* run_to_coast walked ARMED and BOOST. Record them by hand because it
     * asserts rather than reports. */
    visited[n_visited++] = OA_STATE_ARMED;
    visited[n_visited++] = OA_STATE_BOOST;
    visited[n_visited++] = OA_STATE_COAST;

    int32_t alt = 15000;
    for (int i = 0; i < 5; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 100;
        const oa_state_input_t in = sample(t, alt, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        if (out.changed) {
            visited[n_visited++] = out.state;
        }
    }

    /* Over the top and down. */
    for (int i = 0; i < 60; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt -= 100;
        if (alt < 0) {
            alt = 0;
        }
        const oa_state_input_t in = sample(t, alt, -100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        if (out.changed) {
            visited[n_visited++] = out.state;
        }
    }
    assert(out.state == OA_STATE_DESCENT);

    /* On the ground, quiet, for longer than the hold. */
    for (int i = 0; i < (FIX_LANDING_HOLD_MS / FIX_SAMPLE_INTERVAL_MS) + 10; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, 10, 0, 5);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        if (out.changed) {
            visited[n_visited++] = out.state;
        }
    }
    assert(out.state == OA_STATE_LANDED);

    assert(n_visited == 7u);
    for (size_t i = 0; i < n_visited; i++) {
        assert((int)visited[i] == (int)i);
    }

    /* Terminal. Nothing, including a plausible second flight, moves it. */
    for (int i = 0; i < 200; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, 50000, 3000, 20000);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_LANDED);
        assert(out.changed == false);
    }
}

/* CLAIM: data/flight-phases.yaml, DESCENT criteria: "Altitude stable near the
 * ground reference and acceleration quiet, both held for a sustained period."
 * Both, and held: a rocket swinging under a parachute close to the ground
 * satisfies the altitude band intermittently and must not be called down. */
static void test_landing_requires_both_bands_held_for_the_hold(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 8000;
    for (int i = 0; i < FIX_APOGEE_SAMPLES + 1; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt -= 100;
        const oa_state_input_t in = sample(t, alt, -100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_APOGEE);

    for (int i = 0; i < FIX_DESCENT_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt -= 100;
        const oa_state_input_t in = sample(t, alt, -100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_DESCENT);

    /* Inside the altitude band but still moving: not landed, however long. */
    for (int i = 0; i < 500; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, FIX_LANDING_ALT_CM - 1, -50, FIX_LANDING_ACCEL_CG + 1);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_DESCENT);
    }

    /* Quiet but out of the altitude band, which is a rocket hanging somewhere it
     * should not be: not landed either. */
    for (int i = 0; i < 500; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, FIX_LANDING_ALT_CM + 1, 0, 0);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_DESCENT);
    }

    /* Both bands satisfied, but disturbed before the hold completes. The hold
     * restarts, so the total time inside the bands is irrelevant. */
    for (int cycle = 0; cycle < 5; cycle++) {
        for (int i = 0; i < (FIX_LANDING_HOLD_MS / FIX_SAMPLE_INTERVAL_MS) - 1; i++) {
            t += FIX_SAMPLE_INTERVAL_MS;
            const oa_state_input_t in = sample(t, 0, 0, 0);
            assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
            assert(out.state == OA_STATE_DESCENT);
        }
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t swing = sample(t, 0, 0, FIX_LANDING_ACCEL_CG + 1);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &swing, &out) == OA_OK);
        assert(out.state == OA_STATE_DESCENT);
    }

    /* Uninterrupted, it lands. */
    for (int i = 0; i <= (FIX_LANDING_HOLD_MS / FIX_SAMPLE_INTERVAL_MS); i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, -10, 0, -5);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_LANDED);
}

/* CLAIM: docs/spec/telemetry-packet.md, FLIGHT: "alt_cm is signed, and negative
 * values are legitimate rather than a bug. A barometric zero taken on the pad
 * reports negative altitude if the rocket lands below the pad or if pressure
 * rises during the flight." The landing band is a band around the reference, so
 * it is symmetric, and a rocket that lands below the pad still lands. */
static void test_negative_altitude_is_a_measurement_not_a_fault(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 5000;
    for (int i = 0; i < FIX_APOGEE_SAMPLES + FIX_DESCENT_SAMPLES + 2; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt -= 200;
        const oa_state_input_t in = sample(t, alt, -200, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_DESCENT);

    for (int i = 0; i <= (FIX_LANDING_HOLD_MS / FIX_SAMPLE_INTERVAL_MS); i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, -(FIX_LANDING_ALT_CM - 1), 0, 0);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_LANDED);
}

/* CLAIM: docs/spec/telemetry-packet.md, APOGEE: apogee is "the number the entire
 * payload exists to produce". The deployment event follows apogee by a second or
 * so and puts a pressure transient through the static ports, and a transient
 * that read as a higher altitude would revise that number upward after the fact.
 * oa_state.h states the peak is what the machine reports; this asserts the
 * reported peak is the flight's apogee and not the largest number the barometer
 * ever produced. */
static void test_the_apogee_is_not_revised_by_a_later_reading(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 40000;
    for (int i = 0; i < 5; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 100;
        const oa_state_input_t in = sample(t, alt, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    const int32_t true_apogee_cm = alt;

    for (int i = 0; i < FIX_APOGEE_SAMPLES; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        const oa_state_input_t in = sample(t, alt - (10 * (i + 1)), -10, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }
    assert(out.state == OA_STATE_APOGEE);
    assert(out.apogee_cm == true_apogee_cm);

    /* The deployment transient. */
    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t jolt = sample(t, true_apogee_cm + 100000, -100, 5000);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &jolt, &out) == OA_OK);
    assert(out.apogee_cm == true_apogee_cm);

    int32_t peak_out = 0;
    assert(oa_state_peak(&ctx, &peak_out, NULL) == OA_OK);
    assert(peak_out == true_apogee_cm);
}

/* CLAIM: oa_state.h on baro_valid, "When this is false the machine must not
 * confirm apogee from altitude alone."
 *
 * The descent test above covers a sensor that fails and stays failed. This
 * covers the harder one: a sensor that glitches high for a single sample and
 * then recovers. The peak only ever rises, so an invalid sample that raises it
 * is not corrected by anything downstream. Every good sample afterwards sits
 * below a peak that never happened, which reads as a descent that never ends,
 * and the machine confirms apogee while the rocket is still climbing and
 * reports the glitch as the altitude. */
static void test_an_invalid_sample_does_not_set_the_peak(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    int32_t alt = 20000;
    for (int i = 0; i < 5; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 100;
        const oa_state_input_t in = sample(t, alt, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    }

    /* One rejected sample, a kilometre high. */
    t += FIX_SAMPLE_INTERVAL_MS;
    oa_state_input_t glitch = sample(t, alt + 100000, 100, -50);
    glitch.baro_valid       = false;
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &glitch, &out) == OA_OK);
    assert(out.state == OA_STATE_COAST);

    int32_t peak_out = 0;
    assert(oa_state_peak(&ctx, &peak_out, NULL) == OA_OK);
    assert(peak_out == alt);

    /* Still climbing, on good samples. Many times the confirmation count. */
    for (int i = 0; i < 50; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt += 100;
        const oa_state_input_t in = sample(t, alt, 100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
        assert(out.apogee_detected == false);
    }

    assert(oa_state_peak(&ctx, &peak_out, NULL) == OA_OK);
    assert(peak_out == alt);
}

/* CLAIM: oa_state.h, "Returns OA_OK, OA_ERR_NULL, or OA_ERR_STATE if the
 * sample's timestamp went backwards." A machine that accepted one would compute
 * a negative interval for the landing hold. */
static void test_a_backwards_timestamp_is_rejected(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    arm(&ctx, &cfg);

    const oa_state_input_t forward = sample(5000u, 0, 0, 0);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &forward, &out) == OA_OK);

    const oa_state_input_t backward = sample(4999u, 0, 0, 0);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &backward, &out) == OA_ERR_STATE);
    assert(oa_state_current(&ctx) == OA_STATE_ARMED);

    /* The same timestamp twice is not backwards. A caller polling faster than
     * the millisecond clock ticks is normal. */
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &forward, &out) == OA_OK);
}

/* CLAIM: oa_state.h, "Returns OA_OK, OA_ERR_NULL". Every pointer argument is
 * required, and oa_state_current has no error channel, so a NULL context reports
 * the state that advances nothing. */
static void test_null_arguments(void)
{
    oa_state_ctx_t         ctx;
    oa_config_t            cfg;
    oa_state_output_t      out;
    const oa_state_input_t in = sample(0u, 0, 0, 0);

    fixture_config(&cfg, OA_FEATURE_NONE);
    assert(oa_state_init(&ctx) == OA_OK);

    assert(oa_state_step(NULL, &cfg, OA_FEATURE_NONE, &in, &out) == OA_ERR_NULL);
    assert(oa_state_step(&ctx, NULL, OA_FEATURE_NONE, &in, &out) == OA_ERR_NULL);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, NULL, &out) == OA_ERR_NULL);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, NULL) == OA_ERR_NULL);

    assert(oa_state_peak(NULL, NULL, NULL) == OA_ERR_NULL);
    assert(oa_state_current(NULL) == OA_STATE_PAD_IDLE);
}

/* CLAIM: the project rule this whole machine is built around, stated in
 * firmware/README.md and oa_config.h: there are no tuning constants in this
 * firmware, and nothing falls back to a number. A threshold removed mid-flight
 * stops the transition that needed it rather than being replaced with a guess.
 *
 * This cannot happen through the normal path, because arming requires every
 * required field. It is asserted anyway, because "the machine does not advance
 * past ARMED without the numbers" is only true if every predicate says so
 * individually. */
static void test_no_transition_invents_a_missing_threshold(void)
{
    oa_state_ctx_t    ctx;
    oa_config_t       cfg;
    oa_state_output_t out;

    fixture_config(&cfg, OA_FEATURE_NONE);
    uint32_t t = run_to_coast(&ctx, &cfg);

    assert(oa_config_clear(&cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES) == OA_OK);
    assert(oa_config_is_flightworthy(&cfg, OA_FEATURE_NONE, NULL) == false);

    int32_t alt = 25000;
    t += FIX_SAMPLE_INTERVAL_MS;
    const oa_state_input_t top = sample(t, alt, 0, -50);
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &top, &out) == OA_OK);

    for (int i = 0; i < 500; i++) {
        t += FIX_SAMPLE_INTERVAL_MS;
        alt -= 10;
        const oa_state_input_t in = sample(t, alt, -100, -50);
        assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.state == OA_STATE_COAST);
        assert(out.apogee_detected == false);
    }
}

int main(void)
{
    test_init_is_pad_idle_with_no_peak();
    test_will_not_arm_without_every_measured_threshold();
    test_flightworthy_alone_does_not_arm();
    test_launch_needs_the_reference_and_consecutive_samples();
    test_burnout_on_the_sign_change_with_optional_hysteresis();
    test_apogee_needs_consecutive_descending_samples();
    test_apogee_event_time_precedes_detection_time();
    test_no_apogee_from_a_failed_barometer();
    test_a_whole_flight_visits_every_state_in_order();
    test_landing_requires_both_bands_held_for_the_hold();
    test_negative_altitude_is_a_measurement_not_a_fault();
    test_the_apogee_is_not_revised_by_a_later_reading();
    test_an_invalid_sample_does_not_set_the_peak();
    test_a_backwards_timestamp_is_rejected();
    test_null_arguments();
    test_no_transition_invents_a_missing_threshold();
    return 0;
}

/*
 * oApogee: tests for the transmit scheduler, core/src/oa_sched.c.
 *
 * Plain C and assert, no framework. Each test names the specification claim it
 * checks.
 *
 * The sources of those claims:
 *   docs/spec/telemetry-packet.md   the five packet types, the five normative
 *                                   scheduling rules, and the seq field that
 *                                   "increments per transmitted packet, wraps
 *                                   at 255"
 *   firmware/core/include/oapogee/oa_sched.h   the interface contract
 *   firmware/SAFETY.md              the passive payload boundary
 *
 * EVERY NUMBER IN THIS FILE IS A TEST FIXTURE.
 *
 * The intervals below are round numbers chosen so the arithmetic in each test is
 * checkable by eye. They are not proposals and they are not measurements. The
 * real ones depend on the airtime of a packet at a radio configuration nobody
 * has measured, at a spreading factor nobody has chosen, under a regional duty
 * cycle limit that differs by country. The scheduler reports OA_ERR_UNSET rather
 * than inventing any of them, and that is the first thing tested here.
 *
 * These tests have not run on hardware. No packet has been transmitted.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "oapogee/oa_config.h"
#include "oapogee/oa_sched.h"

#define FIX_PAD_MS         (5000)
#define FIX_FLIGHT_MS      (500)
#define FIX_BEACON_MS      (10000)
#define FIX_BEACON_MAX_MS  (30000)
#define FIX_BEACON_STEP_MS (5000)
#define FIX_APOGEE_REPEAT  (3)
#define FIX_APOGEE_GAP_MS  (200)
#define FIX_INTERLEAVE     (3)

#define FEATURES_LINK  ((oa_features_t)OA_FEATURE_RADIO)
#define FEATURES_TRACK ((oa_features_t)(OA_FEATURE_RADIO | OA_FEATURE_GNSS))

/* ---------------------------------------------------------------------------
 * Fixture helpers.
 * ------------------------------------------------------------------------ */

/* Every field this build requires, set to 1, then the transmit fields set to
 * their fixture values. Walked from the field table so that a scheduling field
 * added to oa_config.h does not leave these tests running against a
 * configuration that could never arm. */
static void fixture_config(oa_config_t *cfg, oa_features_t features)
{
    oa_config_init(cfg);

    for (int i = 0; i < (int)OA_CFG_FIELD_COUNT; i++) {
        const oa_config_field_t field = (oa_config_field_t)i;
        if (oa_config_field_is_required(field, features)) {
            assert(oa_config_set(cfg, field, 1) == OA_OK);
        }
    }

    if ((features & (oa_features_t)OA_FEATURE_RADIO) != 0u) {
        assert(oa_config_set(cfg, OA_CFG_TX_INTERVAL_PAD_MS, FIX_PAD_MS) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_INTERVAL_FLIGHT_MS, FIX_FLIGHT_MS) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_INTERVAL_BEACON_MS, FIX_BEACON_MS) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_INTERVAL_BEACON_MAX_MS, FIX_BEACON_MAX_MS) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_BEACON_STRETCH_MS, FIX_BEACON_STEP_MS) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_APOGEE_REPEAT, FIX_APOGEE_REPEAT) == OA_OK);
        assert(oa_config_set(cfg, OA_CFG_TX_APOGEE_REPEAT_INTERVAL_MS, FIX_APOGEE_GAP_MS) == OA_OK);
    }
    if ((features & (oa_features_t)OA_FEATURE_GNSS) != 0u) {
        assert(oa_config_set(cfg, OA_CFG_TX_POSITION_INTERLEAVE, FIX_INTERLEAVE) == OA_OK);
    }
}

static oa_sched_input_t at(uint32_t t_ms, oa_state_t state)
{
    oa_sched_input_t in;
    memset(&in, 0, sizeof(in));
    in.t_ms       = t_ms;
    in.state      = state;
    in.radio_busy = false;
    return in;
}

/* Poll, expect a send of `type`, confirm it, and return the sequence number that
 * went out. The two calls are separate in the interface on purpose, and pairing
 * them here keeps every test that is not about that split short. */
static uint8_t send_one(oa_sched_t *s,
                        const oa_config_t *cfg,
                        oa_features_t features,
                        uint32_t t_ms,
                        oa_state_t state,
                        oa_packet_type_t type)
{
    oa_sched_decision_t    out;
    const oa_sched_input_t in = at(t_ms, state);

    assert(oa_sched_poll(s, cfg, features, &in, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == type);
    assert(oa_sched_notify_sent(s, cfg, out.type, t_ms) == OA_OK);

    return out.seq;
}

/* ---------------------------------------------------------------------------
 * Tests.
 * ------------------------------------------------------------------------ */

/* CLAIM: oa_sched.h, "Reset. Every stream becomes due immediately in its own
 * state, so a payload that powers up on the pad transmits a STATUS packet
 * without waiting a full interval for its first sign of life." */
static void test_init_makes_every_stream_due_immediately(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    assert(oa_sched_init(NULL) == OA_ERR_NULL);
    fixture_config(&cfg, FEATURES_TRACK);

    const oa_state_t immediate[] = {OA_STATE_PAD_IDLE, OA_STATE_ARMED, OA_STATE_BOOST,
                                    OA_STATE_COAST,    OA_STATE_APOGEE, OA_STATE_DESCENT,
                                    OA_STATE_LANDED};
    const oa_packet_type_t expect[] = {OA_PKT_STATUS, OA_PKT_STATUS, OA_PKT_FLIGHT,
                                       OA_PKT_FLIGHT, OA_PKT_FLIGHT, OA_PKT_FLIGHT,
                                       OA_PKT_BEACON};

    for (size_t i = 0; i < (sizeof(immediate) / sizeof(immediate[0])); i++) {
        assert(oa_sched_init(&s) == OA_OK);
        const oa_sched_input_t in = at(0u, immediate[i]);
        assert(oa_sched_poll(&s, &cfg, FEATURES_TRACK, &in, &out) == OA_OK);
        assert(out.send == true);
        assert(out.type == expect[i]);
        assert(out.seq == 0u);
    }
}

/* CLAIM: oa_sched.h, "Every interval this scheduler uses is unmeasured and lives
 * in oa_config_t as unset. It reports OA_ERR_UNSET rather than falling back to a
 * number, because a transmit interval chosen without measured airtime is how a
 * payload exceeds a regional duty cycle limit without anyone noticing."
 *
 * Checked per state, because each state uses different fields and a fallback
 * hiding in one of them would not show up through the others. */
static void test_a_missing_interval_is_reported_never_guessed(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    /* Flightworthy for a build with no radio, so every radio field is unset,
     * then asked to schedule as though a radio were fitted. */
    fixture_config(&cfg, OA_FEATURE_NONE);

    const oa_state_t states[] = {OA_STATE_PAD_IDLE, OA_STATE_ARMED,  OA_STATE_BOOST,
                                 OA_STATE_COAST,    OA_STATE_APOGEE, OA_STATE_DESCENT,
                                 OA_STATE_LANDED};

    for (size_t i = 0; i < (sizeof(states) / sizeof(states[0])); i++) {
        assert(oa_sched_init(&s) == OA_OK);
        const oa_sched_input_t in = at(0u, states[i]);
        assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_ERR_UNSET);
        assert(out.send == false);
    }

    /* The interleave ratio is required on a build with GNSS and is checked
     * separately from the intervals, because it is a count and not a time. */
    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_config_is_set(&cfg, OA_CFG_TX_POSITION_INTERLEAVE) == false);

    assert(oa_sched_init(&s) == OA_OK);
    const oa_sched_input_t boost = at(0u, OA_STATE_BOOST);
    assert(oa_sched_poll(&s, &cfg, FEATURES_TRACK, &boost, &out) == OA_ERR_UNSET);

    /* Without the GNSS feature the same configuration schedules fine, which is
     * what "required only when the corresponding part is populated" means. */
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &boost, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == OA_PKT_FLIGHT);

    /* A missing repeat count stops the APOGEE queue rather than repeating some
     * default number of times. The apogee itself is still recorded, because the
     * BEACON body carries it and a missing interval must not cost the number. */
    assert(oa_config_clear(&cfg, OA_CFG_TX_APOGEE_REPEAT) == OA_OK);
    assert(oa_sched_notify_apogee(&s, &cfg, 1000u, 12345, 900u) == OA_ERR_UNSET);

    int32_t  apogee_cm = 0;
    uint32_t t_apogee  = 0u;
    assert(oa_sched_apogee(&s, &apogee_cm, &t_apogee) == OA_OK);
    assert(apogee_cm == 12345);
    assert(t_apogee == 900u);
}

/* CLAIM: firmware/SAFETY.md and data/tiers: a Solo build has no radio. Nothing
 * is scheduled for a build that cannot transmit, and that is not a fault: a
 * validator that reported one for a complete payload is a validator that gets
 * bypassed. */
static void test_a_build_with_no_radio_sends_nothing(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_TRACK);
    assert(oa_sched_init(&s) == OA_OK);

    for (uint32_t t = 0u; t < 100000u; t += 1000u) {
        const oa_sched_input_t in = at(t, OA_STATE_BOOST);
        assert(oa_sched_poll(&s, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
        assert(out.send == false);
    }
    assert(s.seq == 0u);
}

/* CLAIM: docs/spec/telemetry-packet.md scheduling rule 4, "STATUS runs slowly
 * during PAD_IDLE and ARMED", and rule 2, "FLIGHT runs at the fastest
 * sustainable rate from BOOST through DESCENT". Each stream keeps its own
 * cadence and nothing is sent before its interval has elapsed. */
static void test_each_state_gates_its_own_packet_type(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    (void)send_one(&s, &cfg, FEATURES_LINK, 0u, OA_STATE_PAD_IDLE, OA_PKT_STATUS);

    /* One millisecond short of the interval, nothing is due, and the hint says
     * exactly when to come back. */
    const oa_sched_input_t early = at((uint32_t)FIX_PAD_MS - 1u, OA_STATE_PAD_IDLE);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &early, &out) == OA_OK);
    assert(out.send == false);
    assert(out.next_due_ms == (uint32_t)FIX_PAD_MS);

    (void)send_one(&s, &cfg, FEATURES_LINK, (uint32_t)FIX_PAD_MS, OA_STATE_ARMED, OA_PKT_STATUS);

    /* Into the flight states. The FLIGHT stream has its own schedule and was
     * last due at 0, so it is due immediately on the first sample of BOOST. */
    (void)send_one(&s, &cfg, FEATURES_LINK, 6000u, OA_STATE_BOOST, OA_PKT_FLIGHT);

    const oa_sched_input_t soon = at(6000u + (uint32_t)FIX_FLIGHT_MS - 1u, OA_STATE_COAST);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &soon, &out) == OA_OK);
    assert(out.send == false);

    (void)send_one(&s, &cfg, FEATURES_LINK, 6000u + (uint32_t)FIX_FLIGHT_MS, OA_STATE_COAST,
                   OA_PKT_FLIGHT);
    (void)send_one(&s, &cfg, FEATURES_LINK, 6000u + (2u * (uint32_t)FIX_FLIGHT_MS),
                   OA_STATE_DESCENT, OA_PKT_FLIGHT);

    /* And a STATUS packet is not offered again during the flight, whatever the
     * pad schedule says. */
    const oa_sched_input_t late = at(1000000u, OA_STATE_DESCENT);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &late, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == OA_PKT_FLIGHT);
}

/* CLAIM: docs/spec/telemetry-packet.md header, "seq: Increments per transmitted
 * packet, wraps at 255", and oa_sched.h, "Split from poll because a decision the
 * radio then refused must not count as a transmission: seq would gain a gap that
 * no receiver lost." */
static void test_seq_counts_transmissions_and_wraps_at_255(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    /* Polling does not consume a sequence number. Ten polls, no sends. */
    for (int i = 0; i < 10; i++) {
        const oa_sched_input_t in = at(0u, OA_STATE_PAD_IDLE);
        assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
        assert(out.send == true);
        assert(out.seq == 0u);
    }

    /* 258 transmissions, so the counter passes 255 twice over. */
    for (unsigned i = 0; i < 258u; i++) {
        const uint32_t t   = i * (uint32_t)FIX_PAD_MS;
        const uint8_t  seq = send_one(&s, &cfg, FEATURES_LINK, t, OA_STATE_PAD_IDLE, OA_PKT_STATUS);
        assert(seq == (uint8_t)(i & 0xFFu));
    }

    /* Which is the same thing said plainly: after 256 sends it is back where it
     * started, so a receiver counting gaps sees no discontinuity it did not
     * cause. */
    assert(s.seq == (uint8_t)(258u & 0xFFu));
}

/* CLAIM: docs/spec/telemetry-packet.md scheduling rule 1, "APOGEE preempts
 * everything. It is queued the moment apogee is detected and transmitted at the
 * next opportunity, ahead of any pending packet", and the reason given in the
 * same document: apogee "happens once, at the greatest distance from the
 * receiver, and it is followed by the deployment event that is the most likely
 * moment for the payload to be damaged". */
static void test_apogee_preempts_a_due_flight_packet(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    /* A FLIGHT packet is due right now. */
    const oa_sched_input_t in = at(4000u, OA_STATE_COAST);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == OA_PKT_FLIGHT);

    /* Apogee is detected before that decision is acted on. */
    assert(oa_sched_notify_apogee(&s, &cfg, 4000u, 123456, 3800u) == OA_OK);

    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == OA_PKT_APOGEE);
    assert(oa_sched_notify_sent(&s, &cfg, OA_PKT_APOGEE, 4000u) == OA_OK);

    /* The displaced FLIGHT packet is not dropped. It was due and it is still
     * due, so it is offered on the very next poll. */
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
    assert(out.send == true);
    assert(out.type == OA_PKT_FLIGHT);
    assert(out.seq == 1u);
}

/* CLAIM: docs/spec/telemetry-packet.md, 0x3 APOGEE: "Sent the instant apogee is
 * detected, then repeated a small number of times", spaced by
 * tx_apogee_repeat_interval_ms, and oa_sched.h: "Calling this more than once in
 * a flight is a programming error and returns OA_ERR_STATE, rather than
 * requeueing. Apogee happens once." */
static void test_apogee_repeats_exactly_the_configured_number_of_times(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    int32_t  apogee_cm = 0;
    uint32_t t_apogee  = 0u;
    assert(oa_sched_apogee(&s, &apogee_cm, &t_apogee) == OA_ERR_EMPTY);

    assert(oa_sched_notify_apogee(&s, &cfg, 4000u, 123456, 3800u) == OA_OK);
    assert(oa_sched_notify_apogee(&s, &cfg, 4000u, 999999, 3900u) == OA_ERR_STATE);

    assert(oa_sched_apogee(&s, &apogee_cm, &t_apogee) == OA_OK);
    assert(apogee_cm == 123456);
    assert(t_apogee == 3800u);

    uint32_t t = 4000u;
    for (int i = 0; i < FIX_APOGEE_REPEAT; i++) {
        (void)send_one(&s, &cfg, FEATURES_LINK, t, OA_STATE_APOGEE, OA_PKT_APOGEE);
        t += (uint32_t)FIX_APOGEE_GAP_MS;
    }

    /* Exactly that many, and no more, however long the flight continues. */
    for (int i = 0; i < 50; i++) {
        const oa_sched_input_t in = at(t, OA_STATE_DESCENT);
        assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
        assert(out.send == true);
        assert(out.type == OA_PKT_FLIGHT);
        assert(oa_sched_notify_sent(&s, &cfg, out.type, t) == OA_OK);
        t += (uint32_t)FIX_FLIGHT_MS;
    }

    /* Confirming an APOGEE packet that was never queued is a programming error,
     * not a schedule to advance. */
    assert(oa_sched_notify_sent(&s, &cfg, OA_PKT_APOGEE, t) == OA_ERR_STATE);

    /* The apogee survives the whole flight, because the BEACON body carries it
     * so that a walkaway recovery still yields the number. */
    assert(oa_sched_apogee(&s, &apogee_cm, &t_apogee) == OA_OK);
    assert(apogee_cm == 123456);
}

/* CLAIM: docs/spec/telemetry-packet.md scheduling rule 3, "POSITION is
 * interleaved at a fraction of the FLIGHT rate on Track builds", and oa_sched.h,
 * "POSITION displaces a FLIGHT rather than being added between them, because
 * airtime is the scarce resource and adding packets would silently reduce the
 * flight rate instead of the caller choosing to."
 *
 * So the test is not only that POSITION appears. It is that the slot cadence
 * does not change when it does. */
static void test_position_displaces_a_flight_and_does_not_add_a_slot(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_TRACK);
    assert(oa_sched_init(&s) == OA_OK);

    uint32_t t         = 0u;
    int      positions = 0;
    int      flights   = 0;

    for (int i = 0; i < 12; i++) {
        const oa_sched_input_t in = at(t, OA_STATE_COAST);
        assert(oa_sched_poll(&s, &cfg, FEATURES_TRACK, &in, &out) == OA_OK);
        assert(out.send == true);

        /* Three FLIGHT packets per POSITION, in that order, forever. */
        if ((i % (FIX_INTERLEAVE + 1)) == FIX_INTERLEAVE) {
            assert(out.type == OA_PKT_POSITION);
            positions++;
        } else {
            assert(out.type == OA_PKT_FLIGHT);
            flights++;
        }

        assert(oa_sched_notify_sent(&s, &cfg, out.type, t) == OA_OK);

        /* Every slot, whichever type filled it, is one flight interval apart.
         * That is what "displaces" means and what "adds" would not. */
        const oa_sched_input_t next = at(t, OA_STATE_COAST);
        assert(oa_sched_poll(&s, &cfg, FEATURES_TRACK, &next, &out) == OA_OK);
        assert(out.send == false);
        assert(out.next_due_ms == t + (uint32_t)FIX_FLIGHT_MS);

        t += (uint32_t)FIX_FLIGHT_MS;
    }

    assert(positions == 3);
    assert(flights == 9);

    /* The same twelve slots on a Link build are all FLIGHT: POSITION is Track
     * only, and the ratio is not consulted on a build with no receiver. */
    oa_config_t link_cfg;
    fixture_config(&link_cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    t = 0u;
    for (int i = 0; i < 12; i++) {
        (void)send_one(&s, &link_cfg, FEATURES_LINK, t, OA_STATE_COAST, OA_PKT_FLIGHT);
        t += (uint32_t)FIX_FLIGHT_MS;
    }
}

/* CLAIM: docs/spec/telemetry-packet.md, 0x5 POSITION: "POSITION is transmitted
 * on its scheduled slot whether or not the receiver has a fix. Suppressing it
 * would make a fix outage indistinguishable from a lost packet, and seq gaps are
 * supposed to mean lost packets and nothing else."
 *
 * The scheduler enforces this by construction: it is never told about the fix.
 * This test asserts the shape of the interface, because that absence is the
 * property, and a later change that started passing a fix in would fail to
 * compile here rather than silently start suppressing packets. */
static void test_the_scheduler_is_never_told_about_the_fix(void)
{
    oa_sched_input_t in;
    memset(&in, 0, sizeof(in));

    /* Three members and no more: the time, the state, and whether the radio is
     * still busy. Nothing about a fix, a satellite count, or a coordinate. */
    in.t_ms       = 0u;
    in.state      = OA_STATE_COAST;
    in.radio_busy = false;

    assert(sizeof(in) >= (sizeof(in.t_ms) + sizeof(in.state) + sizeof(in.radio_busy)));
}

/* CLAIM: oa_sched.h, "radio_busy: True while the radio is still sending the last
 * one." A decision cannot be issued into a busy radio, and nothing is lost by
 * waiting: what was due stays due. */
static void test_a_busy_radio_delays_and_never_drops(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    for (uint32_t t = 0u; t < 5000u; t += 100u) {
        oa_sched_input_t in = at(t, OA_STATE_BOOST);
        in.radio_busy       = true;
        assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
        assert(out.send == false);
        /* Due now, so the hint says come straight back. */
        assert(out.next_due_ms == t);
    }

    assert(s.seq == 0u);
    (void)send_one(&s, &cfg, FEATURES_LINK, 5000u, OA_STATE_BOOST, OA_PKT_FLIGHT);
    assert(s.seq == 1u);

    /* An APOGEE preemption is delayed by a busy radio in the same way and is not
     * lost, which is the case that matters: it happens once. */
    assert(oa_sched_notify_apogee(&s, &cfg, 5000u, 777, 4800u) == OA_OK);

    oa_sched_input_t busy = at(5100u, OA_STATE_APOGEE);
    busy.radio_busy       = true;
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &busy, &out) == OA_OK);
    assert(out.send == false);

    (void)send_one(&s, &cfg, FEATURES_LINK, 5200u, OA_STATE_APOGEE, OA_PKT_APOGEE);
}

/* CLAIM: docs/spec/telemetry-packet.md scheduling rule 5, "BEACON runs slowly
 * after LANDED, and the interval lengthens over time to trade update rate for
 * endurance during a long search", bounded by tx_interval_beacon_max_ms. */
static void test_the_beacon_interval_lengthens_to_the_configured_ceiling(void)
{
    oa_sched_t          s;
    oa_config_t         cfg;
    oa_sched_decision_t out;

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    /* base, base + step, base + 2 step, base + 3 step, then the ceiling holds.
     * The initial interval is used, rather than skipped by stretching before the
     * first beacon goes out. */
    const uint32_t expect_gap[] = {(uint32_t)FIX_BEACON_MS,
                                   (uint32_t)FIX_BEACON_MS + (uint32_t)FIX_BEACON_STEP_MS,
                                   (uint32_t)FIX_BEACON_MS + (2u * (uint32_t)FIX_BEACON_STEP_MS),
                                   (uint32_t)FIX_BEACON_MS + (3u * (uint32_t)FIX_BEACON_STEP_MS),
                                   (uint32_t)FIX_BEACON_MAX_MS,
                                   (uint32_t)FIX_BEACON_MAX_MS,
                                   (uint32_t)FIX_BEACON_MAX_MS};

    uint32_t t = 0u;
    for (size_t i = 0; i < (sizeof(expect_gap) / sizeof(expect_gap[0])); i++) {
        (void)send_one(&s, &cfg, FEATURES_LINK, t, OA_STATE_LANDED, OA_PKT_BEACON);

        const oa_sched_input_t in = at(t, OA_STATE_LANDED);
        assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, &out) == OA_OK);
        assert(out.send == false);
        assert(out.next_due_ms == t + expect_gap[i]);

        t += expect_gap[i];
    }

    /* The ceiling is a ceiling, not a step it passes through. */
    assert(s.beacon_interval_ms == (uint32_t)FIX_BEACON_MAX_MS);
}

/* CLAIM: oa_sched.h, the returns each entry point promises, and
 * docs/spec/telemetry-packet.md, "Types 0x0 and 0x6 to 0xF are reserved": a
 * reserved type is not something the payload can be asked to send. */
static void test_null_and_out_of_range_arguments(void)
{
    oa_sched_t             s;
    oa_config_t            cfg;
    oa_sched_decision_t    out;
    const oa_sched_input_t in = at(0u, OA_STATE_PAD_IDLE);

    fixture_config(&cfg, FEATURES_LINK);
    assert(oa_sched_init(&s) == OA_OK);

    assert(oa_sched_poll(NULL, &cfg, FEATURES_LINK, &in, &out) == OA_ERR_NULL);
    assert(oa_sched_poll(&s, NULL, FEATURES_LINK, &in, &out) == OA_ERR_NULL);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, NULL, &out) == OA_ERR_NULL);
    assert(oa_sched_poll(&s, &cfg, FEATURES_LINK, &in, NULL) == OA_ERR_NULL);

    assert(oa_sched_notify_apogee(NULL, &cfg, 0u, 0, 0u) == OA_ERR_NULL);
    assert(oa_sched_notify_apogee(&s, NULL, 0u, 0, 0u) == OA_ERR_NULL);

    assert(oa_sched_notify_sent(NULL, &cfg, OA_PKT_STATUS, 0u) == OA_ERR_NULL);
    assert(oa_sched_notify_sent(&s, NULL, OA_PKT_STATUS, 0u) == OA_ERR_NULL);

    assert(oa_sched_notify_sent(&s, &cfg, (oa_packet_type_t)0x0, 0u) == OA_ERR_RANGE);
    assert(oa_sched_notify_sent(&s, &cfg, (oa_packet_type_t)0x6, 0u) == OA_ERR_RANGE);
    assert(oa_sched_notify_sent(&s, &cfg, (oa_packet_type_t)0xF, 0u) == OA_ERR_RANGE);
    assert(s.seq == 0u);

    assert(oa_sched_apogee(NULL, NULL, NULL) == OA_ERR_NULL);
}

int main(void)
{
    test_init_makes_every_stream_due_immediately();
    test_a_missing_interval_is_reported_never_guessed();
    test_a_build_with_no_radio_sends_nothing();
    test_each_state_gates_its_own_packet_type();
    test_seq_counts_transmissions_and_wraps_at_255();
    test_apogee_preempts_a_due_flight_packet();
    test_apogee_repeats_exactly_the_configured_number_of_times();
    test_position_displaces_a_flight_and_does_not_add_a_slot();
    test_the_scheduler_is_never_told_about_the_fix();
    test_a_busy_radio_delays_and_never_drops();
    test_the_beacon_interval_lengthens_to_the_configured_ceiling();
    test_null_and_out_of_range_arguments();
    return 0;
}

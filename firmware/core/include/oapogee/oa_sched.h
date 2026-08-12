/*
 * oApogee transmit scheduler.
 *
 * Decides which packet type to send next, and when. It does not touch the radio,
 * does not build packets, and does not read a clock: it is told the time and it
 * returns a decision. That is what makes a whole flight's worth of scheduling
 * testable on a laptop, including the case that matters most, which happens once
 * per flight and cannot be repeated.
 *
 * The scheduling rules that are normative, from
 * docs/spec/telemetry-packet.md:
 *
 *   1. APOGEE preempts everything. It is queued the moment apogee is detected
 *      and transmitted at the next opportunity, ahead of any pending packet.
 *   2. FLIGHT runs at the fastest sustainable rate from BOOST through DESCENT.
 *   3. POSITION is interleaved at a fraction of the FLIGHT rate on Track builds.
 *   4. STATUS runs slowly during PAD_IDLE and ARMED.
 *   5. BEACON runs slowly after LANDED, and the interval lengthens over time to
 *      trade update rate for endurance during a long search.
 *
 * WHY APOGEE PREEMPTS
 *
 * Apogee is the number the entire payload exists to produce, and it is the
 * number most likely to be lost. It happens once, at the greatest distance from
 * the receiver, and it is immediately followed by the deployment event that is
 * the most likely moment for the payload to be damaged. Waiting for the next
 * scheduled slot would spend that margin for nothing.
 *
 * THE SCHEDULER CAN ONLY EMIT THE FIVE DOWNLINK TYPES. There is no queue entry
 * that means anything but "send this telemetry", there is no inbound path that
 * can add one, and there is no packet type in the format that commands the
 * vehicle. See firmware/SAFETY.md.
 *
 * Every interval this scheduler uses is unmeasured and lives in oa_config_t as
 * unset. It reports OA_ERR_UNSET rather than falling back to a number, because
 * a transmit interval chosen without measured airtime is how a payload exceeds
 * a regional duty cycle limit without anyone noticing.
 *
 * Nothing in this file has run on hardware, and no packet has been transmitted.
 */

#ifndef OAPOGEE_OA_SCHED_H
#define OAPOGEE_OA_SCHED_H

#include "oa_states.h"
#include "oapogee/oa_config.h"
#include "oapogee/oa_packet.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* When each recurring stream is next due, milliseconds since arming. */
    uint32_t next_status_ms;
    uint32_t next_flight_ms;
    uint32_t next_beacon_ms;

    /* The current beacon interval, which lengthens by tx_beacon_stretch_ms after
     * each beacon up to tx_interval_beacon_max_ms. Held here rather than
     * recomputed from a count so the stretching is visible in one place. */
    uint32_t beacon_interval_ms;

    /* FLIGHT packets sent since the last POSITION, for the interleave. Counting
     * packets rather than time keeps the ratio exact when the flight rate is
     * disturbed by an APOGEE preemption. */
    uint32_t flight_since_position;

    /* The APOGEE preemption. `pending` is how many repeats are still owed, and
     * it is set once when apogee is detected. */
    uint32_t apogee_pending;
    uint32_t apogee_next_ms;
    int32_t  apogee_cm;
    uint32_t apogee_t_ms;
    bool     apogee_known;

    uint8_t seq;
} oa_sched_t;

typedef struct {
    uint32_t   t_ms;      /* Milliseconds since arming. */
    oa_state_t state;
    bool       radio_busy; /* True while the radio is still sending the last one. */
} oa_sched_input_t;

typedef struct {
    /* False when nothing is due. The caller does nothing and polls again. */
    bool send;

    /* Which packet to build. Only meaningful when send is true. */
    oa_packet_type_t type;

    /* The sequence number to put in the header. Handed out here rather than
     * incremented by the caller, so that seq counts transmitted packets exactly
     * and a receiver's gap count means lost packets and nothing else. */
    uint8_t seq;

    /* When to poll again at the latest, milliseconds since arming. A caller that
     * polls sooner gets a correct answer; this is a hint that lets a caller
     * sleep. */
    uint32_t next_due_ms;
} oa_sched_decision_t;

/* Reset. Every stream becomes due immediately in its own state, so a payload
 * that powers up on the pad transmits a STATUS packet without waiting a full
 * interval for its first sign of life. */
oa_result_t oa_sched_init(oa_sched_t *s);

/* Tell the scheduler apogee was detected. Queues tx_apogee_repeat APOGEE packets
 * spaced by tx_apogee_repeat_interval_ms, the first of which preempts whatever
 * else is due.
 *
 * Calling this more than once in a flight is a programming error and returns
 * OA_ERR_STATE, rather than requeueing. Apogee happens once. */
oa_result_t oa_sched_notify_apogee(oa_sched_t *s,
                                   const oa_config_t *cfg,
                                   uint32_t t_ms,
                                   int32_t apogee_cm,
                                   uint32_t t_apogee_ms);

/* Ask what to send now.
 *
 * Returns OA_OK with out->send set or clear, OA_ERR_NULL, or OA_ERR_UNSET when a
 * required interval is missing from the configuration for this build's features.
 *
 * Selection order, which is the normative rule list above in code:
 *   APOGEE if any repeat is owed and its time has come
 *   then, by state:
 *     PAD_IDLE, ARMED       STATUS at tx_interval_pad_ms
 *     BOOST, COAST, APOGEE, DESCENT
 *                           FLIGHT at tx_interval_flight_ms, with POSITION
 *                           displacing every tx_position_interleave-th FLIGHT on
 *                           a build with OA_FEATURE_GNSS
 *     LANDED                BEACON at the current beacon interval
 *
 * POSITION displaces a FLIGHT rather than being added between them, because
 * airtime is the scarce resource and adding packets would silently reduce the
 * flight rate instead of the caller choosing to.
 *
 * A POSITION packet is emitted on its slot whether or not there is a fix.
 * Suppressing it would make a fix outage indistinguishable from a lost packet,
 * and seq gaps are supposed to mean lost packets and nothing else. */
oa_result_t oa_sched_poll(oa_sched_t *s,
                          const oa_config_t *cfg,
                          oa_features_t features,
                          const oa_sched_input_t *in,
                          oa_sched_decision_t *out);

/* Confirm a packet actually went out, so the schedule advances and seq
 * increments. Split from poll because a decision the radio then refused must not
 * count as a transmission: seq would gain a gap that no receiver lost. */
oa_result_t oa_sched_notify_sent(oa_sched_t *s, const oa_config_t *cfg, oa_packet_type_t type, uint32_t t_ms);

/* The apogee the scheduler was told about, for the BEACON body. Returns
 * OA_ERR_EMPTY if apogee has not been detected, rather than zero, because zero
 * is a legitimate apogee for a flight that never left the pad. */
oa_result_t oa_sched_apogee(const oa_sched_t *s, int32_t *out_apogee_cm, uint32_t *out_t_apogee_ms);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_SCHED_H */

/*
 * oApogee core: the transmit scheduler.
 *
 * The implementation of oapogee/oa_sched.h. It does not touch the radio, does
 * not build packets and does not read a clock. It is told the time and the
 * flight state and it answers with a packet type and a sequence number.
 *
 * THIS FILE CONTAINS NO INTERVAL.
 *
 * Every interval, repeat count and interleave ratio arrives from oa_config_t,
 * and every one of those is unset until somebody measures the airtime of a
 * packet at the shipped radio configuration. Where one is missing this returns
 * OA_ERR_UNSET rather than falling back to a number, because a transmit interval
 * chosen without measured airtime is how a payload exceeds a regional duty cycle
 * limit without anyone noticing.
 *
 * THE SCHEDULER CAN ONLY EMIT THE FIVE DOWNLINK TYPES. Its output is an
 * oa_packet_type_t and nothing else. There is no queue entry that means anything
 * but "send this telemetry", no inbound path that could add one, and no packet
 * type in the format that commands the vehicle. See firmware/SAFETY.md.
 *
 * WHERE THE TIMEBASE COMES FROM, AND WHAT THE CALLER OWES
 *
 * in->t_ms is milliseconds since arming. Before arming there is no such
 * quantity, so the caller feeds this scheduler whatever monotonic millisecond
 * count it has, and that count restarts at zero on the transition into ARMED.
 * This module holds no clock and cannot notice the restart, so the caller
 * re-initialises it on that transition. oa_sched_init makes every stream due
 * immediately, which is exactly what is wanted there: the first STATUS packet of
 * the armed run goes out at once rather than an interval later.
 *
 * Nothing in this file has run on hardware and no packet has been transmitted.
 *
 * TODO(verify): publish the measured intervals for each state at the shipped
 * radio configuration, together with the resulting duty cycle, so anyone
 * checking a regional duty cycle limit can do the arithmetic.
 * docs/spec/telemetry-packet.md carries the same marker. This matters for EU and
 * UK builds in particular.
 */

#include "oapogee/oa_sched.h"

/* ---------------------------------------------------------------------------
 * Small helpers.
 * ------------------------------------------------------------------------ */

/* Read a millisecond interval out of the configuration.
 *
 * out_ms may be NULL, which is how the poll path validates a field it is not
 * about to use the value of.
 *
 * OA_ERR_UNSET for a field nobody has measured, and OA_ERR_RANGE for a negative
 * one. They are separated because they are different mistakes: the first is the
 * expected state of this firmware today, and the second is a configuration file
 * that says something impossible. Neither is replaced with a guess. */
static oa_result_t oa_sched_interval_ms(oa_tunable_t value, uint32_t *out_ms)
{
    if (!OA_IS_SET(value)) {
        return OA_ERR_UNSET;
    }
    if (value < 0) {
        return OA_ERR_RANGE;
    }
    if (out_ms != NULL) {
        *out_ms = (uint32_t)value;
    }
    return OA_OK;
}

/* Saturating increment, for the FLIGHT counter that feeds the POSITION
 * interleave. On a build with no GNSS it is never reset, so it would otherwise
 * wrap after about 49 days of continuous flight packets. Wrapping is defined
 * behaviour and would be harmless here, and saturating costs one comparison and
 * removes the need for a reader to work that out. */
static uint32_t oa_sched_count_inc(uint32_t count)
{
    return (count == UINT32_MAX) ? count : (count + 1u);
}

/* STATUS runs during PAD_IDLE and ARMED. */
static bool oa_sched_state_is_pad(oa_state_t state)
{
    return (state == OA_STATE_PAD_IDLE) || (state == OA_STATE_ARMED);
}

/* FLIGHT runs from BOOST through DESCENT, which includes the APOGEE state. The
 * APOGEE packet preempting the schedule is a separate thing from the APOGEE
 * flight state, and the flight stream does not pause for it. */
static bool oa_sched_state_is_flight(oa_state_t state)
{
    return (state == OA_STATE_BOOST) || (state == OA_STATE_COAST) || (state == OA_STATE_APOGEE)
           || (state == OA_STATE_DESCENT);
}

/* The interval the next BEACON uses.
 *
 * beacon_interval_ms is zero after oa_sched_init, and zero means it has not been
 * seeded from the configuration yet. oa_sched_init takes no configuration, by
 * design: it is a reset and it must work before anything is known. A configured
 * beacon interval of 0 ms would mean transmitting continuously, which is not a
 * setting anybody means, so zero is free to carry the other meaning.
 *
 * Mutates the scheduler, which is why it takes a non-const pointer. The
 * stretching lives in one place and this is it. */
static oa_result_t oa_sched_beacon_interval_ms(oa_sched_t *s, const oa_config_t *cfg, uint32_t *out_ms)
{
    if (s->beacon_interval_ms == 0u) {
        uint32_t          base = 0u;
        const oa_result_t rc   = oa_sched_interval_ms(cfg->tx_interval_beacon_ms, &base);
        if (rc != OA_OK) {
            return rc;
        }
        s->beacon_interval_ms = base;
    }

    *out_ms = s->beacon_interval_ms;
    return OA_OK;
}

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

oa_result_t oa_sched_init(oa_sched_t *s)
{
    if (s == NULL) {
        return OA_ERR_NULL;
    }

    /* Whole-struct initialisation, so a member added later starts cleared rather
     * than starting undefined in whichever build first forgot it. Every
     * next_*_ms becomes 0, which is due immediately: a payload that powers up on
     * the pad transmits a STATUS packet without waiting a full interval for its
     * first sign of life. The first packet of the run carries seq 0. */
    const oa_sched_t cleared = {0};
    *s = cleared;

    return OA_OK;
}

oa_result_t oa_sched_notify_apogee(oa_sched_t *s,
                                   const oa_config_t *cfg,
                                   uint32_t t_ms,
                                   int32_t apogee_cm,
                                   uint32_t t_apogee_ms)
{
    if ((s == NULL) || (cfg == NULL)) {
        return OA_ERR_NULL;
    }

    /* Apogee happens once. Requeueing on a second call would restart the
     * repeats, and the second caller would be a bug reporting an event that
     * cannot happen twice. */
    if (s->apogee_known) {
        return OA_ERR_STATE;
    }

    /* The number is recorded before anything below can fail. It is what the
     * whole payload exists to produce, the BEACON body carries it after landing
     * so that a walkaway recovery still yields it, and a missing transmit
     * interval must not cost the number as well as the packet. */
    s->apogee_cm    = apogee_cm;
    s->apogee_t_ms  = t_apogee_ms;
    s->apogee_known = true;

    if (!OA_IS_SET(cfg->tx_apogee_repeat)) {
        return OA_ERR_UNSET;
    }

    /* A repeat count below one queues no APOGEE packet at all, which would
     * silently drop the packet type that exists because this number is the one
     * most likely to be lost. That is reported rather than obeyed, so it shows
     * up as a fault the operator can see instead of as a packet nobody notices
     * was never sent. */
    if (cfg->tx_apogee_repeat < 1) {
        return OA_ERR_RANGE;
    }

    /* The spacing is only needed when there is a second packet to space, so it
     * is not required to queue a single one. */
    if (cfg->tx_apogee_repeat > 1) {
        const oa_result_t rc = oa_sched_interval_ms(cfg->tx_apogee_repeat_interval_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
    }

    s->apogee_pending = (uint32_t)cfg->tx_apogee_repeat;

    /* Due now. Rule 1: APOGEE is queued the moment apogee is detected and
     * transmitted at the next opportunity, ahead of any pending packet. */
    s->apogee_next_ms = t_ms;

    return OA_OK;
}

oa_result_t oa_sched_poll(oa_sched_t *s,
                          const oa_config_t *cfg,
                          oa_features_t features,
                          const oa_sched_input_t *in,
                          oa_sched_decision_t *out)
{
    if ((s == NULL) || (cfg == NULL) || (in == NULL) || (out == NULL)) {
        return OA_ERR_NULL;
    }

    /* Answer first with "nothing to send", then narrow it. Every early return
     * below therefore leaves a complete decision behind rather than a partly
     * filled one. `type` is only meaningful when send is true; it is written
     * anyway so the struct never carries an indeterminate value. */
    out->send        = false;
    out->type        = OA_PKT_STATUS;
    out->seq         = s->seq;
    out->next_due_ms = in->t_ms;

    /* A build with no radio transmits nothing, ever. This is not an error: a
     * Solo payload is a complete payload, and reporting a fault because it has
     * no spreading factor configured would be a validator nobody trusts. */
    if ((features & (oa_features_t)OA_FEATURE_RADIO) == 0u) {
        return OA_OK;
    }

    /* The recurring stream for this state, if it has one.
     *
     * The configuration this state needs is validated here, before any decision
     * is made, so a missing interval is reported whether or not an APOGEE
     * preemption happens to be covering for it. */
    bool             have_slot = false;
    uint32_t         slot_due_ms = 0u;
    oa_packet_type_t slot_type = OA_PKT_STATUS;

    if (oa_sched_state_is_pad(in->state)) {
        /* Rule 4: STATUS runs slowly during PAD_IDLE and ARMED. */
        const oa_result_t rc = oa_sched_interval_ms(cfg->tx_interval_pad_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
        have_slot   = true;
        slot_due_ms = s->next_status_ms;
        slot_type   = OA_PKT_STATUS;
    } else if (oa_sched_state_is_flight(in->state)) {
        /* Rule 2: FLIGHT runs at the fastest sustainable rate from BOOST through
         * DESCENT. */
        const oa_result_t rc = oa_sched_interval_ms(cfg->tx_interval_flight_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
        have_slot   = true;
        slot_due_ms = s->next_flight_ms;
        slot_type   = OA_PKT_FLIGHT;

        /* Rule 3: POSITION is interleaved at a fraction of the FLIGHT rate on
         * Track builds. It displaces a FLIGHT rather than being added between
         * them, because airtime is the scarce resource: adding packets would
         * reduce the flight rate silently instead of the operator choosing to.
         *
         * The unit of tx_position_interleave is FLIGHT packets per POSITION, so
         * with the ratio at 3 the pattern is FLIGHT, FLIGHT, FLIGHT, POSITION.
         *
         * A POSITION goes out on its slot whether or not there is a fix. The
         * scheduler is not told about the fix at all, which is what makes that
         * unconditional: suppressing the packet would make a fix outage
         * indistinguishable from a lost packet, and a seq gap is supposed to
         * mean a lost packet and nothing else. */
        if ((features & (oa_features_t)OA_FEATURE_GNSS) != 0u) {
            if (!OA_IS_SET(cfg->tx_position_interleave)) {
                return OA_ERR_UNSET;
            }

            /* A ratio below one is a configuration error, and it is ambiguous:
             * it could mean every slot or no slot. It resolves toward FLIGHT
             * here, because FLIGHT is the packet the flight is for. */
            if ((cfg->tx_position_interleave > 0)
                && (s->flight_since_position >= (uint32_t)cfg->tx_position_interleave)) {
                slot_type = OA_PKT_POSITION;
            }
        }
    } else if (in->state == OA_STATE_LANDED) {
        /* Rule 5: BEACON runs slowly after LANDED, and the interval lengthens
         * over time to trade update rate for endurance during a long search. All
         * three numbers are validated here, not at the moment the interval next
         * stretches, so an incomplete beacon configuration is reported on the
         * first poll after landing rather than several beacons into a search. */
        oa_result_t rc = oa_sched_interval_ms(cfg->tx_interval_beacon_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
        rc = oa_sched_interval_ms(cfg->tx_beacon_stretch_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
        rc = oa_sched_interval_ms(cfg->tx_interval_beacon_max_ms, NULL);
        if (rc != OA_OK) {
            return rc;
        }
        have_slot   = true;
        slot_due_ms = s->next_beacon_ms;
        slot_type   = OA_PKT_BEACON;
    } else {
        /* A state value this firmware's own state machine cannot produce. There
         * is no stream for it, and inventing one would be worse than sending
         * nothing. */
        have_slot = false;
    }

    /* The polling hint, which a caller uses to decide how long it may sleep. It
     * is the earlier of the recurring slot and any owed APOGEE repeat, never
     * earlier than now. */
    uint32_t next_due_ms = in->t_ms;
    if (have_slot) {
        next_due_ms = slot_due_ms;
    }
    if ((s->apogee_pending > 0u) && (!have_slot || (s->apogee_next_ms < next_due_ms))) {
        next_due_ms = s->apogee_next_ms;
    }
    if (next_due_ms < in->t_ms) {
        next_due_ms = in->t_ms;
    }
    out->next_due_ms = next_due_ms;

    /* The radio is still sending the last one. The schedule is unchanged and
     * nothing is dropped: whatever was due stays due, and the hint above already
     * says to poll again immediately. */
    if (in->radio_busy) {
        return OA_OK;
    }

    /* Rule 1: APOGEE preempts everything.
     *
     * Apogee is the number the entire payload exists to produce and the one most
     * likely to be lost. It happens once, at the greatest distance from the
     * receiver, immediately before the deployment event that is the most likely
     * moment for the payload to be damaged. A displaced FLIGHT packet is not
     * dropped: next_flight_ms is left in the past, so the next poll offers it
     * again. */
    if ((s->apogee_pending > 0u) && (in->t_ms >= s->apogee_next_ms)) {
        out->send = true;
        out->type = OA_PKT_APOGEE;
        /* The schedule does not advance until the send is confirmed, so there is
         * nothing to wait for. */
        out->next_due_ms = in->t_ms;
        return OA_OK;
    }

    if (have_slot && (in->t_ms >= slot_due_ms)) {
        out->send        = true;
        out->type        = slot_type;
        out->next_due_ms = in->t_ms;
        return OA_OK;
    }

    return OA_OK;
}

oa_result_t oa_sched_notify_sent(oa_sched_t *s, const oa_config_t *cfg, oa_packet_type_t type, uint32_t t_ms)
{
    if ((s == NULL) || (cfg == NULL)) {
        return OA_ERR_NULL;
    }

    uint32_t    interval_ms = 0u;
    oa_result_t rc          = OA_OK;

    /* Nothing below increments seq until the stream it belongs to has advanced
     * without error, so a failure here leaves the scheduler exactly as it was.
     *
     * OA_PKT_TYPE_COUNT is not a case: it has the same value as OA_PKT_POSITION,
     * so a label for it would be a duplicate. That collision is noted in the
     * report on this module. */
    switch (type) {
    case OA_PKT_STATUS:
        rc = oa_sched_interval_ms(cfg->tx_interval_pad_ms, &interval_ms);
        if (rc != OA_OK) {
            return rc;
        }
        s->next_status_ms = t_ms + interval_ms;
        break;

    case OA_PKT_FLIGHT:
        rc = oa_sched_interval_ms(cfg->tx_interval_flight_ms, &interval_ms);
        if (rc != OA_OK) {
            return rc;
        }
        s->next_flight_ms        = t_ms + interval_ms;
        s->flight_since_position = oa_sched_count_inc(s->flight_since_position);
        break;

    case OA_PKT_POSITION:
        /* POSITION displaced a FLIGHT rather than being added between two, so it
         * consumed the FLIGHT slot and the FLIGHT interval is what advances. The
         * counter is what keeps the ratio exact when an APOGEE preemption
         * disturbs the flight rate, which is why it counts packets and not
         * time. */
        rc = oa_sched_interval_ms(cfg->tx_interval_flight_ms, &interval_ms);
        if (rc != OA_OK) {
            return rc;
        }
        s->next_flight_ms        = t_ms + interval_ms;
        s->flight_since_position = 0u;
        break;

    case OA_PKT_APOGEE:
        /* Confirming an APOGEE packet that was never queued is a programming
         * error, not a schedule to advance. */
        if (s->apogee_pending == 0u) {
            return OA_ERR_STATE;
        }
        if (s->apogee_pending > 1u) {
            rc = oa_sched_interval_ms(cfg->tx_apogee_repeat_interval_ms, &interval_ms);
            if (rc != OA_OK) {
                return rc;
            }
            s->apogee_next_ms = t_ms + interval_ms;
        }
        s->apogee_pending -= 1u;
        break;

    case OA_PKT_BEACON: {
        uint32_t stretch_ms = 0u;
        uint32_t max_ms     = 0u;

        rc = oa_sched_beacon_interval_ms(s, cfg, &interval_ms);
        if (rc != OA_OK) {
            return rc;
        }
        rc = oa_sched_interval_ms(cfg->tx_beacon_stretch_ms, &stretch_ms);
        if (rc != OA_OK) {
            return rc;
        }
        rc = oa_sched_interval_ms(cfg->tx_interval_beacon_max_ms, &max_ms);
        if (rc != OA_OK) {
            return rc;
        }

        /* The beacon just sent used the current interval, so that is the gap to
         * the next one. Then the interval lengthens, for the one after. Gaps
         * therefore run base, base + stretch, base + 2 stretch, up to the
         * ceiling, rather than starting at the stretched value and never using
         * the configured initial interval at all. */
        s->next_beacon_ms = t_ms + interval_ms;

        if (stretch_ms > (UINT32_MAX - interval_ms)) {
            interval_ms = UINT32_MAX;
        } else {
            interval_ms += stretch_ms;
        }
        if (interval_ms > max_ms) {
            interval_ms = max_ms;
        }
        s->beacon_interval_ms = interval_ms;
        break;
    }

    default:
        /* A value that is not one of the five downlink types. Types 0x0 and 0x6
         * to 0xF are reserved by the format and are not something this payload
         * can be asked to send. */
        return OA_ERR_RANGE;
    }

    /* seq counts transmitted packets exactly, which is what lets a receiver's
     * gap count mean lost packets and nothing else. It is handed out by poll and
     * only advanced here, so a decision the radio then refused does not leave a
     * gap no receiver lost. It wraps at 255, which the format requires and which
     * is unambiguous at any realistic packet rate. */
    s->seq = (uint8_t)((s->seq + 1u) & 0xFFu);

    return OA_OK;
}

oa_result_t oa_sched_apogee(const oa_sched_t *s, int32_t *out_apogee_cm, uint32_t *out_t_apogee_ms)
{
    if (s == NULL) {
        return OA_ERR_NULL;
    }

    /* Not zero. Zero is a legitimate apogee for a flight that never left the
     * pad, and a BEACON carrying zero because nothing was detected would be
     * indistinguishable from one carrying a measurement. */
    if (!s->apogee_known) {
        return OA_ERR_EMPTY;
    }

    if (out_apogee_cm != NULL) {
        *out_apogee_cm = s->apogee_cm;
    }
    if (out_t_apogee_ms != NULL) {
        *out_t_apogee_ms = s->apogee_t_ms;
    }

    return OA_OK;
}

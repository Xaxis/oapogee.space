/*
 * oApogee flight state machine.
 *
 * A pure step function. All of its state is in a context the caller owns, it
 * reads no clock and no sensor, and it returns a decision rather than acting on
 * one. That is what lets a test drive a whole flight through it from a table of
 * samples, on a laptop, and check every transition, including the ones that only
 * happen when a sensor fails.
 *
 * The states and their order come from data/flight-phases.yaml through
 * gen/oa_states.h. The transition criteria are described in prose there because
 * the shape of each rule is a design decision that can be reasoned about on
 * paper. Every number those rules need is unmeasured and lives in oa_config_t as
 * unset, which is why this machine will not leave PAD_IDLE without a
 * flightworthy configuration.
 *
 * THIS MACHINE DRIVES NOTHING. It reports what state the payload is in and
 * whether apogee has been detected. It has no output that touches hardware, and
 * apogee is a measurement, not a command. See firmware/SAFETY.md.
 *
 * Nothing in this file has run on hardware, and no flight has been flown.
 */

#ifndef OAPOGEE_OA_STATE_H
#define OAPOGEE_OA_STATE_H

#include "oa_states.h"
#include "oapogee/oa_config.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Inputs.
 *
 * One sample's worth of everything the machine needs. The caller supplies the
 * fused altitude and velocity rather than the machine computing them, so that
 * fusion and detection can be tested separately: a bad transition is then either
 * the filter's fault or the machine's, and never ambiguous.
 *
 * `baro_valid` is explicit for the same reason it is explicit in oa_fusion.h. A
 * barometer that has failed still returns numbers, and a detector that cannot
 * tell the difference between a sensor that is quiet and a sensor that is dead
 * will call apogee on a dead sensor.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* Milliseconds since arming. Monotonic. Before arming the caller passes 0,
     * and the machine does not use it. */
    uint32_t t_ms;

    /* Fused altitude above the pad reference, centimetres. Signed, and negative
     * values are legitimate: a rocket can land below the pad, and pressure can
     * rise during a flight. */
    int32_t alt_cm;

    /* Fused vertical velocity, decimetres per second. Positive is up. */
    int32_t vel_dm_s;

    /* Vertical acceleration, hundredths of g, with gravity already removed by
     * the caller. Positive is up. From the high-g part when it is fitted and
     * healthy, from the IMU otherwise, in which case it may be saturated. */
    int32_t accel_cg;

    /* False when the barometer has failed or is returning implausible values.
     * When this is false the machine must not confirm apogee from altitude
     * alone. */
    bool baro_valid;

    /* The arming input, already debounced by the caller. PAD_IDLE to ARMED is
     * the one operator-driven transition. */
    bool operator_armed;

    /* True once the pad pressure reference has been locked by oa_baro_ref. The
     * machine will not leave ARMED before this, because a launch detected
     * against an unfinished reference produces a flight measured from the wrong
     * zero. */
    bool pad_reference_locked;

    /* True when onboard storage is full. Recorded so the machine can be asked
     * about it later; it never changes a transition, because a flight does not
     * end because the flash filled up. */
    bool log_full;
} oa_state_input_t;

/* ---------------------------------------------------------------------------
 * Context.
 *
 * Opaque in intent, visible in practice so it can live in a caller's static
 * storage without an allocator. Treat the members as private: the tests drive
 * this through oa_state_step and inspect it through the accessors, and an
 * implementation is free to change what is in here.
 * ------------------------------------------------------------------------ */

typedef struct {
    oa_state_t state;

    /* Consecutive-sample counters for the confirmed transitions. Each is
     * compared against a count from the configuration, and each resets on any
     * sample that does not satisfy the condition, because these rules are about
     * consecutive evidence and not about a total. */
    uint32_t launch_run;
    uint32_t burnout_run;
    uint32_t apogee_run;
    uint32_t descent_run;

    /* When the landing condition first became true, and whether it currently is,
     * for the sustained-period rule. */
    uint32_t landing_since_ms;
    bool     landing_pending;

    /* Peak altitude seen so far, and when. This is the number the whole payload
     * exists to produce. It is tracked continuously rather than sampled at the
     * transition, because detection is confirmed across several samples and by
     * then the peak is already behind. */
    int32_t  peak_alt_cm;
    uint32_t peak_t_ms;
    bool     peak_valid;

    /* Set once, when APOGEE is entered, so the scheduler is told exactly once. */
    bool apogee_reported;

    /* The last input timestamp, so the machine can reject a sample that went
     * backwards rather than computing a negative interval. */
    uint32_t last_t_ms;
    bool     have_last;
} oa_state_ctx_t;

/* ---------------------------------------------------------------------------
 * Outputs.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* State after this step. */
    oa_state_t state;

    /* True on the step where the state changed. The caller uses this to switch
     * log rate, switch transmit schedule, and sync the sink. */
    bool changed;

    /* True on the single step where apogee was first confirmed. The apogee
     * fields below are only meaningful when this is true or when the machine has
     * already passed APOGEE. */
    bool apogee_detected;

    /* Peak altitude and the estimated time of the apogee event. t_apogee_ms is
     * earlier than the t_ms of the step that reports it, and the difference is
     * the detection lag. That lag is a real error in the recorded apogee time,
     * and the packet format carries the event time separately from the packet
     * time so a receiver can show it. */
    int32_t  apogee_cm;
    uint32_t t_apogee_ms;
} oa_state_output_t;

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

/* Reset to PAD_IDLE with every counter cleared and no peak recorded. */
oa_result_t oa_state_init(oa_state_ctx_t *ctx);

/* Advance one sample.
 *
 * Returns OA_OK, OA_ERR_NULL, or OA_ERR_STATE if the sample's timestamp went
 * backwards.
 *
 * The transitions, in the order the machine checks them:
 *
 *   PAD_IDLE -> ARMED    operator_armed, and only if the configuration is
 *                        flightworthy for this build's features. A payload
 *                        whose thresholds are unmeasured refuses to arm, and
 *                        refusing is correct: it is how this firmware avoids
 *                        flying a guess.
 *   ARMED    -> BOOST    pad_reference_locked, and accel_cg at or above
 *                        launch_accel_threshold_cg for launch_confirm_samples
 *                        consecutive samples.
 *   BOOST    -> COAST    accel_cg through zero for burnout_confirm_samples
 *                        consecutive samples, with the optional hysteresis band
 *                        applied when it is set.
 *   COAST    -> APOGEE   altitude decreasing for apogee_confirm_samples
 *                        consecutive samples, and baro_valid.
 *   APOGEE   -> DESCENT  sustained descent confirmed for
 *                        descent_confirm_samples consecutive samples.
 *   DESCENT  -> LANDED   altitude within landing_alt_band_cm of the reference
 *                        and acceleration within landing_accel_band_cg, both
 *                        held for landing_hold_ms.
 *   LANDED   -> nothing. Terminal until a power cycle.
 *
 * There is no transition that skips a state and no transition backwards. The
 * machine cannot be commanded into a state, and there is no entry point that
 * sets one, because the only thing outside the payload that can reach this
 * machine is the arming switch. */
oa_result_t oa_state_step(oa_state_ctx_t *ctx,
                          const oa_config_t *cfg,
                          oa_features_t features,
                          const oa_state_input_t *in,
                          oa_state_output_t *out);

/* Current state without stepping, for the logger and the packet header. */
oa_state_t oa_state_current(const oa_state_ctx_t *ctx);

/* Peak altitude so far and when it occurred. Returns OA_ERR_EMPTY before any
 * sample has been seen, rather than reporting a peak of zero, because zero is a
 * legitimate altitude and "no measurement yet" is not the same thing. */
oa_result_t oa_state_peak(const oa_state_ctx_t *ctx, int32_t *out_alt_cm, uint32_t *out_t_ms);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_STATE_H */

/*
 * oApogee core: the flight state machine.
 *
 * The implementation of oapogee/oa_state.h. A pure step function: it reads no
 * clock, no sensor and no global, it holds all of its state in a context the
 * caller owns, and it returns a decision rather than acting on one.
 *
 * THIS FILE CONTAINS NO THRESHOLD.
 *
 * Every number that decides a transition arrives from oa_config_t, and every one
 * of those is unset until somebody measures it. The only integer literals below
 * are counter resets, the saturation limit on a counter, and zero used as the
 * sign of an acceleration or a velocity. None of those is a tunable: the test is
 * whether measuring something would change the number, and nothing anyone
 * measures moves the sign change that is burnout.
 *
 * The consequence is that a payload whose configuration is incomplete will not
 * leave PAD_IDLE. That is deliberate and it is the point. See the note on
 * refusing to arm, below.
 *
 * THIS MACHINE DRIVES NOTHING. It reports a state and a measured apogee. It
 * cannot name an output function, because core/ is compiled without
 * port/include on its include path and every external symbol core references is
 * checked against core/allowed-undefined.txt. Apogee detection produces a
 * number, not a command. See firmware/SAFETY.md.
 *
 * Nothing in this file has run on hardware, no board has been fabricated, and no
 * flight has been flown. Every transition below is untested against anything but
 * the sample tables in firmware/test/test_oa_state.c.
 *
 * TODO(verify): state the apogee detection lag in milliseconds, which is
 * apogee_confirm_samples divided by the achieved sample rate, once both have
 * been measured on real hardware. That lag is the error between t_apogee_ms and
 * the header t_ms of the packet reporting it, docs/spec/telemetry-packet.md
 * carries the same marker, and a reader is entitled to know its size.
 */

#include "oapogee/oa_state.h"

/* ---------------------------------------------------------------------------
 * Small helpers.
 *
 * All static, all pure except where noted, and none of them holds state. The
 * point of pulling them out is that each transition below then reads as the one
 * sentence of criteria that data/flight-phases.yaml states for it.
 * ------------------------------------------------------------------------ */

/* Saturating increment for a consecutive-sample counter.
 *
 * Unsigned overflow is defined and wraps, and a wrap here would silently reset a
 * run that had already satisfied its count, which is a transition that fails to
 * happen rather than a crash. Saturating costs one comparison per sample. */
static uint32_t oa_run_inc(uint32_t run)
{
    return (run == UINT32_MAX) ? run : (run + 1u);
}

/* Whether a run of consecutive satisfying samples has met its configured count.
 *
 * False when the count is unset, which is what stops the machine advancing on a
 * threshold nobody has measured. False also when the count is below one sample,
 * because confirming across zero samples is not confirmation, and the whole
 * reason these rules count samples is that barometric noise alone will produce a
 * descending pair at any point near the top of a flight. */
static bool oa_confirmed(uint32_t run, oa_tunable_t count)
{
    if (!OA_IS_SET(count) || (count < 1)) {
        return false;
    }
    return run >= (uint32_t)count;
}

/* Whether `v` is inside plus or minus `band`.
 *
 * False when the band is unset, so a landing detector with no measured band
 * never fires. False also for a negative band, which is not a band. The negation
 * cannot overflow: OA_UNSET is INT32_MIN and is excluded by OA_IS_SET, and
 * oa_config_set refuses to store OA_UNSET as a value. */
static bool oa_within_band(int32_t v, oa_tunable_t band)
{
    if (!OA_IS_SET(band) || (band < 0)) {
        return false;
    }
    return (v <= band) && (v >= -band);
}

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

oa_result_t oa_state_init(oa_state_ctx_t *ctx)
{
    if (ctx == NULL) {
        return OA_ERR_NULL;
    }

    /* Whole-struct initialisation rather than field by field, so a member added
     * to the context later starts cleared instead of starting undefined in
     * whichever build first forgot it. PAD_IDLE is 0, so this is also the
     * correct starting state. */
    const oa_state_ctx_t cleared = {0};
    *ctx = cleared;

    return OA_OK;
}

oa_result_t oa_state_step(oa_state_ctx_t *ctx,
                          const oa_config_t *cfg,
                          oa_features_t features,
                          const oa_state_input_t *in,
                          oa_state_output_t *out)
{
    if ((ctx == NULL) || (cfg == NULL) || (in == NULL) || (out == NULL)) {
        return OA_ERR_NULL;
    }

    /* A sample whose timestamp went backwards is rejected rather than used to
     * compute a negative interval. `out` is deliberately not written: a caller
     * that ignores the return code should not be handed a state that was never
     * computed. */
    if (ctx->have_last && (in->t_ms < ctx->last_t_ms)) {
        return OA_ERR_STATE;
    }

    const oa_state_t before = ctx->state;
    bool              armed_this_step = false;
    bool              apogee_this_step = false;

    /* The peak, tracked continuously.
     *
     * Detection is confirmed across several samples, so by the time apogee is
     * called the peak is already behind and sampling the altitude at the
     * transition would report a value that is low by the detection lag. Tracking
     * it every sample is what makes t_apogee_ms an estimate of the event rather
     * than a record of when the detector noticed.
     *
     * Nothing is tracked in PAD_IDLE, because the pad pressure reference is not
     * locked until ARMED and alt_cm before that is measured against nothing.
     *
     * Tracking stops once APOGEE is reached. The deployment event happens within
     * a second or so of it and puts a pressure transient through the static
     * ports, and a transient that read as a higher altitude would revise the one
     * number the whole payload exists to produce, after the fact, upward. */
    if ((before != OA_STATE_PAD_IDLE) && (before < OA_STATE_APOGEE)) {
        if (!ctx->peak_valid || (in->alt_cm > ctx->peak_alt_cm)) {
            ctx->peak_alt_cm = in->alt_cm;
            ctx->peak_t_ms   = in->t_ms;
            ctx->peak_valid  = true;
        }
    }

    switch (before) {
    case OA_STATE_PAD_IDLE: {
        /* The one operator-driven transition, and the one gate that matters.
         *
         * oa_config_is_flightworthy is what turns "no threshold has been
         * measured yet" from a sentence in a specification into a payload that
         * will not arm. A payload that refuses to arm is telling the truth about
         * what it knows.
         *
         * The report is not requested here because oa_state_step has no channel
         * to return one: the header gives it three return codes and an output
         * struct with no room for a field list, and changing the header to suit
         * this implementation is not allowed. The caller asks the same question
         * with a report and prints every missing field with
         * oa_config_field_why() beside it. firmware/test/test_oa_state.c
         * exercises both halves together, so the refusal and the explanation
         * cannot drift apart silently. */
        if (in->operator_armed && oa_config_is_flightworthy(cfg, features, NULL)) {
            ctx->state      = OA_STATE_ARMED;
            armed_this_step = true;
        }
        break;
    }

    case OA_STATE_ARMED: {
        /* Sustained upward acceleration, held for a minimum number of
         * consecutive samples so that a bump on the pad does not trigger it.
         *
         * pad_reference_locked gates the whole rule. A launch detected against
         * an unfinished reference produces a flight measured from the wrong
         * zero, and every altitude in it is wrong by the same unknown amount. */
        const bool accelerating = in->pad_reference_locked
                                  && OA_IS_SET(cfg->launch_accel_threshold_cg)
                                  && (in->accel_cg >= cfg->launch_accel_threshold_cg);

        ctx->launch_run = accelerating ? oa_run_inc(ctx->launch_run) : 0u;

        if (oa_confirmed(ctx->launch_run, cfg->launch_confirm_samples)) {
            ctx->state = OA_STATE_BOOST;
        }
        break;
    }

    case OA_STATE_BOOST: {
        /* Burnout. data/flight-phases.yaml states the criterion as
         * "acceleration falls through zero as thrust ends".
         *
         * Zero here is the sign change itself, not a threshold: no measurement
         * anyone makes would move it, so it is not a tunable and does not belong
         * in the configuration. The tunable nearby is the optional hysteresis
         * band, and whether burnout detection needs one at all is still an open
         * question. Unset means no band and detection is on the sign alone,
         * which is what that field's own description promises. A band of zero or
         * less is likewise no band. */
        int32_t burnout_level_cg = 0;
        if (OA_IS_SET(cfg->burnout_accel_hysteresis_cg) && (cfg->burnout_accel_hysteresis_cg > 0)) {
            burnout_level_cg = -cfg->burnout_accel_hysteresis_cg;
        }

        const bool thrust_ended = (in->accel_cg <= burnout_level_cg);

        ctx->burnout_run = thrust_ended ? oa_run_inc(ctx->burnout_run) : 0u;

        if (oa_confirmed(ctx->burnout_run, cfg->burnout_confirm_samples)) {
            ctx->state = OA_STATE_COAST;
        }
        break;
    }

    case OA_STATE_COAST: {
        /* Apogee. Altitude decreasing across several consecutive samples.
         *
         * "Decreasing" is measured against the running peak rather than against
         * the previous sample, for two reasons. The context this machine is
         * given carries the peak and does not carry the previous altitude, and
         * the header may not be changed to add one. And the quantity that
         * actually matters is how far the rocket has come back down from the
         * highest sample seen, which is what a confirmed drop from the peak
         * measures. A sample that sets a new peak is not a descending sample and
         * clears the run, which is the consecutive-evidence rule the criteria
         * ask for.
         *
         * baro_valid gates it, and that gate is the whole reason the input
         * exists. A barometer that has failed still returns numbers. A dead one
         * returning a constant slightly below the last peak reads as a descent
         * that never ends, and a detector that could not tell the difference
         * would call apogee on a dead sensor, on the pad, at any altitude. */
        const bool descending = in->baro_valid && ctx->peak_valid
                                && (in->alt_cm < ctx->peak_alt_cm);

        ctx->apogee_run = descending ? oa_run_inc(ctx->apogee_run) : 0u;

        if (oa_confirmed(ctx->apogee_run, cfg->apogee_confirm_samples)) {
            ctx->state = OA_STATE_APOGEE;

            /* Told to the scheduler exactly once. Apogee happens once, and
             * oa_sched_notify_apogee refuses a second call. */
            if (!ctx->apogee_reported) {
                ctx->apogee_reported = true;
                apogee_this_step     = true;
            }
        }
        break;
    }

    case OA_STATE_APOGEE: {
        /* Sustained descent confirmed.
         *
         * On the sign of the fused vertical velocity rather than on the
         * barometer, and not gated on baro_valid. A machine that could not leave
         * APOGEE with a failed barometer would stay there for the rest of the
         * flight, which would hold the log rate and the transmit schedule at
         * their apogee settings and would mean the recovery beacon never starts.
         * A rocket that has reached apogee is coming down whether or not the
         * instrument that noticed is still working.
         *
         * Zero is the sign of the velocity, not a threshold. */
        const bool descending = (in->vel_dm_s < 0);

        ctx->descent_run = descending ? oa_run_inc(ctx->descent_run) : 0u;

        if (oa_confirmed(ctx->descent_run, cfg->descent_confirm_samples)) {
            ctx->state = OA_STATE_DESCENT;
        }
        break;
    }

    case OA_STATE_DESCENT: {
        /* Landing. Altitude stable near the ground reference and acceleration
         * quiet, both held for a sustained period.
         *
         * alt_cm is height above the pad reference, so "near the reference" is
         * "inside the band around zero". The band has to be wider than the zero
         * can drift during a flight, which is a measurement of barometer drift
         * over a long pad wait and has not been made.
         *
         * This is a sustained period rather than a sample count because the
         * thing being excluded is a rocket swinging under a parachute close to
         * the ground, which is a duration, not a number of samples.
         *
         * TODO(verify): a rocket hanging in a tree is stationary and is not on
         * the ground, and this detector as written will call it landed.
         * Determine on real hardware whether anything distinguishes the two, and
         * if nothing does, say so, because a reader searching a treeline needs to
         * know which behaviour to expect. */
        const bool quiet = oa_within_band(in->alt_cm, cfg->landing_alt_band_cm)
                           && oa_within_band(in->accel_cg, cfg->landing_accel_band_cg);

        if (!quiet) {
            ctx->landing_pending = false;
        } else {
            if (!ctx->landing_pending) {
                ctx->landing_pending  = true;
                ctx->landing_since_ms = in->t_ms;
            }

            /* The subtraction cannot go negative: a sample whose timestamp went
             * backwards was rejected at the top of this function. */
            if (OA_IS_SET(cfg->landing_hold_ms) && (cfg->landing_hold_ms >= 0)
                && ((in->t_ms - ctx->landing_since_ms) >= (uint32_t)cfg->landing_hold_ms)) {
                ctx->state = OA_STATE_LANDED;
            }
        }
        break;
    }

    case OA_STATE_LANDED:
        /* Terminal until a power cycle. There is no transition out, no way to
         * command one, and no entry point in this module that sets a state. */
        break;

    case OA_STATE_COUNT:
        /* Not a state. It is an enumerator in oa_state_t, so it has to appear
         * here for the switch to be exhaustive.
         *
         * There is deliberately no default label. Every phase is named, so
         * adding one to data/flight-phases.yaml stops this switch compiling
         * rather than quietly falling through it. */
        break;
    }

    /* The timebase.
     *
     * t_ms is milliseconds since arming, so it restarts at the transition into
     * ARMED. The monotonic check is dropped for that one step rather than
     * rejecting the first armed sample for going backwards against whatever the
     * caller was passing before. */
    if (armed_this_step) {
        ctx->have_last = false;
        ctx->last_t_ms = 0u;
    } else {
        ctx->last_t_ms = in->t_ms;
        ctx->have_last = true;
    }

    out->state           = ctx->state;
    out->changed         = (ctx->state != before);
    out->apogee_detected = apogee_this_step;

    /* Reported every step so a caller does not have to latch them, and zero
     * before any peak exists. A caller must read apogee_detected, or the state,
     * before treating these as a measurement: zero is a legitimate altitude and
     * is not the same thing as no measurement yet. oa_state_peak is the accessor
     * that distinguishes the two. */
    out->apogee_cm   = ctx->peak_valid ? ctx->peak_alt_cm : 0;
    out->t_apogee_ms = ctx->peak_valid ? ctx->peak_t_ms : 0u;

    return OA_OK;
}

oa_state_t oa_state_current(const oa_state_ctx_t *ctx)
{
    /* No error channel in this signature, so a NULL context reports PAD_IDLE:
     * the state a payload that has done nothing is in, and the state that
     * advances nothing and transmits least. */
    return (ctx == NULL) ? OA_STATE_PAD_IDLE : ctx->state;
}

oa_result_t oa_state_peak(const oa_state_ctx_t *ctx, int32_t *out_alt_cm, uint32_t *out_t_ms)
{
    if (ctx == NULL) {
        return OA_ERR_NULL;
    }

    /* Not a peak of zero. Zero is a legitimate altitude for a flight that never
     * left the pad, and "no measurement yet" is a different fact that deserves a
     * different answer. */
    if (!ctx->peak_valid) {
        return OA_ERR_EMPTY;
    }

    /* Either output may be NULL: a caller that wants only the altitude should
     * not have to supply a variable for the time to be dropped into. */
    if (out_alt_cm != NULL) {
        *out_alt_cm = ctx->peak_alt_cm;
    }
    if (out_t_ms != NULL) {
        *out_t_ms = ctx->peak_t_ms;
    }

    return OA_OK;
}

/*
 * oApogee sensor fusion: the complementary filter declared in oa_fusion.h.
 *
 * THE FILTER, IN ONE PARAGRAPH
 *
 * Two states, altitude and vertical velocity. Every sample they are advanced by
 * the accelerometer, which is right about the short term and drifts without
 * limit, and then pulled toward the barometer, which is noisy and disturbed
 * around burnout but does not drift. The size of the pull is set by one time
 * constant, tau, which is configuration and is unset, because it comes from
 * measured barometer noise and measured accelerometer bias drift and nobody has
 * measured either.
 *
 * WHERE THE TWO CORRECTION GAINS COME FROM, SINCE THERE IS ONLY ONE TAU
 *
 * With altitude and velocity both corrected from the single altitude error, the
 * error dynamics are second order. Writing k1 for the gain on altitude and k2
 * for the gain on velocity, and e for the altitude error:
 *
 *     e'' + k1 e' + k2 e = 0
 *
 * so the natural frequency is sqrt(k2) and the damping ratio is k1 / (2
 * sqrt(k2)). Putting both poles at 1/tau, which is the definition of the
 * crossover this filter is built around, gives
 *
 *     k2 = 1 / tau^2      k1 = 2 / tau
 *
 * and a damping ratio of exactly 1. The factor of 2 is not a tuning constant: it
 * is the value that makes the response critically damped, which is the fastest
 * decay with no oscillation. Choosing k1 = 1 / tau instead would give a damping
 * ratio of 0.5, and the estimate would ring after every disturbance. Both gains
 * follow from tau alone, which is why the configuration has one field and not
 * two.
 *
 * CRITICALLY DAMPED IS NOT THE SAME AS NO OVERSHOOT, AND THIS FILTER OVERSHOOTS
 *
 * The only way this loop can move the altitude is through the velocity, so an
 * altitude that starts a distance A away from the barometer with no velocity
 * error does not approach it and stop. Solving the equation above with
 * e(0) = -A and e'(0) = -k1 e(0), which is what the structure forces, gives
 *
 *     e(t) = A (t/tau - 1) exp(-t/tau)
 *
 * so the error passes through zero at t = tau, overshoots to A / e^2, which is
 * 13.5% of the original gap, at t = 2 tau, and decays from there. It crosses
 * once and does not oscillate, which is what the damping ratio bought.
 *
 * That is a property of the filter, not of a rocket: it only appears when the
 * estimate and the barometer are far apart, which on a flight means seeding, a
 * barometer coming back after a fault, or a genuine pressure disturbance.
 * test_oa_fusion.c measures it at 13.3% on a 100 m step, which is the discrete
 * version of the same number. It is written down here because a reader looking
 * at a fused altitude that swung past a barometric one is entitled to know
 * whether that is the filter or the flight.
 *
 * ARITHMETIC
 *
 * Fixed point at OA_FUSION_FRAC_BITS throughout, no floating point, no division
 * wider than 32 bits, and no right shift of a negative value left to the
 * compiler. Every scaling constant below is an integer constant expression built
 * from the SI definition of standard gravity and from the unit scales the two
 * specs already use, so it folds at compile time and a reader can check it
 * without running anything.
 *
 * QUANTISATION FLOOR, WHICH IS A REAL LIMITATION OF THIS IMPLEMENTATION
 *
 * The velocity correction per sample is the altitude correction divided by
 * 2 tau, so with dt much smaller than tau it is a fraction of one count of the
 * Q8 velocity state. Once the estimate has nearly settled, every such fraction
 * rounds away, the velocity stops being corrected while it still holds a small
 * error, and the altitude settles wherever that phantom climb rate is balanced
 * by the altitude correction. Both residuals are bounded, and the bounds fall
 * out of the arithmetic rather than being fitted:
 *
 *     residual velocity   <= (tau_ms / dt_ms) counts, one count being
 *                            1/256 dm/s, which is 0.39 mm/s
 *     residual altitude   ~  residual velocity * tau / 2
 *
 * Measured on the host by test_oa_fusion.c against a perfectly steady barometer:
 * 1 cm and 1.9 cm/s at tau of 1 s and 100 Hz, 46 cm and 18 cm/s at tau of 5 s
 * and 200 Hz. It grows as roughly tau squared over dt, so it is negligible for a
 * sub-second crossover and is not negligible for a long one.
 *
 * This cannot be fixed inside the state this filter is given. Carrying the lost
 * fraction across samples needs somewhere to keep it, and oa_fusion_t has two
 * Q8 states and nowhere to put a residue. It is written down rather than left to
 * be discovered because a standing offset between the fused altitude and the raw
 * pressure column of a log is exactly the kind of thing that gets blamed on a
 * sensor.
 *
 * TODO(verify): once tau is measured, compute both residuals at the shipped
 * sample rate and decide whether the velocity state needs more fractional bits
 * than OA_FUSION_FRAC_BITS gives it, or whether oa_fusion_t needs a residue
 * field. Either is a change to oa_fusion.h, so it is a decision to take
 * deliberately and once.
 *
 * Nothing in this file has run on hardware, and no flight has ever been fused.
 */

#include "oapogee/oa_fusion.h"

/* ---------------------------------------------------------------------------
 * Scaling constants.
 *
 * These are unit conversions between the scales the packet spec and the log
 * format already define (0.01 g, 0.1 m/s, cm, ms), not tunables. The test for a
 * tunable is whether the number would change if someone measured something, and
 * none of these would: they would only change if a spec changed.
 * ------------------------------------------------------------------------ */

/* All the multiply-shift constants below use this many fractional bits. Wide
 * enough that the constants keep more precision than the Q8 state can show, and
 * narrow enough that every product stays inside int64. */
#define OA_FUSION_MUL_SHIFT (30)

/* (accel_cg * dt_ms) times this, shifted down by OA_FUSION_MUL_SHIFT, is the
 * velocity increment in decimetres per second at OA_FUSION_FRAC_BITS.
 *
 *     dv [dm/s] = accel_cg [0.01 g] * g [m/s^2] * dt_ms [ms]
 *                 / 100 (hundredths of g per g)
 *                 / 1000 (ms per s)
 *                 * 10 (dm per m)
 *
 * which is written below as one division by 100 * 1000 * 100000, with gravity in
 * micrometres per second squared and the 100000 folding in both the micro prefix
 * and the decimetre. Standard gravity is exact by definition in the SI, so this
 * whole expression is exact. */
#define OA_FUSION_DV_MUL                                                       \
    ((int64_t)(((int64_t)OA_STANDARD_GRAVITY_UM_S2                             \
                << (OA_FUSION_FRAC_BITS + OA_FUSION_MUL_SHIFT))                \
               / (100LL * 1000LL * 100000LL)))

/* (vel_dm_s_fx * dt_ms) times this, shifted down by OA_FUSION_MUL_SHIFT, is the
 * altitude increment in centimetres at the same fractional bits: one decimetre
 * per second held for one millisecond is one hundredth of a centimetre, from
 * 1000 ms per second divided by 10 centimetres per decimetre. */
#define OA_FUSION_DALT_MUL (((int64_t)1 << OA_FUSION_MUL_SHIFT) / (1000 / 10))

/* The velocity correction is the altitude correction divided by 2 tau, which
 * falls out of k2 = k1 / (2 tau). In the units used here that is
 *
 *     vel_correction [dm/s] = alt_correction [cm] * 1000 / (10 * 2 * tau_ms)
 *
 * so the numerator is 1000 ms per second over 10 centimetres per decimetre over
 * the factor of 2 from the critical damping above. It is divided by tau at run
 * time, which is a 32-bit division. */
#define OA_FUSION_VEL_CORR_SHIFT (24)
#define OA_FUSION_VEL_CORR_NUM   (1000 / (10 * 2))

/* ---------------------------------------------------------------------------
 * Arithmetic limits.
 *
 * These are the ranges inside which every product below provably fits in an
 * int64, expressed in terms of the widths the two specs give these quantities.
 * They are bounds on the arithmetic, not thresholds: nothing here decides
 * anything about a flight.
 * ------------------------------------------------------------------------ */

/* An int32 of centimetres at OA_FUSION_FRAC_BITS reaches 83.9 km, which is the
 * "tens of kilometres" the fractional-bit choice in the header is aiming at. */
#define OA_FUSION_ALT_CM_LIMIT (INT32_MAX >> OA_FUSION_FRAC_BITS)
#define OA_FUSION_ALT_FX_LIMIT ((int32_t)OA_FUSION_ALT_CM_LIMIT << OA_FUSION_FRAC_BITS)

/* Vertical velocity is an i16 of decimetres per second in the packet and in the
 * log, so plus or minus 3276.7 m/s is everything either format can carry. A
 * filter state outside that is not a state worth propagating. */
#define OA_FUSION_VEL_DM_S_LIMIT ((int32_t)INT16_MAX)
#define OA_FUSION_VEL_FX_LIMIT   (OA_FUSION_VEL_DM_S_LIMIT << OA_FUSION_FRAC_BITS)

/* Vertical acceleration is an i16 of hundredths of g in the packet, so a value
 * outside this cannot have come from a conforming source. */
#define OA_FUSION_ACCEL_CG_LIMIT ((int32_t)INT16_MAX)

/* The longest interval the altitude integration can take without overflowing,
 * given the velocity limit above. It works out at about 102 seconds. A sample
 * interval that long is not a sample interval, and the clamp is here so that the
 * arithmetic cannot overflow for any input at all, rather than for any input a
 * caller is expected to pass. */
#define OA_FUSION_DT_MS_LIMIT                                                  \
    ((uint32_t)(INT64_MAX / ((int64_t)OA_FUSION_VEL_FX_LIMIT * OA_FUSION_DALT_MUL)))

_Static_assert(OA_FUSION_DV_MUL > 0 && OA_FUSION_DALT_MUL > 0,
               "a scaling constant folded to zero, which means a unit is wrong");
_Static_assert(OA_FUSION_DT_MS_LIMIT > 1000u,
               "the integration limit fell below one second, which no sample rate can use");

/* ---------------------------------------------------------------------------
 * Small fixed point helpers. No state, no allocation.
 * ------------------------------------------------------------------------ */

/* Shift right, rounding to nearest, ties away from zero, on both signs.
 *
 * Two decisions in one function, and both of them are load bearing.
 *
 * The sign is handled explicitly because C11 leaves >> of a negative value
 * implementation-defined. Every compiler this can be built with makes it an
 * arithmetic shift, but the claim this filter rests on is that the host test and
 * the target produce identical bits, and a claim that rests on
 * implementation-defined behaviour is weaker than it looks.
 *
 * It rounds rather than truncates because truncation widens the dead zone
 * described under QUANTISATION FLOOR above. Truncating, the filter settled 3 cm
 * above a stationary barometer at tau of 1 s and 100 Hz; rounding, 1 cm. Both
 * measured on the host, neither reasoned about. */
static int64_t oa_shr_round(int64_t value, unsigned shift)
{
    int64_t half      = (int64_t)1 << (shift - 1u);
    int64_t magnitude = (value < 0) ? -value : value;
    int64_t rounded   = (magnitude + half) >> shift;

    return (value < 0) ? -rounded : rounded;
}

static int32_t oa_clamp32(int64_t value, int32_t limit)
{
    if (value > (int64_t)limit) {
        return limit;
    }
    if (value < -(int64_t)limit) {
        return -limit;
    }
    return (int32_t)value;
}

/* num / den in Q16, saturating at 1.0.
 *
 * 32-bit division only. A 64-bit division in the sample path becomes a compiler
 * runtime helper on a target with no 64-bit divide instruction, which is one of
 * the things core/allowed-undefined.txt exists to catch. When num is too large
 * to shift into Q16, both sides are scaled down together: num keeps at least 15
 * significant bits, which is far more than the Q16 result can show. */
static uint32_t oa_ratio_q16(uint32_t num, uint32_t den)
{
    if (den == 0u || num >= den) {
        return 1u << 16;
    }
    while (num > 0xFFFFu) {
        num >>= 1;
        den >>= 1;
    }
    return (num << 16) / den;
}

/* A tau of zero or less is not a time constant. There is no defensible
 * behaviour to fall back on, and dividing by it would produce an estimate that
 * is simply the barometer while looking like a fused one, so it is reported the
 * same way a missing one is. */
static bool oa_tau_usable(oa_tunable_t tau_ms)
{
    return OA_IS_SET(tau_ms) && (tau_ms > 0);
}

/* Q8 to whole units, rounded to nearest and symmetric about zero so that a
 * descent is not rounded differently from a climb. */
static int32_t oa_fx_to_whole(int32_t value_fx)
{
    int32_t half      = 1 << (OA_FUSION_FRAC_BITS - 1);
    int32_t magnitude = (value_fx < 0) ? -value_fx : value_fx;
    int32_t rounded   = (magnitude + half) >> OA_FUSION_FRAC_BITS;

    return (value_fx < 0) ? -rounded : rounded;
}

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

oa_result_t oa_fusion_init(oa_fusion_t *f)
{
    if (f == NULL) {
        return OA_ERR_NULL;
    }

    f->alt_cm_fx     = 0;
    f->vel_dm_s_fx   = 0;
    f->seeded        = false;
    f->unanchored_ms = 0u;

    return OA_OK;
}

oa_result_t oa_fusion_seed(oa_fusion_t *f, int32_t alt_cm, int32_t vel_dm_s)
{
    if (f == NULL) {
        return OA_ERR_NULL;
    }

    f->alt_cm_fx     = oa_clamp32((int64_t)alt_cm << OA_FUSION_FRAC_BITS, OA_FUSION_ALT_FX_LIMIT);
    f->vel_dm_s_fx   = oa_clamp32((int64_t)vel_dm_s << OA_FUSION_FRAC_BITS, OA_FUSION_VEL_FX_LIMIT);
    f->seeded        = true;
    f->unanchored_ms = 0u;

    return OA_OK;
}

oa_result_t oa_fusion_update(oa_fusion_t *f, const oa_config_t *cfg, const oa_fusion_input_t *in)
{
    oa_tunable_t tau_cfg;
    uint32_t     tau_ms;
    uint32_t     dt_ms;
    int32_t      accel_cg;
    int32_t      vel_before_fx;
    int64_t      dv_fx;
    int64_t      mean_vel_fx;
    int64_t      dalt_fx;

    if (f == NULL || cfg == NULL || in == NULL) {
        return OA_ERR_NULL;
    }

    /* Both constants are required, not only the one this sample would use. A
     * payload that fuses correctly on the pad and then meets an unset boost
     * constant at the worst possible moment is worse than one that refuses now,
     * and the configuration validator refuses to arm without either. */
    if (!oa_tau_usable(cfg->fusion_tau_ms) || !oa_tau_usable(cfg->fusion_tau_boost_ms)) {
        return OA_ERR_UNSET;
    }

    /* Before seeding there is no altitude to advance. Zero is a legitimate
     * altitude on the pad, so starting from it would be indistinguishable from
     * an estimate. */
    if (!f->seeded) {
        return OA_ERR_STATE;
    }

    tau_cfg = in->in_boost ? cfg->fusion_tau_boost_ms : cfg->fusion_tau_ms;
    tau_ms  = (uint32_t)tau_cfg;

    dt_ms = (in->dt_ms > OA_FUSION_DT_MS_LIMIT) ? OA_FUSION_DT_MS_LIMIT : in->dt_ms;

    accel_cg = in->accel_cg;
    if (accel_cg > OA_FUSION_ACCEL_CG_LIMIT) {
        accel_cg = OA_FUSION_ACCEL_CG_LIMIT;
    }
    if (accel_cg < -OA_FUSION_ACCEL_CG_LIMIT) {
        accel_cg = -OA_FUSION_ACCEL_CG_LIMIT;
    }

    /* --- Predict from the accelerometer ----------------------------------
     *
     * Velocity first, then altitude over the mean of the velocity before and
     * after, which is the trapezoid rule and is exactly the missing
     * 0.5 a dt^2 term. At 10 g and a 10 ms sample that term is half a
     * centimetre, which is small once and is not small accumulated over a whole
     * boost, and it has the same sign every time. */
    vel_before_fx = f->vel_dm_s_fx;

    dv_fx = oa_shr_round((int64_t)accel_cg * (int64_t)dt_ms * OA_FUSION_DV_MUL,
                         OA_FUSION_MUL_SHIFT);
    f->vel_dm_s_fx = oa_clamp32((int64_t)vel_before_fx + dv_fx, OA_FUSION_VEL_FX_LIMIT);

    mean_vel_fx = oa_shr_round((int64_t)vel_before_fx + (int64_t)f->vel_dm_s_fx, 1u);
    dalt_fx     = oa_shr_round(mean_vel_fx * (int64_t)dt_ms * OA_FUSION_DALT_MUL,
                               OA_FUSION_MUL_SHIFT);
    f->alt_cm_fx = oa_clamp32((int64_t)f->alt_cm_fx + dalt_fx, OA_FUSION_ALT_FX_LIMIT);

    /* --- Correct toward the barometer, or do not -------------------------
     *
     * WHAT THIS FILTER DOES WHEN THE BAROMETER IS FAULTED
     *
     * Nothing. With baro_valid false the block below is skipped entirely: the
     * estimate is the integral of acceleration and nothing else, it accumulates
     * accelerometer bias without limit, and unanchored_ms counts how long that
     * has been going on so a caller can decide when to stop trusting it and a
     * log can show it afterwards. There is no reduced-confidence blend and no
     * decay toward anything, because the alternative to an honest drifting
     * number is a number corrected toward a barometer that is known to be
     * wrong, and that one looks fine on a plot. Deciding what to do about it is
     * the caller's, which is why baro_valid is an input and not something this
     * filter infers. */
    if (in->baro_valid) {
        uint32_t ratio_q16     = oa_ratio_q16(dt_ms, tau_ms);
        uint32_t alt_gain_q16  = ratio_q16 * 2u;
        int32_t  vel_gain_q24;
        int64_t  baro_fx;
        int64_t  err_fx;
        int64_t  alt_corr_fx;
        int64_t  vel_corr_fx;

        /* dt has caught up with tau. The correction cannot exceed the error
         * without oscillating, so it saturates at taking the barometer whole. */
        if (alt_gain_q16 > (1u << 16)) {
            alt_gain_q16 = 1u << 16;
        }

        vel_gain_q24 = ((int32_t)OA_FUSION_VEL_CORR_NUM << OA_FUSION_VEL_CORR_SHIFT) / tau_cfg;

        baro_fx = (int64_t)oa_clamp32((int64_t)in->baro_alt_cm, OA_FUSION_ALT_CM_LIMIT)
                  << OA_FUSION_FRAC_BITS;
        err_fx  = baro_fx - (int64_t)f->alt_cm_fx;

        alt_corr_fx = oa_shr_round(err_fx * (int64_t)alt_gain_q16, 16u);
        vel_corr_fx = oa_shr_round(alt_corr_fx * (int64_t)vel_gain_q24,
                                         OA_FUSION_VEL_CORR_SHIFT);

        f->alt_cm_fx = oa_clamp32((int64_t)f->alt_cm_fx + alt_corr_fx, OA_FUSION_ALT_FX_LIMIT);
        f->vel_dm_s_fx =
            oa_clamp32((int64_t)f->vel_dm_s_fx + vel_corr_fx, OA_FUSION_VEL_FX_LIMIT);

        f->unanchored_ms = 0u;
    } else {
        /* Saturates rather than wrapping. Wrapping would show a long dead
         * reckoning run as a short one, which is the wrong way round for a
         * number whose whole job is to say how far the estimate has drifted. */
        if (dt_ms > (UINT32_MAX - f->unanchored_ms)) {
            f->unanchored_ms = UINT32_MAX;
        } else {
            f->unanchored_ms += dt_ms;
        }
    }

    return OA_OK;
}

oa_result_t oa_fusion_get(const oa_fusion_t *f, int32_t *out_alt_cm, int32_t *out_vel_dm_s)
{
    if (f == NULL) {
        return OA_ERR_NULL;
    }
    if (!f->seeded) {
        return OA_ERR_EMPTY;
    }

    if (out_alt_cm != NULL) {
        *out_alt_cm = oa_fx_to_whole(f->alt_cm_fx);
    }
    if (out_vel_dm_s != NULL) {
        *out_vel_dm_s = oa_fx_to_whole(f->vel_dm_s_fx);
    }

    return OA_OK;
}

uint32_t oa_fusion_unanchored_ms(const oa_fusion_t *f)
{
    return (f == NULL) ? 0u : f->unanchored_ms;
}

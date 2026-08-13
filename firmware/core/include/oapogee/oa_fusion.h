/*
 * oApogee sensor fusion: a complementary filter over barometric altitude and
 * integrated vertical acceleration.
 *
 * Each sensor covers the other's failure. The barometer is noisy and actively
 * wrong during the pressure disturbance around burnout and, on faster flights,
 * through transonic effects, but it does not drift. The accelerometer does not
 * care about air pressure at all, but integrating any sensor's bias accumulates
 * it, so an accelerometer-only altitude drifts steadily and without limit. Fused,
 * the barometer anchors the long term and the accelerometer carries the short
 * term.
 *
 * Decided: a complementary filter for v1, revisited when there is flight data.
 * It is simpler to implement, to explain, and to check by hand, on a project
 * whose product is its documentation. A Kalman filter is better behaved when
 * the noise characteristics are known, and tuning one now would mean inventing
 * covariances for a barometer and an IMU that have never flown. The reasoning
 * is in docs/open-questions.md. If it is revisited, this interface changes.
 *
 * NO FLOATING POINT, DELIBERATELY
 *
 * This filter runs in the sample path, which is interrupt-adjacent. Fixed point
 * is used throughout for three reasons: an interrupt context that touches an FPU
 * has FPU registers to save, whether the core has an FPU is a property of a
 * microcontroller nobody has run this on, and integer arithmetic gives a result
 * that is bit-for-bit identical on the host tests and on the target. The last
 * one matters most: a test that agrees with the flight code to within a
 * tolerance is a test that will one day disagree in flight for a reason nobody
 * can reproduce.
 *
 * Nothing in this file has run on hardware.
 */

#ifndef OAPOGEE_OA_FUSION_H
#define OAPOGEE_OA_FUSION_H

#include "oa_states.h"
#include "oapogee/oa_config.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fractional bits in the internal state. Not a tunable: it is the shape of the
 * arithmetic, chosen so that centimetre-scale altitudes keep sub-millimetre
 * internal resolution in an int32 while leaving room for the tens of kilometres
 * an int32 of centimetres can hold. */
#define OA_FUSION_FRAC_BITS (8)

/* Standard gravity, exactly 9.80665 m/s^2 by definition in the SI. This is a
 * defined constant, not a measurement and not a tunable, so it belongs in code.
 * Stored in micrometres per second squared so the implementation can convert
 * hundredths of g into an integer acceleration without a rounding choice that
 * differs between the host and the target. */
#define OA_STANDARD_GRAVITY_UM_S2 (9806650)

/* ---------------------------------------------------------------------------
 * Inputs.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* Interval since the previous update, milliseconds. The filter is driven by
     * the measured interval rather than an assumed rate, because the sample rate
     * varies with flight phase and an assumed rate would integrate acceleration
     * over the wrong time. */
    uint32_t dt_ms;

    /* Barometric altitude above the pad reference, centimetres, from
     * oa_baro_altitude_cm. */
    int32_t baro_alt_cm;

    /* THE EXPLICIT BARO-VALID INPUT.
     *
     * False when the barometer has failed, has left its plausible band, or is
     * being disturbed badly enough not to be trusted. When this is false the
     * filter integrates acceleration alone and does not correct toward the
     * barometer, and it accumulates drift while it does so, which is honest and
     * bounded rather than being corrected toward a wrong number.
     *
     * This is an input rather than something the filter infers, because the
     * filter cannot tell a barometer that is reading badly from a rocket that is
     * moving unusually. A failed barometer still returns numbers, and a filter
     * that silently blends them produces an altitude that looks fine and is not.
     * The caller knows about BARO_FAULT and the plausibility band; the filter
     * does not. */
    bool baro_valid;

    /* Vertical acceleration, hundredths of g, gravity already removed by the
     * caller, positive up. Gravity removal needs the orientation estimate, which
     * is the caller's business, and doing it here would make this filter depend
     * on the IMU's attitude solution. */
    int32_t accel_cg;

    /* Whether the vehicle is in BOOST. Selects fusion_tau_boost_ms instead of
     * fusion_tau_ms, because airflow over the static ports during boost disturbs
     * the pressure the sensor sees, and the estimate has to lean on integrated
     * acceleration through it. Passed as a flag rather than the whole state so
     * that the filter has no opinion about the state machine. */
    bool in_boost;
} oa_fusion_input_t;

/* ---------------------------------------------------------------------------
 * Filter state.
 *
 * Caller-owned. No allocation, no globals.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* Altitude in centimetres and velocity in decimetres per second, both
     * shifted left by OA_FUSION_FRAC_BITS. */
    int32_t alt_cm_fx;
    int32_t vel_dm_s_fx;

    /* True once the filter has been seeded from a barometer reading. Before
     * that it has no altitude at all and reports OA_ERR_EMPTY rather than
     * zero, because zero is a legitimate altitude on the pad and "no estimate
     * yet" is not the same thing. */
    bool seeded;

    /* Consecutive updates taken with baro_valid false. Exposed through
     * oa_fusion_dead_reckoned_ms so the caller can decide when an
     * accelerometer-only altitude has drifted far enough to stop trusting, and
     * so a log can show how long the estimate was unanchored. */
    uint32_t unanchored_ms;
} oa_fusion_t;

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

/* Clear to unseeded. */
oa_result_t oa_fusion_init(oa_fusion_t *f);

/* Seed the filter from a known altitude and velocity, which on the pad is the
 * reference altitude and zero. Called once, when the pad reference locks. */
oa_result_t oa_fusion_seed(oa_fusion_t *f, int32_t alt_cm, int32_t vel_dm_s);

/* Advance one sample.
 *
 * Returns OA_OK, OA_ERR_NULL, OA_ERR_UNSET when fusion_tau_ms or
 * fusion_tau_boost_ms is not set in the configuration, or OA_ERR_STATE if the
 * filter has not been seeded.
 *
 * OA_ERR_UNSET is not a formality. The crossover time constant is the whole
 * behaviour of this filter, and there is no defensible value to fall back on:
 * it is set from measured barometer noise and measured accelerometer bias drift,
 * and neither has been measured. A filter that ran with a guessed constant would
 * produce an altitude that looks like a measurement. */
oa_result_t oa_fusion_update(oa_fusion_t *f, const oa_config_t *cfg, const oa_fusion_input_t *in);

/* Current estimate in the units the packet and the log use. Returns OA_ERR_EMPTY
 * before the filter has been seeded. Either out pointer may be NULL. */
oa_result_t oa_fusion_get(const oa_fusion_t *f, int32_t *out_alt_cm, int32_t *out_vel_dm_s);

/* How long the estimate has been running on integrated acceleration alone,
 * milliseconds. Zero when the last update was anchored to the barometer. */
uint32_t oa_fusion_unanchored_ms(const oa_fusion_t *f);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_FUSION_H */

/*
 * oApogee barometric altitude, and the pad reference accumulator.
 *
 * Two things, kept together because they are two halves of one measurement: the
 * reference is the zero, and the altitude is the distance from it. Publishing
 * the reference with its sample count is what makes a flight re-derivable when
 * the reference turns out to have been taken badly, which is why both end up in
 * meta.json.
 *
 * WHY THE REFERENCE IS TAKEN ON ARMING
 *
 * A barometric altimeter measures pressure, and pressure varies with weather as
 * well as with height. Without a local reference taken now, altitude would be
 * height above sea level under a standard atmosphere, which is nobody's desired
 * number and is wrong by tens of metres on any day with weather. Taking the
 * reference when the operator arms makes the number mean height above where the
 * rocket is standing.
 *
 * The cost is real and is documented rather than hidden: the reference is taken
 * once, so a front moving through during a long pad wait shifts the actual
 * pressure and the altitude drifts with it. A flight that sat on the pad for
 * twenty minutes may land reporting a small negative altitude, and that is not a
 * fault.
 *
 * NO FLOATING POINT IN THE SAMPLE PATH. The altitude conversion runs at the
 * sample rate in an interrupt-adjacent context, so it is integer arithmetic for
 * the same three reasons given in oa_fusion.h, the strongest of which is that
 * the host tests and the target must agree bit for bit.
 *
 * Nothing in this file has run on hardware, and no reference has ever been
 * taken.
 */

#ifndef OAPOGEE_OA_BARO_H
#define OAPOGEE_OA_BARO_H

#include "oapogee/oa_config.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Pressure to altitude.
 * ------------------------------------------------------------------------ */

/* Altitude of `pressure_pa` above the level where the pressure is
 * `reference_pa`, in centimetres. Positive is up. Negative results are
 * legitimate and must not be clamped: a rocket can land below the pad, and
 * pressure can rise during a flight.
 *
 * The conversion is the hypsometric relation under the International Standard
 * Atmosphere, whose constants (sea level standard pressure and temperature, the
 * tropospheric lapse rate, the specific gas constant, standard gravity) are a
 * published international standard, not measurements this project has to make.
 * They are therefore constants in the implementation, not tunables, and the
 * implementation must name the standard it took them from.
 *
 * TODO(verify): state the altitude error the integer implementation introduces
 * against a double-precision reference across the full 50000 to 115535 Pa band,
 * measured by the conformance test, so that a reader knows the size of the
 * approximation. An integer implementation whose error is not stated is a number
 * nobody can check.
 *
 * Returns 0 if reference_pa is not positive, which is a programming error rather
 * than a reading: the caller checks oa_baro_ref_locked first. */
int32_t oa_baro_altitude_cm(int32_t pressure_pa, int32_t reference_pa);

/* ---------------------------------------------------------------------------
 * The pad reference accumulator.
 *
 * Started when the operator arms, fed every barometer sample, and locked when
 * the settling window completes. Nothing leaves ARMED before it locks, because a
 * launch detected against an unfinished reference produces a whole flight
 * measured from the wrong zero.
 *
 * Both the window duration and the minimum sample count come from oa_config_t
 * and are unset. There is no fallback: the window is set from measured barometer
 * noise on a settled pad, and nobody has measured it.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* Sum of the samples so far. 64 bit because a long window at a high sample
     * rate at around 100000 Pa overflows a 32 bit sum in well under a minute,
     * and the failure would be a reference that is silently wrong rather than a
     * reference that is obviously missing. */
    int64_t  sum_pa;
    uint32_t samples;

    uint32_t started_ms;
    bool     started;

    /* Once locked, reference_pa is frozen and further samples are ignored. It is
     * frozen rather than continuously averaged because every altitude in the
     * flight is measured against it, and a zero that moves during the flight
     * moves the whole altitude column with it. */
    int32_t reference_pa;
    bool    locked;

    /* Samples rejected for being outside the plausible band, which is what
     * stops a dropout or a garbage reading from poisoning the average. Recorded
     * so the operator can be told the reference was taken from a struggling
     * sensor. */
    uint32_t rejected;
} oa_baro_ref_t;

/* Begin a settling window at now_ms. Discards anything already accumulated. */
oa_result_t oa_baro_ref_start(oa_baro_ref_t *ref, uint32_t now_ms);

/* Feed one barometer sample.
 *
 * A reading outside [baro_plausible_min_pa, baro_plausible_max_pa] is counted in
 * `rejected` and not accumulated. Once the window has run for
 * pad_reference_window_ms and at least pad_reference_min_samples have been
 * accepted, the reference locks and later samples are ignored.
 *
 * Returns OA_OK, OA_ERR_NULL, OA_ERR_UNSET when any of those three configuration
 * fields is missing, or OA_ERR_STATE if the window was never started. */
oa_result_t oa_baro_ref_add(oa_baro_ref_t *ref, const oa_config_t *cfg, int32_t pressure_pa, uint32_t now_ms);

bool     oa_baro_ref_locked(const oa_baro_ref_t *ref);
uint32_t oa_baro_ref_samples(const oa_baro_ref_t *ref);
uint32_t oa_baro_ref_rejected(const oa_baro_ref_t *ref);

/* The locked reference pressure, for meta.json calibration.pad_pressure_pa and
 * for the STATUS packet. Returns OA_ERR_STATE before it locks, rather than the
 * running average, because a partial average presented as a reference is exactly
 * the mistake this accumulator exists to prevent. */
oa_result_t oa_baro_ref_pressure_pa(const oa_baro_ref_t *ref, int32_t *out_pressure_pa);

/* Whether a single reading is inside the configured plausible band. The caller
 * uses this to raise OA_FLAG_BARO_FAULT and to drive the baro_valid input to
 * fusion and to the state machine. Returns false and sets *out_unset when the
 * band is not configured, so a missing band cannot be mistaken for a healthy
 * sensor. */
bool oa_baro_is_plausible(const oa_config_t *cfg, int32_t pressure_pa, bool *out_unset);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_BARO_H */

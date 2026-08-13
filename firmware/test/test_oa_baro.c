/*
 * oApogee: conformance tests for oa_baro.c.
 *
 * Plain C and assert, no framework. Every test says which specification claim it
 * is checking, because a test whose failure does not tell you what promise broke
 * is a test nobody acts on.
 *
 * This file runs on a laptop, so it uses double precision and libm deliberately:
 * the point of the error test below is to compare the firmware's integer
 * arithmetic against the real formula, and the real formula needs pow(). None of
 * that crosses into core, which has neither.
 *
 * These tests check arithmetic and contracts. They check nothing about a
 * barometer, because no barometer has been read: no hardware exists, and no
 * pressure in this file came from a sensor.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_baro.h"

/* The ISA troposphere relation, in double precision, from the same published
 * constants oa_baro.c compiles in. This is the reference the integer
 * implementation is measured against. */
static double reference_altitude_cm(double pressure_pa, double ref_pa)
{
    const double t0_k        = 288.15;
    const double lapse_k_m   = 0.0065;
    const double gas_j_kg_k  = 287.052874;
    const double gravity     = 9.80665;
    const double exponent    = lapse_k_m * gas_j_kg_k / gravity;

    return (t0_k / lapse_k_m) * (1.0 - pow(pressure_pa / ref_pa, exponent)) * 100.0;
}

/* The four configuration fields oa_baro.c reads, set directly rather than
 * through oa_config_set, so this file links against oa_baro.c alone and a
 * failure here cannot be somebody else's module. Every other field is zeroed and
 * unused; oa_config.c has its own tests for the set bits. */
static void config_with_band(oa_config_t *cfg, int32_t min_pa, int32_t max_pa,
                             int32_t window_ms, int32_t min_samples)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->baro_plausible_min_pa     = min_pa;
    cfg->baro_plausible_max_pa     = max_pa;
    cfg->pad_reference_window_ms   = window_ms;
    cfg->pad_reference_min_samples = min_samples;
}

/* SPEC: oa_baro.h. Altitude is measured from the level where the pressure is
 * reference_pa, so a reading equal to the reference is exactly zero. This is
 * also what makes calibration.pad_pressure_pa in the log format the zero the
 * whole altitude column is measured against. */
static void test_zero_at_reference(void)
{
    assert(oa_baro_altitude_cm(101325, 101325) == 0);
    assert(oa_baro_altitude_cm(50000, 50000) == 0);
    assert(oa_baro_altitude_cm(115535, 115535) == 0);
}

/* SPEC: telemetry-packet.md, alt_cm, "negative values are legitimate rather than
 * a bug ... a receiver that clamps this at zero is hiding a real measurement",
 * and oa_baro.h, "negative results are legitimate and must not be clamped".
 * Pressure above the reference means below the reference level. */
static void test_negative_altitudes_are_not_clamped(void)
{
    assert(oa_baro_altitude_cm(102325, 101325) < 0);
    assert(oa_baro_altitude_cm(115535, 50000) < 0);

    /* And the sign the other way: less pressure is more altitude. */
    assert(oa_baro_altitude_cm(100325, 101325) > 0);
}

/* SPEC: oa_baro.h. Altitude decreases monotonically with increasing pressure at
 * a fixed reference. A conversion that is not monotonic would put a false apogee
 * anywhere its slope changed sign, and apogee detection is a comparison of
 * consecutive altitudes. */
static void test_monotonic_in_pressure(void)
{
    int32_t previous = oa_baro_altitude_cm(50000, 101325);
    int32_t pa;

    for (pa = 50001; pa <= 115535; pa++) {
        int32_t current = oa_baro_altitude_cm(pa, 101325);
        assert(current <= previous);
        previous = current;
    }
}

/* SPEC: oa_baro.h. "Returns 0 if reference_pa is not positive, which is a
 * programming error rather than a reading." */
static void test_non_positive_reference_returns_zero(void)
{
    assert(oa_baro_altitude_cm(101325, 0) == 0);
    assert(oa_baro_altitude_cm(101325, -1) == 0);
}

/* SPEC: oa_baro.h, the open TODO(verify): "state the altitude error the integer
 * implementation introduces against a double-precision reference across the full
 * 50000 to 115535 Pa band". This test is that measurement. The band is the one
 * telemetry-packet.md gives pad_pressure_pa_off, so it is every reference the
 * format can carry, and the pressure argument is swept over the same band.
 *
 * The bound asserted here is one centimetre. The figure this test printed when
 * it was written is in the comment above oa_baro_altitude_cm; most of it is the
 * unavoidable rounding of an exact quantity to a whole centimetre. */
static void test_integer_error_against_double_reference(void)
{
    double  worst_cm    = 0.0;
    int32_t worst_p     = 0;
    int32_t worst_ref   = 0;
    int32_t ref_pa;

    for (ref_pa = 50000; ref_pa <= 115535; ref_pa += 499) {
        int32_t pa;
        for (pa = 50000; pa <= 115535; pa += 53) {
            double expected = reference_altitude_cm((double)pa, (double)ref_pa);
            double actual   = (double)oa_baro_altitude_cm(pa, ref_pa);
            double error    = fabs(actual - expected);

            if (error > worst_cm) {
                worst_cm  = error;
                worst_p   = pa;
                worst_ref = ref_pa;
            }
        }
    }

    printf("  baro: worst integer error %.3f cm at pressure %d Pa, reference %d Pa\n",
           worst_cm, worst_p, worst_ref);
    assert(worst_cm < 1.0);
}

/* SPEC: oa_baro.h. The conversion is pure integer arithmetic, so it is
 * repeatable exactly. The fixed point argument in oa_fusion.h is that the host
 * test and the target agree bit for bit; a conversion that varied between calls
 * on one machine could not make that claim on two. */
static void test_conversion_is_repeatable(void)
{
    int32_t first = oa_baro_altitude_cm(89874, 101325);
    int     i;

    for (i = 0; i < 1000; i++) {
        assert(oa_baro_altitude_cm(89874, 101325) == first);
    }
}

/* SPEC: oa_baro.h. Feeding the accumulator before starting a window is a state
 * error, not something that quietly accumulates into a reference nobody asked
 * for. */
static void test_add_before_start_is_a_state_error(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;

    memset(&ref, 0, sizeof(ref));
    config_with_band(&cfg, 50000, 115535, 1000, 10);

    assert(oa_baro_ref_add(&ref, &cfg, 101325, 0u) == OA_ERR_STATE);
    assert(oa_baro_ref_locked(&ref) == false);
}

/* SPEC: oa_baro.h, "both the window duration and the minimum sample count come
 * from oa_config_t and are unset. There is no fallback." A payload with no
 * measured settling window must not establish a reference at all. */
static void test_unset_configuration_refuses(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;

    config_with_band(&cfg, 50000, 115535, 1000, 10);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);

    cfg.pad_reference_window_ms = OA_UNSET;
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 1u) == OA_ERR_UNSET);
    cfg.pad_reference_window_ms = 1000;

    cfg.pad_reference_min_samples = OA_UNSET;
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 1u) == OA_ERR_UNSET);
    cfg.pad_reference_min_samples = 10;

    cfg.baro_plausible_min_pa = OA_UNSET;
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 1u) == OA_ERR_UNSET);

    /* Nothing was accumulated by any of the refusals. */
    assert(oa_baro_ref_samples(&ref) == 0u);
    assert(oa_baro_ref_locked(&ref) == false);
}

/* SPEC: oa_baro.h. A minimum sample count of zero would mean a reference
 * averaged from no samples, which is the mistake the accumulator exists to
 * prevent, so it is reported as missing rather than acted on. */
static void test_zero_minimum_samples_is_treated_as_unset(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;

    config_with_band(&cfg, 50000, 115535, 0, 0);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 0u) == OA_ERR_UNSET);
    assert(oa_baro_ref_locked(&ref) == false);
}

/* SPEC: oa_baro.h. The reference locks only once the window has run AND at least
 * the minimum number of samples has been accepted. Either one alone is not
 * enough: a short window is a reference taken from too few readings, and a long
 * window during a sensor dropout is a reference taken from nothing. */
static void test_lock_needs_both_window_and_samples(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    uint32_t      t;

    config_with_band(&cfg, 50000, 115535, 1000, 100);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);

    /* Plenty of samples, window not finished. */
    for (t = 0u; t < 500u; t++) {
        assert(oa_baro_ref_add(&ref, &cfg, 101325, t) == OA_OK);
    }
    assert(oa_baro_ref_samples(&ref) == 500u);
    assert(oa_baro_ref_locked(&ref) == false);

    /* SPEC: oa_baro.h. The running average is not reachable before the lock. */
    {
        int32_t out = 12345;
        assert(oa_baro_ref_pressure_pa(&ref, &out) == OA_ERR_STATE);
        assert(out == 12345);
    }

    /* Window finishes, so the next accepted sample locks it. */
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 1000u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == true);
}

static void test_lock_needs_the_sample_count(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;

    config_with_band(&cfg, 50000, 115535, 10, 100);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);

    /* Window long finished, only three samples accepted. */
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 5000u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 6000u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 101325, 7000u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == false);
}

/* SPEC: log-format.md, calibration.pad_pressure_pa is "the reference actually
 * used" and pad_pressure_samples is "how many samples went into that average".
 * The locked value is the mean of the accepted samples and the count is what
 * went into it. */
static void test_reference_is_the_mean_of_accepted_samples(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    int32_t       locked = 0;
    uint32_t      i;

    config_with_band(&cfg, 50000, 115535, 0, 4);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);

    /* 100000, 100002, 100004, 100006: mean exactly 100003. */
    for (i = 0u; i < 4u; i++) {
        assert(oa_baro_ref_add(&ref, &cfg, 100000 + (int32_t)(i * 2u), i) == OA_OK);
    }

    assert(oa_baro_ref_locked(&ref) == true);
    assert(oa_baro_ref_samples(&ref) == 4u);
    assert(oa_baro_ref_pressure_pa(&ref, &locked) == OA_OK);
    assert(locked == 100003);
}

/* SPEC: oa_baro.h. A reading outside the plausible band is counted in `rejected`
 * and not accumulated, "which is what stops a dropout or a garbage reading from
 * poisoning the average". */
static void test_implausible_samples_are_rejected_not_averaged(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    int32_t       locked = 0;

    config_with_band(&cfg, 90000, 110000, 0, 2);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);

    assert(oa_baro_ref_add(&ref, &cfg, 100000, 0u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 0, 1u) == OA_OK);        /* dropout */
    assert(oa_baro_ref_add(&ref, &cfg, 500000, 2u) == OA_OK);   /* garbage */
    assert(oa_baro_ref_add(&ref, &cfg, 100002, 3u) == OA_OK);

    assert(oa_baro_ref_rejected(&ref) == 2u);
    assert(oa_baro_ref_samples(&ref) == 2u);
    assert(oa_baro_ref_locked(&ref) == true);
    assert(oa_baro_ref_pressure_pa(&ref, &locked) == OA_OK);
    assert(locked == 100001);
}

/* SPEC: oa_baro.h. "Once locked, reference_pa is frozen and further samples are
 * ignored ... a zero that moves during the flight moves the whole altitude
 * column with it." */
static void test_reference_is_frozen_after_lock(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    int32_t       locked = 0;
    int           i;

    config_with_band(&cfg, 50000, 115535, 0, 1);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 100000, 0u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == true);

    for (i = 0; i < 100; i++) {
        assert(oa_baro_ref_add(&ref, &cfg, 90000, 1000u) == OA_OK);
    }

    assert(oa_baro_ref_pressure_pa(&ref, &locked) == OA_OK);
    assert(locked == 100000);
    assert(oa_baro_ref_samples(&ref) == 1u);
}

/* SPEC: oa_baro.h. Starting a window "discards anything already accumulated",
 * including a previous lock. Arming somewhere else and arming again has to take
 * the reference again, because where you arm decides what the zero is. */
static void test_restart_discards_everything(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;

    config_with_band(&cfg, 50000, 115535, 0, 1);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);
    assert(oa_baro_ref_add(&ref, &cfg, 100000, 0u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == true);

    assert(oa_baro_ref_start(&ref, 5000u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == false);
    assert(oa_baro_ref_samples(&ref) == 0u);
    assert(oa_baro_ref_rejected(&ref) == 0u);
}

/* SPEC: telemetry-packet.md, t_ms is a u32 of milliseconds, and the payload's
 * own millisecond counter wraps. A settling window that straddles the wrap has
 * to measure the same interval as one that does not, or a payload armed at the
 * wrong moment would never lock. */
static void test_window_survives_a_millisecond_counter_wrap(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    uint32_t      start = 0xFFFFFF00u;

    config_with_band(&cfg, 50000, 115535, 1000, 2);
    assert(oa_baro_ref_start(&ref, start) == OA_OK);

    assert(oa_baro_ref_add(&ref, &cfg, 101325, start + 100u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == false);

    /* start + 1000 has wrapped past zero. */
    assert(oa_baro_ref_add(&ref, &cfg, 101325, start + 1000u) == OA_OK);
    assert(oa_baro_ref_locked(&ref) == true);
}

/* SPEC: oa_baro.h. oa_baro_is_plausible "returns false and sets *out_unset when
 * the band is not configured, so a missing band cannot be mistaken for a healthy
 * sensor". */
static void test_plausibility_band(void)
{
    oa_config_t cfg;
    bool        unset = false;

    config_with_band(&cfg, 90000, 110000, 0, 1);

    assert(oa_baro_is_plausible(&cfg, 90000, &unset) == true && unset == false);
    assert(oa_baro_is_plausible(&cfg, 110000, &unset) == true && unset == false);
    assert(oa_baro_is_plausible(&cfg, 89999, &unset) == false && unset == false);
    assert(oa_baro_is_plausible(&cfg, 110001, &unset) == false && unset == false);

    cfg.baro_plausible_max_pa = OA_UNSET;
    assert(oa_baro_is_plausible(&cfg, 100000, &unset) == false);
    assert(unset == true);

    /* No configuration at all is the same answer, and out_unset may be NULL. */
    assert(oa_baro_is_plausible(NULL, 100000, &unset) == false);
    assert(unset == true);
    assert(oa_baro_is_plausible(NULL, 100000, NULL) == false);
}

/* SPEC: oa_types.h. Every fallible entry point reports a NULL argument rather
 * than reporting failure through a value that is also a legal result. */
static void test_null_arguments(void)
{
    oa_baro_ref_t ref;
    oa_config_t   cfg;
    int32_t       out = 0;

    config_with_band(&cfg, 50000, 115535, 0, 1);

    assert(oa_baro_ref_start(NULL, 0u) == OA_ERR_NULL);
    assert(oa_baro_ref_add(NULL, &cfg, 101325, 0u) == OA_ERR_NULL);
    assert(oa_baro_ref_start(&ref, 0u) == OA_OK);
    assert(oa_baro_ref_add(&ref, NULL, 101325, 0u) == OA_ERR_NULL);
    assert(oa_baro_ref_pressure_pa(NULL, &out) == OA_ERR_NULL);
    assert(oa_baro_ref_pressure_pa(&ref, NULL) == OA_ERR_NULL);
    assert(oa_baro_ref_locked(NULL) == false);
    assert(oa_baro_ref_samples(NULL) == 0u);
    assert(oa_baro_ref_rejected(NULL) == 0u);
}

int main(void)
{
    test_zero_at_reference();
    test_negative_altitudes_are_not_clamped();
    test_monotonic_in_pressure();
    test_non_positive_reference_returns_zero();
    test_integer_error_against_double_reference();
    test_conversion_is_repeatable();

    test_add_before_start_is_a_state_error();
    test_unset_configuration_refuses();
    test_zero_minimum_samples_is_treated_as_unset();
    test_lock_needs_both_window_and_samples();
    test_lock_needs_the_sample_count();
    test_reference_is_the_mean_of_accepted_samples();
    test_implausible_samples_are_rejected_not_averaged();
    test_reference_is_frozen_after_lock();
    test_restart_discards_everything();
    test_window_survives_a_millisecond_counter_wrap();
    test_plausibility_band();
    test_null_arguments();

    printf("test_oa_baro: all assertions passed. Nothing here has run on hardware.\n");
    return 0;
}

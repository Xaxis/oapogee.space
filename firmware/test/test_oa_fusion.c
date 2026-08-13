/*
 * oApogee: conformance tests for oa_fusion.c.
 *
 * Plain C and assert, no framework. Every test says which specification claim it
 * is checking.
 *
 * There is no floating point anywhere in this file. The filter is integer
 * arithmetic on purpose, and a test that compared it against a floating point
 * model to within a tolerance would be a test that one day disagrees in flight
 * for a reason nobody can reproduce. The expected values below are either exact
 * consequences of the unit scales in the two specs and the SI definition of
 * standard gravity, or they are properties of the filter (convergence, the shape
 * of its step response, what it does when the barometer is faulted) worked out
 * on paper in oa_fusion.c and checked here.
 *
 * No flight has ever been fused, no barometer has ever been read, and no
 * accelerometer has ever been sampled. Everything fed to the filter here was
 * written by hand in this file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_fusion.h"

static void config_with_tau(oa_config_t *cfg, int32_t tau_ms, int32_t tau_boost_ms)
{
    /* The two fields oa_fusion.c reads, set directly rather than through
     * oa_config_set, so this file links against oa_fusion.c alone. */
    memset(cfg, 0, sizeof(*cfg));
    cfg->fusion_tau_ms       = tau_ms;
    cfg->fusion_tau_boost_ms = tau_boost_ms;
}

static oa_fusion_input_t sample(uint32_t dt_ms, int32_t baro_alt_cm, bool baro_valid,
                                int32_t accel_cg, bool in_boost)
{
    oa_fusion_input_t in;

    in.dt_ms       = dt_ms;
    in.baro_alt_cm = baro_alt_cm;
    in.baro_valid  = baro_valid;
    in.accel_cg    = accel_cg;
    in.in_boost    = in_boost;

    return in;
}

/* SPEC: oa_fusion.h. "Before that it has no altitude at all and reports
 * OA_ERR_EMPTY rather than zero, because zero is a legitimate altitude on the
 * pad and 'no estimate yet' is not the same thing." */
static void test_unseeded_reports_empty_not_zero(void)
{
    oa_fusion_t f;
    int32_t     alt = 12345;
    int32_t     vel = 12345;

    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_get(&f, &alt, &vel) == OA_ERR_EMPTY);

    /* And it did not write a zero into the caller's variables on the way out. */
    assert(alt == 12345);
    assert(vel == 12345);
}

/* SPEC: oa_fusion.h. oa_fusion_update returns "OA_ERR_STATE if the filter has
 * not been seeded". */
static void test_update_before_seed_is_a_state_error(void)
{
    oa_fusion_t       f;
    oa_config_t       cfg;
    oa_fusion_input_t in = sample(10u, 0, true, 0, false);

    config_with_tau(&cfg, 1000, 2000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_update(&f, &cfg, &in) == OA_ERR_STATE);
}

/* SPEC: oa_fusion.h. "OA_ERR_UNSET is not a formality. The crossover time
 * constant is the whole behaviour of this filter, and there is no defensible
 * value to fall back on." Both constants are required, including the boost one
 * on a sample that is not in boost: a filter that works on the pad and meets a
 * missing constant at burnout is worse than one that refuses now. */
static void test_unset_tau_refuses(void)
{
    oa_fusion_t       f;
    oa_config_t       cfg;
    oa_fusion_input_t in = sample(10u, 100, true, 0, false);
    int32_t           alt = 0;

    config_with_tau(&cfg, OA_UNSET, 2000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);
    assert(oa_fusion_update(&f, &cfg, &in) == OA_ERR_UNSET);

    config_with_tau(&cfg, 1000, OA_UNSET);
    assert(oa_fusion_update(&f, &cfg, &in) == OA_ERR_UNSET);

    /* A time constant of zero or less is not a time constant either, and it is
     * reported the same way rather than dividing by it. */
    config_with_tau(&cfg, 0, 2000);
    assert(oa_fusion_update(&f, &cfg, &in) == OA_ERR_UNSET);
    config_with_tau(&cfg, 1000, -5);
    assert(oa_fusion_update(&f, &cfg, &in) == OA_ERR_UNSET);

    /* The refusals moved nothing. */
    assert(oa_fusion_get(&f, &alt, NULL) == OA_OK);
    assert(alt == 0);
}

/* SPEC: oa_types.h. A NULL required argument is reported, never dereferenced. */
static void test_null_arguments(void)
{
    oa_fusion_t       f;
    oa_config_t       cfg;
    oa_fusion_input_t in = sample(10u, 0, true, 0, false);

    config_with_tau(&cfg, 1000, 2000);
    assert(oa_fusion_init(NULL) == OA_ERR_NULL);
    assert(oa_fusion_seed(NULL, 0, 0) == OA_ERR_NULL);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_update(NULL, &cfg, &in) == OA_ERR_NULL);
    assert(oa_fusion_update(&f, NULL, &in) == OA_ERR_NULL);
    assert(oa_fusion_update(&f, &cfg, NULL) == OA_ERR_NULL);
    assert(oa_fusion_get(NULL, NULL, NULL) == OA_ERR_NULL);
    assert(oa_fusion_unanchored_ms(NULL) == 0u);
}

/* SPEC: oa_fusion.h. Seeding is "from a known altitude and velocity, which on
 * the pad is the reference altitude and zero", and the estimate comes back in
 * the units the packet and the log use. Both out pointers are optional. */
static void test_seed_round_trips(void)
{
    oa_fusion_t f;
    int32_t     alt = 0;
    int32_t     vel = 0;

    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, -250, 37) == OA_OK);
    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(alt == -250);
    assert(vel == 37);

    assert(oa_fusion_get(&f, NULL, NULL) == OA_OK);
    assert(oa_fusion_unanchored_ms(&f) == 0u);
}

/* SPEC: telemetry-packet.md gives accel_cg in hundredths of g and vel_dm_s in
 * decimetres per second, log-format.md gives alt_cm in centimetres, and
 * oa_fusion.h fixes standard gravity at exactly 9.80665 m/s^2 by SI definition.
 * One g held for one second is therefore 9.80665 m/s, which is 98 decimetres per
 * second to the nearest unit the format carries, and the distance covered from
 * rest is 4.903 m, which is 490 cm.
 *
 * This is the test that catches a scale error. A filter whose acceleration
 * units are wrong by a factor of ten produces an altitude that looks like a
 * flight and is not one. */
static void test_one_g_for_one_second(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt = 0;
    int32_t     vel = 0;
    int         i;

    config_with_tau(&cfg, 1000, 1000);

    /* One step of a second, with the barometer faulted so nothing corrects. */
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);
    {
        oa_fusion_input_t in = sample(1000u, 0, false, 100, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }
    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(vel == 98);
    assert(alt == 490);

    /* And the same second in a hundred ten-millisecond steps. The trapezoid rule
     * integrates a constant acceleration exactly, so the two agree to within the
     * rounding of the fixed point state. */
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);
    for (i = 0; i < 100; i++) {
        oa_fusion_input_t in = sample(10u, 0, false, 100, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }
    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(vel == 98);
    assert(alt >= 489 && alt <= 491);
}

/* SPEC: oa_fusion.h, the baro_valid input. "When this is false the filter
 * integrates acceleration alone and does not correct toward the barometer, and
 * it accumulates drift while it does so, which is honest and bounded rather than
 * being corrected toward a wrong number."
 *
 * The barometer here is shouting a large wrong altitude. The estimate must
 * ignore it completely, not blend it in a little. */
static void test_faulted_barometer_is_ignored_entirely(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt = 0;
    int32_t     vel = 0;
    int         i;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 500; i++) {
        oa_fusion_input_t in = sample(10u, 999999, false, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }

    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(alt == 0);
    assert(vel == 0);

    /* SPEC: oa_fusion.h. unanchored_ms is "how long the estimate has been
     * running on integrated acceleration alone", so the caller can decide when
     * to stop trusting it and a log can show it afterwards. */
    assert(oa_fusion_unanchored_ms(&f) == 5000u);
}

/* SPEC: oa_fusion.h. "Zero when the last update was anchored to the barometer."
 * A single good sample ends the dead reckoning run. */
static void test_unanchored_resets_on_a_valid_sample(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int         i;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 10; i++) {
        oa_fusion_input_t in = sample(20u, 0, false, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }
    assert(oa_fusion_unanchored_ms(&f) == 200u);

    {
        oa_fusion_input_t in = sample(20u, 0, true, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }
    assert(oa_fusion_unanchored_ms(&f) == 0u);
}

/* SPEC: oa_fusion.h. "The barometer anchors the long term." With the
 * accelerometer reading zero and the barometer holding a constant altitude the
 * estimate must converge on the barometer and stay there. */
static void test_converges_on_the_barometer(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt = 0;
    int32_t     vel = 0;
    int         i;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 3000; i++) {
        oa_fusion_input_t in = sample(10u, 10000, true, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }

    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(alt >= 9998 && alt <= 10002);
    assert(vel == 0);
}

/* SPEC: oa_fusion.c, the step response worked out in the comment at the top of
 * the file: with both gains taken from one tau the error dynamics are critically
 * damped, and an estimate that starts a distance A from the barometer with no
 * velocity error follows e(t) = A (t/tau - 1) exp(-t/tau). That crosses zero at
 * tau, overshoots to A / e^2, which is 13.5%, at 2 tau, and crosses exactly
 * once.
 *
 * This test is the reason that paragraph is trustworthy. Critical damping is
 * often assumed to mean no overshoot; here it does not, and a reader looking at
 * a fused altitude that swung past a barometric one deserves to know whether
 * that is the filter or the flight. */
static void test_step_response_overshoots_once_by_thirteen_percent(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt        = 0;
    int32_t     previous   = 0;
    int32_t     peak       = 0;
    int         peak_step  = 0;
    int         crossings  = 0;
    int         i;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 3000; i++) {
        oa_fusion_input_t in = sample(10u, 10000, true, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
        assert(oa_fusion_get(&f, &alt, NULL) == OA_OK);

        if (alt > peak) {
            peak      = alt;
            peak_step = i;
        }
        if (i > 0 && ((previous < 10000) != (alt < 10000))) {
            crossings++;
        }
        previous = alt;
    }

    printf("  fusion: step overshoot %d cm of 10000 at %d ms, %d crossing(s)\n",
           peak - 10000, (peak_step + 1) * 10, crossings);

    /* A / e^2 is 1353 cm of 10000. The discrete filter reaches 1331. */
    assert(peak - 10000 >= 1200 && peak - 10000 <= 1400);

    /* 2 tau is 2000 ms. */
    assert((peak_step + 1) * 10 >= 1800 && (peak_step + 1) * 10 <= 2200);

    /* Once, not repeatedly. A damping ratio below 1 would ring. */
    assert(crossings == 1);
}

/* SPEC: oa_fusion.c, QUANTISATION FLOOR. The velocity correction per sample is
 * the altitude correction divided by 2 tau, so with dt much smaller than tau it
 * eventually rounds to nothing and the velocity stops being corrected while it
 * still holds a small error. The bound is (tau_ms / dt_ms) counts of the Q8
 * velocity state, and the altitude then settles at about that velocity times
 * tau / 2.
 *
 * This test exists to make that limitation a measured number rather than a
 * worry. If it ever fails, the filter has a different problem from the one
 * documented, which is worth knowing immediately. */
static void test_quantisation_floor_stays_inside_its_bound(void)
{
    static const int32_t taus[]  = { 500, 1000, 2000, 5000 };
    static const uint32_t dts[]  = { 10u, 10u, 5u, 5u };
    size_t               c;

    for (c = 0; c < sizeof(taus) / sizeof(taus[0]); c++) {
        oa_fusion_t f;
        oa_config_t cfg;
        int32_t     alt   = 0;
        int32_t     bound = taus[c] / (int32_t)dts[c];
        int         i;

        config_with_tau(&cfg, taus[c], taus[c]);
        assert(oa_fusion_init(&f) == OA_OK);
        assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

        /* Twenty tau, which is far past settled. */
        for (i = 0; i < (int)((20u * (uint32_t)taus[c]) / dts[c]); i++) {
            oa_fusion_input_t in = sample(dts[c], 10000, true, 0, false);
            assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
        }

        assert(oa_fusion_get(&f, &alt, NULL) == OA_OK);
        printf("  fusion: tau %d ms at dt %u ms settles %d cm high, "
               "velocity residue %d of %d counts\n",
               taus[c], dts[c], alt - 10000, f.vel_dm_s_fx, bound);

        assert(f.vel_dm_s_fx >= -bound && f.vel_dm_s_fx <= bound);

        /* The altitude offset is that velocity held against the correction:
         * v [dm/s] * 10 cm/dm * tau/2 [s]. Three centimetres of slack, because
         * the balance is struck between two increments that are themselves
         * rounded to whole counts, and the reported altitude is rounded again. */
        assert(alt - 10000
               <= ((f.vel_dm_s_fx * 10 * taus[c]) / (256 * 2 * 1000)) + 3);
    }
}

/* SPEC: oa_fusion.h. The filter produces a fused vertical velocity, and the
 * velocity has to be corrected by the barometer too or an accelerometer bias
 * would drift it without limit. With the accelerometer reading exactly zero and
 * the barometer climbing at a constant rate, the reported velocity must converge
 * on that rate: it can only get there through the correction path.
 *
 * The rate here is 5 m/s, which is 50 decimetres per second in the units the
 * packet spec uses, expressed as 5 cm per 10 ms sample. */
static void test_velocity_tracks_a_barometric_ramp(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     baro_cm = 0;
    int32_t     vel     = 0;
    int         i;

    config_with_tau(&cfg, 500, 500);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 1000; i++) {
        oa_fusion_input_t in;
        baro_cm += 5;
        in = sample(10u, baro_cm, true, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }

    assert(oa_fusion_get(&f, NULL, &vel) == OA_OK);
    assert(vel >= 49 && vel <= 51);
}

/* SPEC: oa_fusion.h, the in_boost input. It "selects fusion_tau_boost_ms instead
 * of fusion_tau_ms, because airflow over the static ports during boost disturbs
 * the pressure the sensor sees, and the estimate has to lean on integrated
 * acceleration through it". A longer boost constant must therefore pull less
 * hard toward the barometer over the same interval. */
static void test_in_boost_selects_the_boost_constant(void)
{
    oa_fusion_t normal;
    oa_fusion_t boosting;
    oa_config_t cfg;
    int32_t     alt_normal   = 0;
    int32_t     alt_boosting = 0;
    int         i;

    config_with_tau(&cfg, 200, 20000);

    assert(oa_fusion_init(&normal) == OA_OK);
    assert(oa_fusion_seed(&normal, 0, 0) == OA_OK);
    assert(oa_fusion_init(&boosting) == OA_OK);
    assert(oa_fusion_seed(&boosting, 0, 0) == OA_OK);

    for (i = 0; i < 50; i++) {
        oa_fusion_input_t in_normal   = sample(10u, 10000, true, 0, false);
        oa_fusion_input_t in_boosting = sample(10u, 10000, true, 0, true);
        assert(oa_fusion_update(&normal, &cfg, &in_normal) == OA_OK);
        assert(oa_fusion_update(&boosting, &cfg, &in_boosting) == OA_OK);
    }

    assert(oa_fusion_get(&normal, &alt_normal, NULL) == OA_OK);
    assert(oa_fusion_get(&boosting, &alt_boosting, NULL) == OA_OK);
    assert(alt_normal > alt_boosting);
}

/* SPEC: oa_fusion.h. "Integer arithmetic gives a result that is bit-for-bit
 * identical on the host tests and on the target." This cannot check the target,
 * which does not exist, but it can check that the filter is a pure function of
 * its inputs and carries no hidden state: two runs of the same sequence must
 * leave identical bits. */
static void test_is_deterministic(void)
{
    oa_fusion_t first;
    oa_fusion_t second;
    oa_config_t cfg;
    int         run;
    int         i;

    config_with_tau(&cfg, 750, 1500);

    for (run = 0; run < 2; run++) {
        oa_fusion_t *f = (run == 0) ? &first : &second;

        assert(oa_fusion_init(f) == OA_OK);
        assert(oa_fusion_seed(f, -37, 4) == OA_OK);

        for (i = 0; i < 300; i++) {
            oa_fusion_input_t in = sample((uint32_t)(5 + (i % 7)),
                                          (int32_t)(i * 13 - 500),
                                          (i % 11) != 0,
                                          (int32_t)((i % 23) * 17 - 200),
                                          (i % 5) == 0);
            assert(oa_fusion_update(f, &cfg, &in) == OA_OK);
        }
    }

    assert(memcmp(&first, &second, sizeof(first)) == 0);
}

/* SPEC: oa_fusion.h. The filter is driven by the measured interval rather than
 * an assumed rate. An interval of zero advances nothing and corrects nothing,
 * which is the limit of both, rather than being a special case that divides by
 * it. */
static void test_zero_interval_changes_nothing(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt = 0;
    int32_t     vel = 0;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 1234, 56) == OA_OK);

    {
        oa_fusion_input_t in = sample(0u, 999999, true, 30000, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }

    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(alt == 1234);
    assert(vel == 56);
}

/* SPEC: telemetry-packet.md, alt_cm is signed and negative altitudes are
 * legitimate. A payload that lands below the pad has to be able to hold the
 * number on the way down, so the filter must be signed throughout and must not
 * round asymmetrically about zero. */
static void test_descent_below_the_pad(void)
{
    oa_fusion_t f;
    oa_config_t cfg;
    int32_t     alt = 0;
    int32_t     vel = 0;
    int         i;

    config_with_tau(&cfg, 1000, 1000);
    assert(oa_fusion_init(&f) == OA_OK);
    assert(oa_fusion_seed(&f, 0, 0) == OA_OK);

    for (i = 0; i < 2000; i++) {
        oa_fusion_input_t in = sample(10u, -5000, true, 0, false);
        assert(oa_fusion_update(&f, &cfg, &in) == OA_OK);
    }

    assert(oa_fusion_get(&f, &alt, &vel) == OA_OK);
    assert(alt >= -5002 && alt <= -4998);
    assert(vel == 0);
}

int main(void)
{
    test_unseeded_reports_empty_not_zero();
    test_update_before_seed_is_a_state_error();
    test_unset_tau_refuses();
    test_null_arguments();
    test_seed_round_trips();
    test_one_g_for_one_second();
    test_faulted_barometer_is_ignored_entirely();
    test_unanchored_resets_on_a_valid_sample();
    test_converges_on_the_barometer();
    test_step_response_overshoots_once_by_thirteen_percent();
    test_quantisation_floor_stays_inside_its_bound();
    test_velocity_tracks_a_barometric_ramp();
    test_in_boost_selects_the_boost_constant();
    test_is_deterministic();
    test_zero_interval_changes_nothing();
    test_descent_below_the_pad();

    printf("test_oa_fusion: all assertions passed. Nothing here has run on hardware.\n");
    return 0;
}

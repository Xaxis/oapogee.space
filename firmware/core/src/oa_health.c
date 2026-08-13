/*
 * oApogee core: instrument health.
 *
 * Implements oapogee/oa_health.h. Read that header for what the three detectable
 * failures are and why they are worth detecting. This file carries the
 * arithmetic and the reasons behind it.
 *
 * THERE ARE NO THRESHOLDS IN THIS FILE. Every number these checks need is
 * unmeasured, so every one of them arrives as an oa_tunable_t that starts at
 * OA_UNSET, either from oa_health_limits_t or, for the barometer's plausible
 * band and its rate ceiling, from oa_config_t. A check whose threshold is not
 * usable is not performed, and the bit that says so goes back to the caller in
 * out->checks_unset. Nothing here fills in a number.
 *
 * THIS MODULE NEVER OMITS A FIELD. Its entire output is flags, fault bits,
 * unset bits, and two validity bits. There is no return value that means "skip
 * this field" and no way to construct one, because omission would change the
 * packet length, and a sentinel would produce a plot with a spike in it rather
 * than a gap. See the flags section of docs/spec/telemetry-packet.md.
 *
 * NO FLOATING POINT AND NO DIVISION IN THE SAMPLE PATH. oa_health_step runs once
 * per sample in an interrupt-adjacent context. The one comparison that would
 * naturally be a division is cross multiplied instead, and the reason it is
 * allowed to reach 64 bits is written out where it happens.
 *
 * Nothing in this file has run on hardware, and no sensor has ever failed in
 * front of it.
 */

#include "oapogee/oa_health.h"

#include "oapogee/oa_baro.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * The limit table, expanded three ways.
 *
 * Names, units and reasons come from the same X macro the struct members and the
 * enumerators come from, so a limit cannot be added to one and missed by the
 * others. The offset table is what lets a caller reach a named member from an
 * index without a switch that could fall out of order with the enumeration.
 * ------------------------------------------------------------------------ */

/* The six cfg this module applies, in the order an operator reading the
 * configuration would meet them. They are ordinary configuration fields now, so
 * there is no second table to keep in step and no offsets to walk. */
static const oa_config_field_t oa_health_fields[] = {
    OA_CFG_BARO_STUCK_SAMPLES, OA_CFG_BARO_STALE_MS,   OA_CFG_IMU_STUCK_SAMPLES,
    OA_CFG_IMU_STALE_MS,       OA_CFG_IMU_ACCEL_MAX_MG, OA_CFG_IMU_GYRO_MAX_CDPS,
};

static bool oa_health_value_usable(oa_config_field_t field, oa_tunable_t value)
{
    switch (field) {
    case OA_CFG_BARO_STUCK_SAMPLES:
    case OA_CFG_IMU_STUCK_SAMPLES:
        /* A count of one means the first reading of a run is already a repeat,
         * which faults every sensor on its first sample. */
        return value >= 2;

    default:
        /* An interval and a magnitude are both non-negative. Zero is left alone:
         * a stale interval of zero declares a sensor dead the moment a sample
         * arrives without a fresh read, and a magnitude ceiling of zero faults
         * any nonzero axis. Both are aggressive rather than nonsensical, and
         * choosing which side of that line an operator is allowed to be on is
         * not this module's decision to make. */
        return value >= 0;
    }
}

/* Read a limit if it is set and usable. Returns false when the check that needs
 * it must not run, which is the same answer whether the limit was never
 * measured or was written down as something that is not a quantity: the caller's
 * question is whether the check ran. oa_health_limits_check is where an
 * unusable limit is rejected loudly, once, at startup. */
static bool oa_health_limit_usable(const oa_config_t *cfg,
                                   oa_config_field_t  field,
                                   oa_tunable_t      *out)
{
    oa_tunable_t value = OA_UNSET;

    if (oa_config_get(cfg, field, &value) != OA_OK) {
        return false;
    }
    if (!OA_IS_SET(value) || !oa_health_value_usable(field, value)) {
        return false;
    }
    *out = value;
    return true;
}

/* ---------------------------------------------------------------------------
 * Integer helpers.
 * ------------------------------------------------------------------------ */

/* |a - b| for two int32 values.
 *
 * Computed in unsigned arithmetic because the difference of two int32 values
 * does not fit in an int32: INT32_MAX minus INT32_MIN is 2^32 - 1, and the
 * signed subtraction would be undefined behaviour rather than a large number.
 * A barometer that has failed can return either endpoint, so this is a case that
 * happens rather than a hypothetical. */
static uint32_t oa_health_abs_diff(int32_t a, int32_t b)
{
    if (a >= b) {
        return (uint32_t)a - (uint32_t)b;
    }
    return (uint32_t)b - (uint32_t)a;
}

/* Magnitude of one sensor axis, widened first.
 *
 * -32768 has no positive counterpart in an int16, so negating in place would
 * overflow on exactly the reading a saturated axis produces. */
static int32_t oa_health_abs_axis(int16_t v)
{
    const int32_t w = (int32_t)v;

    return (w < 0) ? -w : w;
}

/* Reject a limit set to something the checks cannot use, once, at startup.
 *
 * Returns OA_OK, OA_ERR_NULL, or OA_ERR_RANGE with *out_field naming the first
 * offending field. An unset limit is never an error: unset means the check is
 * not performed, which out->checks_unset reports on every sample. */
oa_result_t oa_health_config_check(const oa_config_t *cfg, oa_config_field_t *out_field)
{
    size_t i;

    if (cfg == NULL) {
        return OA_ERR_NULL;
    }

    for (i = 0; i < (sizeof oa_health_fields / sizeof oa_health_fields[0]); i++) {
        const oa_config_field_t field = oa_health_fields[i];
        oa_tunable_t            value = OA_UNSET;

        if (oa_config_get(cfg, field, &value) != OA_OK) {
            return OA_ERR_RANGE;
        }
        if (!OA_IS_SET(value)) {
            continue;
        }
        if (!oa_health_value_usable(field, value)) {
            if (out_field != NULL) {
                *out_field = field;
            }
            return OA_ERR_RANGE;
        }
    }

    return OA_OK;
}

const char *oa_health_check_name(oa_health_check_t check)
{
    /* A switch rather than a table indexed by bit position, because the switch
     * answers NULL for a value that is not exactly one defined bit without any
     * separate popcount: a combination of two bits matches no case. Printing
     * something that looks like a check name for a value that is not one is the
     * failure this rule exists to prevent, and it is the same rule oa_flag_name
     * follows. */
    switch (check) {
    case OA_HEALTH_CHECK_BARO_RANGE:
        return "baro_range";
    case OA_HEALTH_CHECK_BARO_RATE:
        return "baro_rate";
    case OA_HEALTH_CHECK_BARO_STUCK:
        return "baro_stuck";
    case OA_HEALTH_CHECK_BARO_STALE:
        return "baro_stale";
    case OA_HEALTH_CHECK_IMU_ACCEL_RANGE:
        return "imu_accel_range";
    case OA_HEALTH_CHECK_IMU_GYRO_RANGE:
        return "imu_gyro_range";
    case OA_HEALTH_CHECK_IMU_STUCK:
        return "imu_stuck";
    case OA_HEALTH_CHECK_IMU_STALE:
        return "imu_stale";
    default:
        break;
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * The sample path.
 * ------------------------------------------------------------------------ */

static bool oa_health_rate_exceeded(uint32_t delta_pa, uint32_t elapsed_ms, uint32_t max_pa_s)
{
    return ((uint64_t)delta_pa * 1000u) > ((uint64_t)max_pa_s * (uint64_t)elapsed_ms);
}

oa_result_t oa_health_init(oa_health_t *health, uint32_t now_ms)
{
    if (health == NULL) {
        return OA_ERR_NULL;
    }

    memset(health, 0, sizeof *health);

    /* The staleness clocks start now rather than at zero. Starting them at zero
     * would make the first sample of a payload that has been powered for a while
     * arrive already stale, and the fault would appear on a healthy sensor at the
     * one moment an operator is watching the console. */
    health->baro_fresh_ms = now_ms;
    health->imu_fresh_ms  = now_ms;

    /* baro_have_sample and imu_have_sample stay false, which is what makes
     * baro_valid and imu_valid false until a reading actually arrives. No
     * measurement yet is not the same thing as measured and fine. */
    return OA_OK;
}

/* The barometer half. Returns the check bits that fired, and ORs the bits for
 * checks it could not perform into *unset. */
static uint32_t oa_health_step_baro(oa_health_t              *health,
                                    const oa_config_t        *cfg,
                                    const oa_health_input_t  *in,
                                    uint32_t                 *unset)
{
    uint32_t     faults       = 0u;
    bool         range_failed = false;
    bool         band_unset   = false;
    bool         plausible;
    oa_tunable_t limit;

    /* --- range, against the band in oa_config_t ---------------------------- */

    /* Asked on every sample, including one where the read failed, because the
     * answer to "is the band configured" does not depend on this sample. The
     * plausibility answer is used only when the reading is fresh: on a failed
     * read the input carries the last value actually read, and re-judging it
     * would report the same fault twice. */
    plausible = oa_baro_is_plausible(cfg, in->pressure_pa, &band_unset);
    if (band_unset) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_BARO_RANGE;
    } else if (in->baro_ok && !plausible) {
        faults |= (uint32_t)OA_HEALTH_CHECK_BARO_RANGE;
        range_failed = true;
    } else {
        /* In band, or no fresh reading to judge. */
    }

    /* --- rate, against the optional ceiling in oa_config_t ------------------ */

    if (!OA_IS_SET(cfg->baro_max_rate_pa_s) || cfg->baro_max_rate_pa_s < 0) {
        /* Unset means no rate check, which is what oa_config.h documents for
         * this field. A negative ceiling is not a rate, and it is reported
         * through the same bit for the reason given on oa_health_limit_usable. */
        *unset |= (uint32_t)OA_HEALTH_CHECK_BARO_RATE;
    } else if (in->baro_ok && !range_failed) {
        if (health->baro_rate_have_ref) {
            /* Unsigned subtraction, so the u32 millisecond counter wrapping at
             * about 49 days produces the true interval rather than an enormous
             * one. The header requires only that the clock does not go
             * backwards. */
            const uint32_t elapsed = in->t_ms - health->baro_rate_ref_ms;

            if (elapsed > 0u) {
                const uint32_t delta =
                    oa_health_abs_diff(in->pressure_pa, health->baro_rate_ref_pa);

                if (oa_health_rate_exceeded(delta, elapsed, (uint32_t)cfg->baro_max_rate_pa_s)) {
                    faults |= (uint32_t)OA_HEALTH_CHECK_BARO_RATE;
                }

                health->baro_rate_ref_pa = in->pressure_pa;
                health->baro_rate_ref_ms = in->t_ms;
            }
            /* Two samples inside the same millisecond leave the reference where
             * it is. The interval is not resolvable at this clock's resolution,
             * and dividing a real pressure change by an interval of zero would
             * call any change at all a jump. The pair is measured against the
             * next reading instead. */
        } else {
            health->baro_rate_ref_pa   = in->pressure_pa;
            health->baro_rate_ref_ms   = in->t_ms;
            health->baro_rate_have_ref = true;
        }
    } else {
        /* A reading the range check rejected must not become the point the next
         * one is measured against, or one spike trips the rate check twice: once
         * going out and once coming back. */
    }

    /* --- stuck -------------------------------------------------------------- */

    if (in->baro_ok) {
        if (health->baro_have_sample && in->pressure_pa == health->baro_last_pa) {
            if (health->baro_repeat < UINT32_MAX) {
                health->baro_repeat++;
            }
        } else {
            /* The first reading of a run counts as one, so the limit is a count
             * of identical readings and not a count of repeats after the first.
             * The header says so, and the two differ by one, which is exactly
             * the kind of off by one that would make a measured threshold wrong
             * in a way nobody could see. */
            health->baro_repeat = 1u;
        }

        health->baro_last_pa     = in->pressure_pa;
        health->baro_have_sample = true;
        health->baro_fresh_ms    = in->t_ms;
    }

    if (!oa_health_limit_usable(cfg, OA_CFG_BARO_STUCK_SAMPLES, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_BARO_STUCK;
    } else if (health->baro_repeat >= (uint32_t)limit) {
        faults |= (uint32_t)OA_HEALTH_CHECK_BARO_STUCK;
    } else {
        /* Not stuck yet. */
    }

    /* --- stale -------------------------------------------------------------- */

    if (!oa_health_limit_usable(cfg, OA_CFG_BARO_STALE_MS, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_BARO_STALE;
    } else if ((in->t_ms - health->baro_fresh_ms) > (uint32_t)limit) {
        faults |= (uint32_t)OA_HEALTH_CHECK_BARO_STALE;
    } else {
        /* Fresh enough. A successful read this sample set baro_fresh_ms to
         * in->t_ms a few lines ago, so the interval is zero and this cannot
         * fire on a sensor that is answering. */
    }

    return faults;
}

/* The IMU half. Same shape as the barometer half. */
static uint32_t oa_health_step_imu(oa_health_t              *health,
                                   const oa_config_t *cfg,
                                   const oa_health_input_t  *in,
                                   uint32_t                 *unset)
{
    uint32_t     faults = 0u;
    oa_tunable_t limit;
    int          axis;

    /* --- accelerometer range ------------------------------------------------ */

    if (!oa_health_limit_usable(cfg, OA_CFG_IMU_ACCEL_MAX_MG, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE;
    } else if (in->imu_ok) {
        for (axis = 0; axis < 3; axis++) {
            /* Strictly greater than. A reading AT full scale is saturation, and
             * the log format describes a saturated axis as a flat plateau rather
             * than a fault. This limit is here to catch a part reporting past its
             * own range, which is a different thing. */
            if (oa_health_abs_axis(in->accel_mg[axis]) > limit) {
                faults |= (uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE;
            }
        }
    } else {
        /* No fresh reading to judge. */
    }

    /* --- gyroscope range ---------------------------------------------------- */

    if (!oa_health_limit_usable(cfg, OA_CFG_IMU_GYRO_MAX_CDPS, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_IMU_GYRO_RANGE;
    } else if (in->imu_ok) {
        for (axis = 0; axis < 3; axis++) {
            if (oa_health_abs_axis(in->gyro_cdps[axis]) > limit) {
                faults |= (uint32_t)OA_HEALTH_CHECK_IMU_GYRO_RANGE;
            }
        }
    } else {
        /* No fresh reading to judge. */
    }

    /* --- stuck, all six axes at once ---------------------------------------- */

    if (in->imu_ok) {
        bool identical = health->imu_have_sample;

        for (axis = 0; axis < 3; axis++) {
            if (in->accel_mg[axis] != health->imu_last_accel_mg[axis] ||
                in->gyro_cdps[axis] != health->imu_last_gyro_cdps[axis]) {
                identical = false;
            }
        }

        if (identical) {
            if (health->imu_repeat < UINT32_MAX) {
                health->imu_repeat++;
            }
        } else {
            health->imu_repeat = 1u;
        }

        /* All six axes, not any one of them. A single axis can legitimately read
         * the same value twice while the others move, and faulting on that would
         * fault a payload lying still on the pad. An exact repeat across six
         * noisy axes is the thing that does not happen to a live part. */
        memcpy(health->imu_last_accel_mg, in->accel_mg, sizeof health->imu_last_accel_mg);
        memcpy(health->imu_last_gyro_cdps, in->gyro_cdps, sizeof health->imu_last_gyro_cdps);
        health->imu_have_sample = true;
        health->imu_fresh_ms    = in->t_ms;
    }

    if (!oa_health_limit_usable(cfg, OA_CFG_IMU_STUCK_SAMPLES, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_IMU_STUCK;
    } else if (health->imu_repeat >= (uint32_t)limit) {
        faults |= (uint32_t)OA_HEALTH_CHECK_IMU_STUCK;
    } else {
        /* Not stuck yet. */
    }

    /* --- stale -------------------------------------------------------------- */

    if (!oa_health_limit_usable(cfg, OA_CFG_IMU_STALE_MS, &limit)) {
        *unset |= (uint32_t)OA_HEALTH_CHECK_IMU_STALE;
    } else if ((in->t_ms - health->imu_fresh_ms) > (uint32_t)limit) {
        faults |= (uint32_t)OA_HEALTH_CHECK_IMU_STALE;
    } else {
        /* Fresh enough. */
    }

    return faults;
}

oa_result_t oa_health_step(oa_health_t              *health,
                           const oa_config_t        *cfg,
                           const oa_health_input_t  *in,
                           oa_health_output_t       *out)
{
    uint32_t faults = 0u;
    uint32_t unset  = 0u;
    uint32_t baro_faults;
    uint32_t imu_faults;

    if (health == NULL || cfg == NULL || cfg == NULL || in == NULL || out == NULL) {
        return OA_ERR_NULL;
    }

    memset(out, 0, sizeof *out);

    baro_faults = oa_health_step_baro(health, cfg, in, &unset);
    imu_faults  = oa_health_step_imu(health, cfg, in, &unset);
    faults      = baro_faults | imu_faults;

    if (baro_faults != 0u) {
        out->flags = (uint8_t)(out->flags | (uint8_t)OA_FLAG_BARO_FAULT);
        health->baro_fault_samples++;
    }
    if (imu_faults != 0u) {
        out->flags = (uint8_t)(out->flags | (uint8_t)OA_FLAG_IMU_FAULT);
        health->imu_fault_samples++;
    }

    out->faults       = faults;
    out->checks_unset = unset;

    /* A reading has to have been seen AND the fault flag has to be clear. False
     * before the first successful read, because a filter told that a sensor it
     * has never heard from is valid will seed itself from whatever was in the
     * caller's sample struct. */
    out->baro_valid = health->baro_have_sample && (baro_faults == 0u);
    out->imu_valid  = health->imu_have_sample && (imu_faults == 0u);

    health->samples++;

    return OA_OK;
}

uint32_t oa_health_baro_fault_samples(const oa_health_t *health)
{
    return (health == NULL) ? 0u : health->baro_fault_samples;
}

uint32_t oa_health_imu_fault_samples(const oa_health_t *health)
{
    return (health == NULL) ? 0u : health->imu_fault_samples;
}

uint32_t oa_health_samples(const oa_health_t *health)
{
    return (health == NULL) ? 0u : health->samples;
}

/* One last note on what is deliberately absent.
 *
 * There is no oa_health_reset, no way to clear a fault counter, and no sticky
 * fault bit. A fault is decided per sample from that sample's evidence, and the
 * counters only ever go up. A payload that could clear its own fault history
 * could produce a flight summary that disagrees with the flags in its own log
 * records, and the log is supposed to be the thing that can be checked. */

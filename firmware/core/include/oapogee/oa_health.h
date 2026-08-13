/*
 * oApogee instrument health.
 *
 * Decides, once per sample, whether the barometer and the IMU are working, and
 * raises OA_FLAG_BARO_FAULT and OA_FLAG_IMU_FAULT when they are not.
 *
 * WHY THIS IS A MODULE AND NOT A COUPLE OF IF STATEMENTS
 *
 * A failed sensor does not stop returning numbers. A barometer that has lost its
 * bus returns the last value it latched, forever, and it looks exactly like a
 * rocket sitting perfectly still. An IMU whose configuration was corrupted
 * returns full scale on every axis, and it looks exactly like a boost. Both of
 * those decode cleanly, plot cleanly, and are wrong, which is the only kind of
 * wrong this project actually fears. So the question "is this sensor alive" gets
 * asked deliberately, in one place, with the answer written into the flags byte
 * that both the packet and the log record carry.
 *
 * Three failures are detectable without knowing anything about the flight:
 *
 *   stuck        the same reading, repeated. A live sensor has noise.
 *   out of range a reading outside the band the part can physically produce.
 *   stale        no successful read for too long.
 *
 * WHAT THIS MODULE NEVER DOES
 *
 * It never omits a field. A faulted sensor's fields carry the last value read
 * and the flag is what tells you not to trust them, because omission would
 * change the packet length and a sentinel would produce a plot with a spike in
 * it rather than a gap. There is deliberately no output here that says "skip
 * this field": the only thing this module produces is flags, counters, and a
 * validity bit for the fusion filter and the state machine.
 *
 * It also drives nothing. It is in core, it has no global state, and the caller
 * owns the context. See firmware/SAFETY.md.
 *
 * NOTHING HAS BEEN MEASURED
 *
 * Every threshold these checks need is unmeasured, so every one of them is an
 * oa_tunable_t that starts at OA_UNSET, exactly like the flight thresholds in
 * oa_config_t. A check whose threshold is unset is not performed, and the output
 * says which checks were not performed, in `checks_unset`. That distinction is
 * the point: a health monitor that reported "no fault" when it had not looked
 * would be worse than no health monitor, because somebody would believe it.
 *
 * Every one of them lives in OA_CONFIG_FIELDS in oa_config.h, which is the
 * normative home for every tunable in this firmware. They used to live in a
 * second table here, and the cost was not untidiness: the configuration parser
 * only ever knew OA_CONFIG_FIELDS, so nothing could set them, and a check whose
 * threshold can never be set is a check that never runs.
 *
 * Decided: the high-g accelerometer gets the same treatment. OA_FLAG_HIGH_G
 * means "present and healthy", and a health module with no opinion about the
 * second half of that makes the flag a claim nothing checks, which is worse
 * than not having it.
 * TODO(confirm-on-hardware): add its plausibility limits once the part's
 * full-scale range is known, since the limits are expressed against it.
 *
 * Nothing in this file has run on hardware, and no sensor has ever failed in
 * front of it.
 */

#ifndef OAPOGEE_OA_HEALTH_H
#define OAPOGEE_OA_HEALTH_H

#include "oapogee/oa_config.h"
#include "oapogee/oa_flags.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * THE LIMIT TABLE
 *
 * X(name, NAME, unit, why)
 *
 * Same shape and same rules as OA_CONFIG_FIELDS: `why` is the measurement that
 * would close the field, so a payload can say which number is missing and what
 * would settle it rather than just refusing to check.
 *
 * The barometer's plausible band and its maximum rate of change are NOT here.
 * They are already in oa_config_t as baro_plausible_min_pa, baro_plausible_max_pa
 * and baro_max_rate_pa_s, and this module reads them from there through
 * oa_baro_is_plausible. One tunable, one home.
 * ------------------------------------------------------------------------ */

/* The six limits these checks need now live in OA_CONFIG_FIELDS in oa_config.h,
 * alongside the three baro thresholds that were always there. They were in a
 * second table here, which the configuration parser had never heard of, so
 * nothing could set them: every check was permanently unset, BARO_FAULT and
 * IMU_FAULT could never be raised, and the baro_valid gate in the state machine
 * could never fire from a real fault. One normative home, and the parser
 * reaches all of them. */

/* Reject a limit set to something the checks cannot use, once, at startup
 * rather than in the sample path, so the sample path can apply a limit without
 * re-deciding whether it makes sense every time.
 *
 * Returns OA_OK, OA_ERR_NULL, or OA_ERR_RANGE with *out_field naming the first
 * offending field in table order. An unset limit is never an error: unset means
 * the check is not performed, which out->checks_unset reports every sample. */
oa_result_t oa_health_config_check(const oa_config_t *cfg, oa_config_field_t *out_field);

/* ---------------------------------------------------------------------------
 * The checks, as bits.
 *
 * One enumeration used for two things, deliberately. In `faults` a bit means
 * this check fired on this sample. In `checks_unset` the same bit means this
 * check was not performed, because the number it needs has not been measured.
 * Neither can be mistaken for the other, and a caller printing both gets a
 * complete account of what the payload knows and what it does not.
 * ------------------------------------------------------------------------ */

typedef enum {
    OA_HEALTH_CHECK_BARO_RANGE      = 1u << 0, /* baro_plausible_min_pa / _max_pa, in oa_config_t */
    OA_HEALTH_CHECK_BARO_RATE       = 1u << 1, /* baro_max_rate_pa_s, in oa_config_t */
    OA_HEALTH_CHECK_BARO_STUCK      = 1u << 2,
    OA_HEALTH_CHECK_BARO_STALE      = 1u << 3,
    OA_HEALTH_CHECK_IMU_ACCEL_RANGE = 1u << 4,
    OA_HEALTH_CHECK_IMU_GYRO_RANGE  = 1u << 5,
    OA_HEALTH_CHECK_IMU_STUCK       = 1u << 6,
    OA_HEALTH_CHECK_IMU_STALE       = 1u << 7
} oa_health_check_t;

#define OA_HEALTH_CHECKS_BARO                                                    \
    ((uint32_t)OA_HEALTH_CHECK_BARO_RANGE | (uint32_t)OA_HEALTH_CHECK_BARO_RATE | \
     (uint32_t)OA_HEALTH_CHECK_BARO_STUCK | (uint32_t)OA_HEALTH_CHECK_BARO_STALE)

#define OA_HEALTH_CHECKS_IMU                                                                    \
    ((uint32_t)OA_HEALTH_CHECK_IMU_ACCEL_RANGE | (uint32_t)OA_HEALTH_CHECK_IMU_GYRO_RANGE |     \
     (uint32_t)OA_HEALTH_CHECK_IMU_STUCK | (uint32_t)OA_HEALTH_CHECK_IMU_STALE)

/* Short name of a single check bit, for the console and for test failures.
 * NULL if `check` is not exactly one defined bit, under the same rule as
 * oa_flag_name: a caller must not be able to print something that looks like a
 * check name for a value that is not one. */
const char *oa_health_check_name(oa_health_check_t check);

/* ---------------------------------------------------------------------------
 * Input.
 *
 * One sample's worth. The values are in the units the log format stores, which
 * are the units the port layer produces, so nothing rescales anything between
 * the sensor and this check.
 *
 * This struct exists rather than the port's sample structs because core cannot
 * include port headers: core is compiled without port/include on its include
 * path, which is the second fence around the passive boundary.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* Monotonic milliseconds. Uptime before arming and still uptime after: this
     * module never appears in a packet or a record, so it does not care which
     * zero the caller is using, only that the clock does not go backwards. */
    uint32_t t_ms;

    /* True when this sample's barometer read succeeded. A read that failed left
     * the caller's sample struct untouched, per oa_port_baro_read, so the values
     * below are the last ones actually read and are ignored by every check that
     * needs a fresh reading. */
    bool    baro_ok;
    int32_t pressure_pa;

    bool    imu_ok;
    int16_t accel_mg[3];
    int16_t gyro_cdps[3];
} oa_health_input_t;

/* ---------------------------------------------------------------------------
 * Context.
 *
 * Caller-owned so there is no allocation and no global state. Visible so it can
 * live in a caller's static storage. Treat the members as private.
 * ------------------------------------------------------------------------ */

typedef struct {
    int32_t  baro_last_pa;
    bool     baro_have_sample;
    uint32_t baro_repeat;      /* Consecutive identical readings, including the first. */
    uint32_t baro_fresh_ms;    /* Timestamp of the last successful read. */
    uint32_t baro_fault_samples;

    /* The rate check's reference point. Held separately from baro_last_pa
     * because a reading rejected by the range check must not become the point
     * the next one is measured against: one spike would otherwise trip the rate
     * check twice, once going out and once coming back. */
    int32_t  baro_rate_ref_pa;
    uint32_t baro_rate_ref_ms;
    bool     baro_rate_have_ref;

    int16_t  imu_last_accel_mg[3];
    int16_t  imu_last_gyro_cdps[3];
    bool     imu_have_sample;
    uint32_t imu_repeat;
    uint32_t imu_fresh_ms;
    uint32_t imu_fault_samples;

    uint32_t samples;
} oa_health_t;

/* ---------------------------------------------------------------------------
 * Output.
 * ------------------------------------------------------------------------ */

typedef struct {
    /* OA_FLAG_BARO_FAULT and OA_FLAG_IMU_FAULT, or zero. The caller ORs this
     * into the flags byte it is already assembling; nothing here knows about the
     * other six bits. */
    uint8_t flags;

    /* Which individual checks fired, as OA_HEALTH_CHECK_* bits. The flags byte
     * says a sensor is faulted; this says why, which is what a console message
     * and a test failure both need. */
    uint32_t faults;

    /* Which checks were not performed because their threshold is unset. Never
     * confuse an empty `faults` with a healthy payload without reading this. */
    uint32_t checks_unset;

    /* Feeds oa_state_input_t.baro_valid and the fusion filter's baro_valid.
     * True when a reading has been seen and the corresponding fault flag is
     * clear. False before the first successful read, because "no measurement
     * yet" is not the same thing as "measured and fine". */
    bool baro_valid;
    bool imu_valid;
} oa_health_output_t;

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

/* Start the staleness clocks at now_ms with no sample seen yet. */
oa_result_t oa_health_init(oa_health_t *health, uint32_t now_ms);

/* Assess one sample.
 *
 * Pure with respect to the input: it reads `in`, updates `health`, and fills
 * `out`. It never writes through any other pointer and never suppresses a field.
 *
 * `cfg` supplies every threshold these checks use: the barometer's plausible
 * band and maximum rate of change, and the six stuck, stale and magnitude
 * limits that used to sit in a table of their own. Read only. Returns OA_OK or OA_ERR_NULL. It does not return an error for a missing
 * threshold: a check it cannot perform is reported in out->checks_unset, and
 * failing the whole sample loop over an unmeasured number would take a payload
 * off the air for a reason the operator can already see. */
oa_result_t oa_health_step(oa_health_t              *health,
                           const oa_config_t        *cfg,
                           const oa_health_input_t  *in,
                           oa_health_output_t       *out);

/* Samples on which each fault flag was raised, since init. Reported in the
 * flight summary so that a flag which appeared for two samples at transonic can
 * be told apart from one that was set for the whole flight. */
uint32_t oa_health_baro_fault_samples(const oa_health_t *health);
uint32_t oa_health_imu_fault_samples(const oa_health_t *health);
uint32_t oa_health_samples(const oa_health_t *health);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_HEALTH_H */

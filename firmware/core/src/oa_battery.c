/*
 * oApogee core: the battery byte.
 *
 * battery_volts = 2.5 + batt / 100, from docs/spec/telemetry-packet.md. The
 * 2500 mV floor and the 10 mV step are the format and live in oa_battery.h. The
 * threshold that raises LOW_BATT is not the format: it depends on the measured
 * cell discharge curve and the measured sag under radio transmit, neither of
 * which has been measured, so it comes from oa_config_t and is reported as
 * unset rather than guessed.
 *
 * Integers throughout. There is no floating point here, and not because it
 * would be slow: the battery byte is assembled in the same pass as the rest of
 * a packet, on a part whose floating point behaviour nobody has characterised,
 * and a scale factor that is exact in integers has no reason to become
 * approximate on the way to the wire.
 *
 * Nothing in this file has run on hardware, and no cell has been measured.
 */

#include "oapogee/oa_battery.h"

uint8_t oa_battery_encode_mv(int32_t millivolts)
{
    int32_t above_floor;
    int32_t steps;

    /* Clamp rather than fail. A packet that is not sent carries no information
     * at all, and a cell reading below 2.5 V is a state this encoding
     * deliberately does not describe finely, because a protected cell should
     * already have disconnected. */
    if (millivolts <= OA_BATTERY_MIN_MV) {
        return 0u;
    }
    if (millivolts >= OA_BATTERY_MAX_MV) {
        return 255u;
    }

    above_floor = millivolts - OA_BATTERY_OFFSET_MV; /* 1 to 2549 inclusive */

    /* Round to nearest, so a decoded value is never more than half a step from
     * the reading and the error is centred rather than always pessimistic.
     *
     * above_floor is strictly positive here, which is what makes adding half a
     * step and truncating the same thing as rounding. The identical expression
     * on a negative value would round away from nearest, and that is why the
     * low clamp above is a clamp and not a saturation applied afterwards. */
    steps = (above_floor + (OA_BATTERY_STEP_MV / 2)) / OA_BATTERY_STEP_MV;

    /* No upper clamp here, and its absence is deliberate. The largest input
     * that reaches this line is 5049 mV, giving 2549 + 5 = 2554, which is 255
     * steps, so rounding cannot carry past the top of the byte. A clamp that
     * can never fire reads to a later reader as though it can, and invites them
     * to reason about a case that does not exist. */
    return (uint8_t)steps;
}

int32_t oa_battery_decode_mv(uint8_t batt)
{
    return OA_BATTERY_OFFSET_MV + ((int32_t)batt * OA_BATTERY_STEP_MV);
}

bool oa_battery_is_low(const oa_config_t *cfg, int32_t millivolts, bool *out_unset)
{
    bool unset = true;
    bool low   = false;

    /* The set bit is the authority on whether the threshold has a value, and
     * the sentinel is the second line of defence behind it. Both are checked
     * because this is the one place in the firmware where a value that leaked
     * past the set bit would produce a flag that looks measured: OA_UNSET is
     * INT32_MIN, so a threshold read without the set bit check would compare as
     * "never low" and LOW_BATT would simply never appear on any flight.
     *
     * A NULL configuration is the same answer as an unset field: the threshold
     * is not known. It is not a crash and it is not "battery fine". */
    if (cfg != NULL && oa_config_is_set(cfg, OA_CFG_BATT_LOW_MV) && OA_IS_SET(cfg->batt_low_mv)) {
        unset = false;

        /* Strictly below. A reading exactly at the threshold is not below it,
         * and the threshold is the value someone will have measured as the
         * lowest acceptable, not the highest unacceptable. */
        low = (millivolts < cfg->batt_low_mv);
    }

    /* The header tells the caller this pointer may not be NULL, and a caller
     * that ignores that would be treating an unmeasured threshold as a healthy
     * cell. There is no result code to return here, so the pointer is checked
     * rather than dereferenced: a firmware that hard faults while reporting a
     * battery reading has turned a missing configuration value into a lost
     * flight. */
    if (out_unset != NULL) {
        *out_unset = unset;
    }

    return low;
}

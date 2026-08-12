/*
 * oApogee battery byte encoding.
 *
 * One byte in every STATUS and FLIGHT packet: battery_volts = 2.5 + batt / 100.
 * Range 2.50 V to 5.05 V in 10 mV steps.
 *
 * The 2.5 V floor and the 10 mV step are the wire format, from
 * docs/spec/telemetry-packet.md, so they are constants here rather than
 * configuration. The floor is 2.5 V because a single cell below that is already
 * past the point its protection circuit should have disconnected, so finer
 * resolution down there would describe a state that should not occur. The
 * ceiling covers USB present.
 *
 * The threshold that raises OA_FLAG_LOW_BATT is not the format. It depends on
 * the measured cell discharge curve and the measured sag under radio transmit,
 * neither of which has been measured, so it lives in oa_config_t as unset and
 * oa_battery_is_low reports that rather than guessing.
 *
 * Millivolts everywhere in the interface, integers throughout. The encode and
 * decode functions are exact inverses on every representable value and the
 * conformance test checks all 256 of them, which is cheap and rules out the
 * whole class of off-by-one scaling bugs.
 *
 * Nothing in this file has run on hardware, and no cell has been measured.
 */

#ifndef OAPOGEE_OA_BATTERY_H
#define OAPOGEE_OA_BATTERY_H

#include "oapogee/oa_config.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OA_BATTERY_OFFSET_MV (2500)
#define OA_BATTERY_STEP_MV   (10)
#define OA_BATTERY_MIN_MV    (OA_BATTERY_OFFSET_MV)                        /* 2500 */
#define OA_BATTERY_MAX_MV    (OA_BATTERY_OFFSET_MV + 255 * OA_BATTERY_STEP_MV) /* 5050 */

/* Millivolts to the transmitted byte.
 *
 * Below OA_BATTERY_MIN_MV encodes as 0 and above OA_BATTERY_MAX_MV encodes as
 * 255. Clamping rather than failing, because a packet that is not sent carries
 * no information at all, and a cell reading below 2.5 V is a state the encoding
 * deliberately does not describe finely.
 *
 * Rounds to nearest rather than truncating, so that a decoded value is never
 * more than 5 mV from the reading and the error is centred rather than always
 * pessimistic. */
uint8_t oa_battery_encode_mv(int32_t millivolts);

/* The transmitted byte back to millivolts. Exact. */
int32_t oa_battery_decode_mv(uint8_t batt);

/* Whether this reading is below the configured low threshold, which is what
 * raises OA_FLAG_LOW_BATT.
 *
 * *out_unset is set true when batt_low_mv is not configured, and the function
 * returns false. A caller must check it: an unconfigured threshold reported as
 * "not low" would look identical to a healthy cell, and the flag would simply
 * never appear on any flight. */
bool oa_battery_is_low(const oa_config_t *cfg, int32_t millivolts, bool *out_unset);

/* TODO(confirm-on-hardware): the conversion from the divider's ADC counts to
 * millivolts belongs in the port layer, not here, because it depends on the
 * divider ratio, the reference voltage and the ADC's measured nonlinearity. None
 * of those are known: no board has been fabricated. Nothing in core assumes any
 * of them. */

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_BATTERY_H */

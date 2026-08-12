/*
 * oApogee status flags.
 *
 * Eight bits, defined once. The same byte appears at offset 1 of every downlink
 * packet and at offset 35 of every flight.bin record, and the specs say so in
 * two places precisely because it is one thing. There is one definition here for
 * the same reason: two definitions of a bit field drift, and the drift shows up
 * as a flight log whose fault flags disagree with the telemetry that was heard
 * live, which is the kind of contradiction that discredits the whole dataset.
 *
 * Source of truth: docs/spec/telemetry-packet.md, the flags table.
 *
 * Nothing here has run on hardware.
 */

#ifndef OAPOGEE_OA_FLAGS_H
#define OAPOGEE_OA_FLAGS_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * The table.
 *
 * X(SYM, bit, mask, description)
 *
 * Expanded below into the enumerators, and expanded again by the conformance
 * test to check every mask against 1u << bit and to print the names. The
 * description is here so that a serial console or a test failure can say what a
 * bit means without a second table to keep in step.
 * ------------------------------------------------------------------------ */

#define OA_FLAG_LIST(X)                                                                    \
    X(GNSS_FIX,   0, 0x01u, "The GNSS receiver has a position fix.")                       \
    X(HIGH_G,     1, 0x02u, "The high-g accelerometer is present and healthy.")            \
    X(BARO_FAULT, 2, 0x04u, "The barometer has failed or is returning implausible values.")\
    X(IMU_FAULT,  3, 0x08u, "The IMU has failed or is returning implausible values.")      \
    X(LOG_FULL,   4, 0x10u, "Onboard storage is full. Logging has stopped.")               \
    X(LOW_BATT,   5, 0x20u, "Battery below the low threshold.")                            \
    X(SIM,        6, 0x40u, "Produced by a simulation or bench test, not a flight.")       \
    X(RESERVED7,  7, 0x80u, "Reserved. Transmitted as 0 and ignored on receive.")

#define OA_FLAG_ENUMERATOR(SYM, bit, mask, desc) OA_FLAG_##SYM = (mask),

typedef enum {
    OA_FLAG_NONE = 0x00u,
    OA_FLAG_LIST(OA_FLAG_ENUMERATOR)
    OA_FLAG_ALL = 0xFFu
} oa_flag_t;

#undef OA_FLAG_ENUMERATOR

/* Bit 7 is reserved and must be transmitted as 0. This mask is what enforces
 * that on the way out, rather than trusting every call site to have remembered.
 * When bit 7 is one day assigned, this constant changes here and nowhere else. */
#define OA_FLAG_TRANSMIT_MASK (0x7Fu)

/* Number of defined bits, including the reserved one. */
#define OA_FLAG_COUNT (8)

/* ---------------------------------------------------------------------------
 * Operations.
 *
 * The flags byte is built by the caller and carried through unchanged. These
 * helpers exist so that the reserved bit is cleared in exactly one place and so
 * that a test can round-trip a name.
 * ------------------------------------------------------------------------ */

/* Clear every bit that must not go on the air. Applied by the packet builders,
 * so a caller cannot transmit a reserved bit by accident. */
uint8_t oa_flags_sanitise(uint8_t flags);

/* The short name of a single flag bit, for the serial console and for test
 * failure messages. Returns NULL if `flag` is not exactly one defined bit,
 * rather than a placeholder string, so that a caller cannot print something
 * that looks like a flag name for a value that is not a flag. */
const char *oa_flag_name(oa_flag_t flag);

/* The one line description from the table above. NULL under the same rule. */
const char *oa_flag_description(oa_flag_t flag);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_FLAGS_H */

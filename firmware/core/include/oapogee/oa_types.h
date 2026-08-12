/*
 * oApogee core types.
 *
 * Everything in core/ compiles against stdint.h, stdbool.h, stddef.h and
 * string.h and nothing else. No SDK, no libc beyond those, no allocation, no
 * global mutable state. That is what makes core testable on a laptop today,
 * with no hardware in the room.
 *
 * Nothing in this file has run on hardware.
 */

#ifndef OAPOGEE_OA_TYPES_H
#define OAPOGEE_OA_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Wire type aliases.
 *
 * These exist so the X-macro tables can carry a bare token. A table line says
 * `i16` and a consumer pastes it into oa_##wire##_t to get a C type, into
 * oa_put_##wire to get the store, and stringises it for the layout dump and the
 * manifest. Without the aliases the tables would have to carry the type and the
 * name of the type as two separate columns that could disagree.
 *
 * There is deliberately no oa_i8_t or 64 bit alias: neither appears in either
 * spec, and adding an alias for a width the format does not use would invite
 * someone to use it.
 * ------------------------------------------------------------------------ */

typedef uint8_t  oa_u8_t;
typedef uint16_t oa_u16_t;
typedef uint32_t oa_u32_t;
typedef int16_t  oa_i16_t;
typedef int32_t  oa_i32_t;

/* ---------------------------------------------------------------------------
 * Tunables and the unset sentinel.
 *
 * Every flight threshold, interval, rate and window in this firmware is
 * unmeasured. Not "defaulted pending measurement": unmeasured. There is no
 * hardware, nothing has flown, and a plausible number chosen from intuition and
 * shipped as a default is the exact failure this project exists to avoid.
 *
 * So every tunable has one type and one sentinel. A single signed 32 bit
 * quantity carries every threshold the firmware needs, in the integer units the
 * specs already use (cm, cg, dm/s, ms, Pa, samples, Hz), and OA_UNSET is a value
 * no real threshold can take. It is INT32_MIN rather than 0 or -1 because both
 * of those are legitimate values for some of these fields, and a sentinel that
 * collides with a real reading is worse than no sentinel.
 *
 * The sentinel is the second line of defence. The first is the per-field set
 * bit in oa_config_t: a field is unset because nobody set it, not because it
 * happens to hold a magic number. Both exist because a value that leaks past
 * the set-bit check should be obviously wrong on a plot rather than quietly
 * plausible.
 * ------------------------------------------------------------------------ */

typedef int32_t oa_tunable_t;

#define OA_UNSET (INT32_MIN)

/* True when a tunable holds a measured value. Written as a macro rather than a
 * function because it is used in static initialiser checks and in the config
 * validator, and neither wants a call. */
#define OA_IS_SET(v) ((v) != OA_UNSET)

/* ---------------------------------------------------------------------------
 * Results.
 *
 * One enum for every fallible entry point in core. There is no errno, no global
 * error slot, and no code that reports failure by returning a value that is
 * also a legal result.
 *
 * The values are stable so that a fault can be reported over the serial console
 * or blinked out on the status LED without the meaning shifting between builds.
 * ------------------------------------------------------------------------ */

typedef enum {
    OA_OK = 0,

    /* A required pointer argument was NULL. */
    OA_ERR_NULL = 1,

    /* An argument was outside the range the function accepts. This is for
     * programming errors, not for sensor readings: an out of range sensor value
     * is clamped and flagged by the encoder, never rejected, because a packet
     * that is not sent carries no information at all. */
    OA_ERR_RANGE = 2,

    /* The caller's output buffer is smaller than the fixed length of the thing
     * being written. Buffers are caller-owned everywhere in core, so this is the
     * only way a build function can fail on space. */
    OA_ERR_BUFFER = 3,

    /* A configuration field required for the requested operation is unset.
     * Returned by oa_config_is_flightworthy and by anything that would
     * otherwise have to invent the missing number. */
    OA_ERR_UNSET = 4,

    /* The sink refused the write: out of space, or the underlying storage
     * reported a failure. Distinguished from OA_ERR_BUFFER because one is the
     * caller's mistake and the other is the flash filling up mid-flight, which
     * is a normal thing that sets LOG_FULL and keeps flying. */
    OA_ERR_SINK_FULL = 5,
    OA_ERR_SINK_IO   = 6,

    /* The operation is not valid in the current state. Used by the state
     * machine and the scheduler, never as a general purpose failure. */
    OA_ERR_STATE = 7,

    /* Nothing to return. Used by ring buffer reads and by the scheduler when no
     * packet is due. Not an error in the sense of something going wrong. */
    OA_ERR_EMPTY = 8,

    /* The requested thing exists in the format but is not implemented by this
     * build. Distinct from OA_ERR_RANGE so that a Solo build asking for a
     * POSITION packet reports the truth rather than looking like a bug. */
    OA_ERR_UNSUPPORTED = 9
} oa_result_t;

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_TYPES_H */

/*
 * oApogee configuration file parser.
 *
 * WHY THIS HEADER EXISTS AT ALL
 *
 * The contract commit shipped oa_config_parse.c with no header. This file was
 * added with the implementation so the parser has a declared interface like
 * every other module in core. Nothing in oa_config.h was changed to make it fit.
 *
 * oa_config.h says parsing the configuration file is the application's job
 * rather than core's, on the grounds that core defines the fields and validates
 * them. This parser is still in core, and the reason is the same reason the
 * validator is: it is pure text in, integers out, it touches no filesystem and
 * no peripheral, and being in core is what makes it testable on a laptop against
 * every malformed file anyone can think of. The application reads the bytes off
 * the flash and hands them here.
 *
 * WHAT THE PARSER WILL NOT DO
 *
 * It will not supply a value for a key that has none. A key written with no
 * value stays unset, because the alternative is a default, and a default is an
 * invented number that would fly. It will not guess at a misspelled key either:
 * an unknown key is an error that names the line, since a typo in a threshold
 * name that was quietly ignored would leave that threshold unset and the
 * operator would find out at the rail.
 *
 * It also does not judge whether a value is sensible. It has no idea what a
 * plausible sample rate is, and neither does anyone else yet. Range checking a
 * tunable against a band nobody has measured would be inventing two numbers
 * instead of one.
 *
 * Nothing in this file has run on hardware, and no configuration file has ever
 * been read off a board.
 */

#ifndef OAPOGEE_OA_CONFIG_PARSE_H
#define OAPOGEE_OA_CONFIG_PARSE_H

#include "oapogee/oa_config.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * THE FILE FORMAT
 *
 * One key per line. Nothing else.
 *
 *     # everything after a hash is a comment, anywhere on the line
 *     sample_rate_hz = 200
 *     apogee_confirm_samples =        <- present, deliberately not set
 *     prelaunch_ring_ms               <- the same thing, written shorter
 *     simulated = true
 *     callsign = N0CALL
 *
 * Rules, all of them:
 *
 *   - A line is trimmed of leading and trailing spaces and tabs. CRLF and LF
 *     both end a line, so a file edited on Windows over USB parses the same as
 *     one edited on a laptop.
 *   - A hash starts a comment that runs to the end of the line. A value can
 *     therefore never contain a hash, which costs nothing: every value here is
 *     a decimal integer, a boolean, or a callsign.
 *   - A blank line, or a line that is only a comment, is skipped.
 *   - Otherwise the line is a key, optionally followed by '=' and a value.
 *   - A key with no value leaves its field unset. This is not an error and it is
 *     the intended way to write a configuration file: list every key, fill in
 *     the ones that have been measured, and leave the rest visibly blank. The
 *     payload will refuse to arm and will name exactly what is missing.
 *   - A key that appears twice is an error. Last-one-wins is the kind of quiet
 *     behaviour that produces a flight tuned by whichever line the operator
 *     forgot to delete.
 *   - Values are decimal integers with an optional leading + or -. No hex, no
 *     underscores, no floating point. Every tunable in this firmware is an
 *     integer in a unit the specs already use, so there is nothing to widen the
 *     grammar for.
 *
 * Two keys are not tunables and are handled separately, because they are members
 * of oa_config_t that sit outside OA_CONFIG_FIELDS:
 *
 *   simulated = true | false | 1 | 0
 *   callsign  = up to 15 characters, no spaces
 *
 * `simulated` is the bit that stops a bench run being published as a flight, so
 * it has to be settable from the file that configures the bench run.
 * ------------------------------------------------------------------------ */

/* Longest key the parser will copy out of a line before looking it up. A key
 * longer than this cannot match any field name in OA_CONFIG_FIELDS, so it is
 * reported as unknown, which is the same answer by a shorter route. This is a
 * buffer size, not a tunable: no measurement would change it. */
#define OA_CONFIG_PARSE_KEY_MAX 64

/* How much of an offending line the report echoes back, plus its terminator.
 * Sized for a console line. Also a buffer size, also not a tunable. */
#define OA_CONFIG_PARSE_ECHO_MAX 96

/* Longest callsign the configuration can hold, from oa_config_t.callsign. */
#define OA_CONFIG_PARSE_CALLSIGN_MAX 15

typedef enum {
    OA_PARSE_OK = 0,

    /* The key is not in OA_CONFIG_FIELDS and is not one of the two non-tunable
     * keys. Reported rather than ignored, because ignoring it leaves the field
     * the operator thought they had set still unset. */
    OA_PARSE_ERR_UNKNOWN_KEY = 1,

    /* The line begins with '=' or is otherwise a value with no key. */
    OA_PARSE_ERR_NO_KEY = 2,

    /* Something follows the key that is not '='. */
    OA_PARSE_ERR_SEPARATOR = 3,

    /* The value is not a decimal integer, not a boolean where one was required,
     * or contains a character a callsign cannot contain. */
    OA_PARSE_ERR_VALUE_SYNTAX = 4,

    /* The value is a well-formed integer that does not fit in oa_tunable_t. */
    OA_PARSE_ERR_VALUE_RANGE = 5,

    /* The value is exactly OA_UNSET. It is a legal int32 and it is the sentinel
     * that means no measurement, so it cannot also mean a measurement. Its own
     * error rather than a range error, because the operator who typed it
     * deserves to be told which of those two things went wrong. */
    OA_PARSE_ERR_VALUE_SENTINEL = 6,

    /* The value is longer than the field can hold. Only a callsign can do this.
     * Truncating instead would transmit a station identification that is not the
     * operator's, under a legal regime where identification is the requirement. */
    OA_PARSE_ERR_VALUE_TOO_LONG = 7,

    /* The same key appears on two lines. */
    OA_PARSE_ERR_DUPLICATE_KEY = 8
} oa_parse_error_t;

/* What went wrong and where.
 *
 * A line number and the line itself, because the useful thing to print at an
 * operator who is standing at a laptop with a text editor open is the line they
 * have to fix. Fixed buffers: core allocates nothing. */
typedef struct {
    /* 1-based. Zero when no line is implicated, which is every success and every
     * NULL-argument rejection. */
    size_t line;

    oa_parse_error_t error;

    /* The offending line, trimmed and with its comment removed, truncated to fit
     * and always NUL terminated. Empty on success. */
    char text[OA_CONFIG_PARSE_ECHO_MAX];

    /* Lines that carried a key, whether or not that key carried a value. */
    size_t keys_seen;

    /* Keys that carried a value and therefore set something. The difference
     * between this and keys_seen is how many fields the operator has written
     * down but not yet measured, which is a number worth printing. */
    size_t keys_set;
} oa_config_parse_report_t;

/* Zero a report. Safe to call on a report that is about to be filled in anyway;
 * it exists so a caller can print a report it never passed to the parser without
 * reading uninitialised memory. */
void oa_config_parse_report_init(oa_config_parse_report_t *report);

/* Parse `len` bytes of configuration text into `cfg`.
 *
 * Total over the input. Any byte sequence of any length, including one with
 * embedded NULs, no trailing newline, or no valid line at all, produces a
 * defined result and reads nothing outside [text, text + len). The text does not
 * have to be NUL terminated and is never scanned past `len`.
 *
 * ALL OR NOTHING. The text is validated completely before anything is written,
 * so a file with an error on its last line leaves `cfg` exactly as it was. On
 * success `cfg` is initialised with oa_config_init and then filled in, so every
 * key absent from the file, and every key present with no value, is unset rather
 * than left over from a previous parse.
 *
 * Returns OA_OK, OA_ERR_NULL if text or cfg is NULL, or OA_ERR_RANGE when the
 * text does not parse, in which case `report` names the line. `report` may be
 * NULL, in which case the failure is reported only as a result code. */
oa_result_t oa_config_parse(const char *text,
                            size_t len,
                            oa_config_t *cfg,
                            oa_config_parse_report_t *report);

/* One line of English for an error code, for the console. Never NULL: an
 * unrecognised code returns a string saying so rather than a null pointer that
 * a print path would have to special-case. */
const char *oa_config_parse_error_text(oa_parse_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_CONFIG_PARSE_H */

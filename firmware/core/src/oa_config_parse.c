/*
 * oApogee core: the configuration file parser.
 *
 * Implements oapogee/oa_config_parse.h, which carries the grammar. This file
 * carries the reasons.
 *
 * THERE ARE NO TUNABLES IN HERE, AND THERE IS NO DEFAULTING. The parser sets
 * what the file says and nothing else. A key present with no value leaves its
 * field unset, a key absent from the file leaves its field unset, and unset is
 * what makes oa_config_is_flightworthy refuse to arm. A parser that filled in a
 * plausible number for a missing threshold would defeat the only mechanism this
 * firmware has for admitting that nothing has been measured.
 *
 * TOTAL OVER THE INPUT. This is the one function in core that reads bytes a
 * human typed, so it is the one place where the input is genuinely arbitrary. It
 * never reads outside [text, text + len), never requires a terminator, never
 * calls anything from libc beyond memcpy and memcmp, and has no failure mode
 * other than returning an error that names a line.
 *
 * NO strlen, strcmp, strtol OR ctype. core may reference exactly memcpy, memset,
 * memmove and memcmp, per core/allowed-undefined.txt, and that list is one of
 * the two artifacts that fence core off from the platform. The three helpers
 * below are what that costs, and it is a small cost.
 *
 * Nothing in this file has run on hardware, and no configuration file has ever
 * been read off a board.
 */

#include "oapogee/oa_config_parse.h"

#include <string.h>

/* The sentinel check in oa_parse_integer is written against INT32_MAX rather
 * than a literal, and this is what ties the two together. If OA_UNSET ever stops
 * being INT32_MIN, this file stops compiling instead of silently accepting the
 * new sentinel as a measurement. */
_Static_assert(OA_UNSET == INT32_MIN, "the parser assumes OA_UNSET is INT32_MIN");

_Static_assert(OA_CONFIG_PARSE_CALLSIGN_MAX + 1u <= sizeof(((oa_config_t *)0)->callsign),
               "callsign buffer in oa_config_t is smaller than the parser allows");

/* ---------------------------------------------------------------------------
 * Text spans.
 *
 * A span is a slice of the caller's buffer. Nothing is copied out of the input
 * except the key, which has to be NUL terminated to be looked up, and the echo
 * in the report. That is what lets the parser handle a line of any length: only
 * the diagnostic is bounded, not the input.
 * ------------------------------------------------------------------------ */

typedef struct {
    const char *p;
    size_t      n;
} oa_span_t;

static bool oa_is_space(char c)
{
    return c == ' ' || c == '\t';
}

static bool oa_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* Printable ASCII excluding space. A key or a callsign made of anything else is
 * rejected rather than passed on. The reason is specific: without this check a
 * key containing an embedded NUL, which a corrupted flash read can produce,
 * would be copied into a NUL terminated buffer and would match the field whose
 * name is its prefix. That is a threshold set by a corrupted file, silently. */
static bool oa_is_visible(char c)
{
    return c > 0x20 && c < 0x7F;
}

static oa_span_t oa_span_trim(oa_span_t s)
{
    while (s.n > 0u && oa_is_space(s.p[0])) {
        s.p++;
        s.n--;
    }
    while (s.n > 0u && oa_is_space(s.p[s.n - 1u])) {
        s.n--;
    }
    return s;
}

/* Everything from the first hash to the end of the line is a comment. Applied
 * before anything else, so a full line comment is just a line that trims to
 * nothing, and there is one rule instead of two. */
static oa_span_t oa_span_cut_comment(oa_span_t s)
{
    size_t i;

    for (i = 0u; i < s.n; i++) {
        if (s.p[i] == '#') {
            s.n = i;
            break;
        }
    }
    return s;
}

/* ---------------------------------------------------------------------------
 * Small string helpers.
 * ------------------------------------------------------------------------ */

/* Compare a NUL terminated key against a NUL terminated literal. Hand written
 * because strcmp is not on core/allowed-undefined.txt and this is a four line
 * function. */
static bool oa_key_equals(const char *key, const char *literal)
{
    size_t i = 0u;

    while (key[i] != '\0' && literal[i] != '\0') {
        if (key[i] != literal[i]) {
            return false;
        }
        i++;
    }
    return key[i] == literal[i];
}

static bool oa_span_equals(oa_span_t s, const char *literal)
{
    size_t i = 0u;

    while (i < s.n && literal[i] != '\0') {
        if (s.p[i] != literal[i]) {
            return false;
        }
        i++;
    }
    return i == s.n && literal[i] == '\0';
}

/* ---------------------------------------------------------------------------
 * Parse state.
 *
 * The duplicate-key bitmap. One bit per field plus one for each of the two
 * members of oa_config_t that are not tunables and therefore have no index in
 * OA_CONFIG_FIELDS.
 * ------------------------------------------------------------------------ */

typedef struct {
    uint8_t seen[OA_CONFIG_SET_BYTES];
    bool    seen_simulated;
    bool    seen_callsign;
} oa_parse_seen_t;

static bool oa_seen_test(const oa_parse_seen_t *s, oa_config_field_t field)
{
    const size_t idx = (size_t)field;

    return (s->seen[idx / 8u] & (uint8_t)(1u << (idx % 8u))) != 0u;
}

static void oa_seen_mark(oa_parse_seen_t *s, oa_config_field_t field)
{
    const size_t idx = (size_t)field;

    s->seen[idx / 8u] = (uint8_t)(s->seen[idx / 8u] | (uint8_t)(1u << (idx % 8u)));
}

/* ---------------------------------------------------------------------------
 * Value parsing.
 * ------------------------------------------------------------------------ */

/* Decimal integer with an optional sign, into an oa_tunable_t.
 *
 * Accumulated in uint32_t with an overflow test before every step, rather than
 * in a wider type, so no 64-bit arithmetic appears here at all. The division is
 * by the constant 10, which every compiler turns into a multiply, so it is not a
 * divide in the emitted code. */
static oa_parse_error_t oa_parse_integer(oa_span_t v, oa_tunable_t *out)
{
    const uint32_t positive_limit = (uint32_t)INT32_MAX;
    const uint32_t negative_limit = (uint32_t)INT32_MAX + 1u; /* magnitude of INT32_MIN */

    bool     negative = false;
    size_t   i        = 0u;
    uint32_t acc      = 0u;

    if (v.n == 0u) {
        return OA_PARSE_ERR_VALUE_SYNTAX;
    }
    if (v.p[0] == '+' || v.p[0] == '-') {
        negative = (v.p[0] == '-');
        i        = 1u;
    }
    if (i >= v.n) {
        /* A lone sign. */
        return OA_PARSE_ERR_VALUE_SYNTAX;
    }

    for (; i < v.n; i++) {
        uint32_t digit;

        if (!oa_is_digit(v.p[i])) {
            return OA_PARSE_ERR_VALUE_SYNTAX;
        }
        digit = (uint32_t)(v.p[i] - '0');

        if (acc > (UINT32_MAX - digit) / 10u) {
            return OA_PARSE_ERR_VALUE_RANGE;
        }
        acc = (acc * 10u) + digit;
    }

    if (acc > (negative ? negative_limit : positive_limit)) {
        return OA_PARSE_ERR_VALUE_RANGE;
    }
    if (negative && acc == negative_limit) {
        /* The operator wrote the unset sentinel as if it were a measurement.
         * Reported as its own error, because the fix is different: they need a
         * different number, not a smaller one. */
        return OA_PARSE_ERR_VALUE_SENTINEL;
    }

    *out = negative ? -(int32_t)acc : (int32_t)acc;
    return OA_PARSE_OK;
}

static oa_parse_error_t oa_parse_boolean(oa_span_t v, bool *out)
{
    if (oa_span_equals(v, "true") || oa_span_equals(v, "1")) {
        *out = true;
        return OA_PARSE_OK;
    }
    if (oa_span_equals(v, "false") || oa_span_equals(v, "0")) {
        *out = false;
        return OA_PARSE_OK;
    }
    return OA_PARSE_ERR_VALUE_SYNTAX;
}

static oa_parse_error_t oa_parse_callsign(oa_span_t v, char *out, size_t cap)
{
    size_t i;

    if (v.n + 1u > cap) {
        /* Refused rather than truncated. A truncated callsign is a station
         * identification that is not the operator's, transmitted under the one
         * legal regime where identifying is the requirement. */
        return OA_PARSE_ERR_VALUE_TOO_LONG;
    }
    for (i = 0u; i < v.n; i++) {
        if (!oa_is_visible(v.p[i])) {
            return OA_PARSE_ERR_VALUE_SYNTAX;
        }
    }

    memset(out, 0, cap);
    memcpy(out, v.p, v.n);
    return OA_PARSE_OK;
}

/* ---------------------------------------------------------------------------
 * Reporting.
 * ------------------------------------------------------------------------ */

/* Copy as much of the offending line into the report as fits.
 *
 * Bytes that are not printable are replaced rather than copied. The line came
 * from a file on a flash part that can return anything, and this string is
 * printed to a console: an embedded NUL would truncate the message at the
 * interesting part, and a control byte would do something to a terminal that
 * nobody intended. */
static void oa_report_echo(oa_config_parse_report_t *report, oa_span_t line)
{
    size_t n = line.n;
    size_t i;

    if (report == NULL) {
        return;
    }
    if (n > (OA_CONFIG_PARSE_ECHO_MAX - 1u)) {
        n = OA_CONFIG_PARSE_ECHO_MAX - 1u;
    }
    for (i = 0u; i < n; i++) {
        const char c = line.p[i];

        report->text[i] = (oa_is_visible(c) || c == ' ') ? c : '?';
    }
    report->text[n] = '\0';
}

static oa_result_t oa_report_fail(oa_config_parse_report_t *report,
                                  size_t                    line_no,
                                  oa_parse_error_t          error,
                                  oa_span_t                 line)
{
    if (report != NULL) {
        report->line  = line_no;
        report->error = error;
        oa_report_echo(report, line);
    }
    return OA_ERR_RANGE;
}

/* ---------------------------------------------------------------------------
 * One line.
 *
 * `cfg` NULL means the validating pass: everything is checked, nothing is
 * written. That is what makes the parse all or nothing, so a file with an error
 * on its last line does not leave a half-configured payload behind.
 * ------------------------------------------------------------------------ */

static oa_result_t oa_parse_line(oa_span_t                 line,
                                 size_t                    line_no,
                                 oa_config_t              *cfg,
                                 oa_parse_seen_t          *seen,
                                 oa_config_parse_report_t *report)
{
    char              key_buf[OA_CONFIG_PARSE_KEY_MAX];
    oa_span_t         key;
    oa_span_t         rest;
    oa_span_t         value;
    oa_config_field_t field;
    size_t            i;

    line = oa_span_trim(oa_span_cut_comment(line));
    if (line.n == 0u) {
        return OA_OK; /* Blank, or a comment. */
    }

    /* The key is everything up to the first space, tab or '='. */
    key.p = line.p;
    key.n = 0u;
    while (key.n < line.n && !oa_is_space(line.p[key.n]) && line.p[key.n] != '=') {
        key.n++;
    }
    if (key.n == 0u) {
        return oa_report_fail(report, line_no, OA_PARSE_ERR_NO_KEY, line);
    }
    for (i = 0u; i < key.n; i++) {
        if (!oa_is_visible(key.p[i])) {
            return oa_report_fail(report, line_no, OA_PARSE_ERR_UNKNOWN_KEY, line);
        }
    }

    rest.p = line.p + key.n;
    rest.n = line.n - key.n;
    rest   = oa_span_trim(rest);

    if (rest.n == 0u) {
        /* `key` on its own. */
        value.p = NULL;
        value.n = 0u;
    } else if (rest.p[0] == '=') {
        rest.p++;
        rest.n--;
        value = oa_span_trim(rest);
    } else {
        return oa_report_fail(report, line_no, OA_PARSE_ERR_SEPARATOR, line);
    }

    if (report != NULL) {
        report->keys_seen++;
    }

    /* --- the two members that are not tunables --------------------------- */

    if (key.n < sizeof key_buf) {
        memcpy(key_buf, key.p, key.n);
        key_buf[key.n] = '\0';
    } else {
        /* Longer than any field name, so it cannot be one. Reported as unknown
         * by the same route a misspelling is. */
        return oa_report_fail(report, line_no, OA_PARSE_ERR_UNKNOWN_KEY, line);
    }

    if (oa_key_equals(key_buf, "simulated")) {
        bool simulated = false;

        if (seen->seen_simulated) {
            return oa_report_fail(report, line_no, OA_PARSE_ERR_DUPLICATE_KEY, line);
        }
        seen->seen_simulated = true;

        if (value.n == 0u) {
            return OA_OK; /* Present with no value. Leaves the initialised false. */
        }
        {
            const oa_parse_error_t e = oa_parse_boolean(value, &simulated);

            if (e != OA_PARSE_OK) {
                return oa_report_fail(report, line_no, e, line);
            }
        }
        if (cfg != NULL) {
            cfg->simulated = simulated;
        }
        if (report != NULL) {
            report->keys_set++;
        }
        return OA_OK;
    }

    if (oa_key_equals(key_buf, "callsign")) {
        char callsign[OA_CONFIG_PARSE_CALLSIGN_MAX + 1u];

        if (seen->seen_callsign) {
            return oa_report_fail(report, line_no, OA_PARSE_ERR_DUPLICATE_KEY, line);
        }
        seen->seen_callsign = true;

        if (value.n == 0u) {
            return OA_OK; /* Empty is a valid state for a callsign. */
        }
        {
            const oa_parse_error_t e = oa_parse_callsign(value, callsign, sizeof callsign);

            if (e != OA_PARSE_OK) {
                return oa_report_fail(report, line_no, e, line);
            }
        }
        if (cfg != NULL) {
            /* oa_parse_callsign zeroed the whole of its output buffer before
             * copying, so the terminator and the padding come across with it. */
            memcpy(cfg->callsign, callsign, sizeof callsign);
        }
        if (report != NULL) {
            report->keys_set++;
        }
        return OA_OK;
    }

    /* --- a tunable -------------------------------------------------------- */

    if (oa_config_field_by_name(key_buf, &field) != OA_OK) {
        return oa_report_fail(report, line_no, OA_PARSE_ERR_UNKNOWN_KEY, line);
    }
    if (oa_seen_test(seen, field)) {
        return oa_report_fail(report, line_no, OA_PARSE_ERR_DUPLICATE_KEY, line);
    }
    oa_seen_mark(seen, field);

    if (value.n == 0u) {
        /* The whole point. A key with no value stays unset, because the only
         * other thing to do with it is invent a number. */
        return OA_OK;
    }

    {
        oa_tunable_t           parsed = OA_UNSET;
        const oa_parse_error_t e      = oa_parse_integer(value, &parsed);

        if (e != OA_PARSE_OK) {
            return oa_report_fail(report, line_no, e, line);
        }
        if (cfg != NULL) {
            const oa_result_t r = oa_config_set(cfg, field, parsed);

            if (r != OA_OK) {
                /* Unreachable as written: the only value oa_config_set rejects
                 * is OA_UNSET, and oa_parse_integer rejects it first. It is
                 * checked anyway because "unreachable" is a claim about two
                 * functions in different files. */
                return oa_report_fail(report, line_no, OA_PARSE_ERR_VALUE_RANGE, line);
            }
        }
    }

    if (report != NULL) {
        report->keys_set++;
    }
    return OA_OK;
}

/* ---------------------------------------------------------------------------
 * One pass over the text.
 * ------------------------------------------------------------------------ */

static oa_result_t oa_parse_pass(const char               *text,
                                 size_t                    len,
                                 oa_config_t              *cfg,
                                 oa_config_parse_report_t *report)
{
    oa_parse_seen_t seen;
    size_t          pos     = 0u;
    size_t          line_no = 0u;

    memset(&seen, 0, sizeof seen);

    if (report != NULL) {
        report->keys_seen = 0u;
        report->keys_set  = 0u;
    }

    while (pos < len) {
        oa_span_t line;
        size_t    end = pos;

        while (end < len && text[end] != '\n') {
            end++;
        }

        line.p = text + pos;
        line.n = end - pos;

        /* A file written on Windows and copied over USB ends its lines with CRLF,
         * and a stray carriage return inside a value would otherwise make every
         * line of that file a syntax error. */
        if (line.n > 0u && line.p[line.n - 1u] == '\r') {
            line.n--;
        }

        line_no++;
        {
            const oa_result_t r = oa_parse_line(line, line_no, cfg, &seen, report);

            if (r != OA_OK) {
                return r;
            }
        }

        pos = (end < len) ? (end + 1u) : len;
    }

    return OA_OK;
}

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

void oa_config_parse_report_init(oa_config_parse_report_t *report)
{
    if (report == NULL) {
        return;
    }
    memset(report, 0, sizeof *report);
    report->error = OA_PARSE_OK;
}

oa_result_t oa_config_parse(const char               *text,
                            size_t                    len,
                            oa_config_t              *cfg,
                            oa_config_parse_report_t *report)
{
    oa_result_t r;

    if (cfg == NULL || (text == NULL && len > 0u)) {
        return OA_ERR_NULL;
    }

    oa_config_parse_report_init(report);

    /* Pass one validates and writes nothing. A file whose last line is a typo
     * must not leave a payload holding half of it: the operator would fix the
     * typo, reload, and never know that the run before it had a different
     * configuration than the file described. */
    r = oa_parse_pass(text, len, NULL, report);
    if (r != OA_OK) {
        return r;
    }

    /* Pass two applies. oa_config_init first, so that a key absent from the file
     * is unset rather than left over from whatever was in this struct before. */
    oa_config_init(cfg);

    r = oa_parse_pass(text, len, cfg, report);
    if (r != OA_OK) {
        /* Cannot happen: the same text passed the same checks a moment ago. If
         * it somehow does, the configuration is discarded rather than flown. */
        oa_config_init(cfg);
        return r;
    }

    return OA_OK;
}

const char *oa_config_parse_error_text(oa_parse_error_t error)
{
    switch (error) {
    case OA_PARSE_OK:
        return "no error";
    case OA_PARSE_ERR_UNKNOWN_KEY:
        return "unknown key: this firmware defines no setting by that name";
    case OA_PARSE_ERR_NO_KEY:
        return "no key before the '='";
    case OA_PARSE_ERR_SEPARATOR:
        return "expected '=' after the key";
    case OA_PARSE_ERR_VALUE_SYNTAX:
        return "value is not a decimal integer";
    case OA_PARSE_ERR_VALUE_RANGE:
        return "value does not fit in a 32 bit signed integer";
    case OA_PARSE_ERR_VALUE_SENTINEL:
        return "-2147483648 is the unset sentinel and cannot be a measurement";
    case OA_PARSE_ERR_VALUE_TOO_LONG:
        return "value is longer than the field can hold";
    case OA_PARSE_ERR_DUPLICATE_KEY:
        return "key already set on an earlier line";
    default:
        break;
    }
    return "unrecognised parse error code";
}

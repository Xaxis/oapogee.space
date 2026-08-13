/*
 * Conformance tests for oa_config_parse.
 *
 * The grammar under test is the one written out in oa_config_parse.h, and the
 * rule that outranks the rest of it is the accuracy rule from CONTENT-STYLE.md:
 * never invent a number. For a parser that means a key with no value leaves its
 * field unset, and an unknown key is an error that names the line rather than a
 * line quietly ignored. Both of those are tested here, and both are the reason
 * this file is longer than the parser deserves on grammar alone.
 *
 * The other thing tested at length is that the function is total over its input.
 * This is the one function in core that reads bytes a human typed, and it has to
 * be defined on a file that is empty, has no trailing newline, was edited on
 * Windows, or came back from a flash part as garbage.
 *
 * No value in this file is a proposal for anything. Where a test needs an
 * integer it supplies one, the way a flight would supply one measured on real
 * hardware. No threshold ships in the firmware and none is suggested here.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware, and no
 * configuration file has ever been read off a board.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_config.h"
#include "oapogee/oa_config_parse.h"

/* Parse a NUL terminated C string, which is how every case below is written.
 * The parser itself is given the length and never sees the terminator, which is
 * the point: the text on a flash part is not terminated. */
static oa_result_t parse(const char *text, oa_config_t *cfg, oa_config_parse_report_t *report)
{
    return oa_config_parse(text, strlen(text), cfg, report);
}

static bool str_equals(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

/* CLAIM, from oa_config_parse.h: "A key with no value leaves its field unset.
 * This is not an error and it is the intended way to write a configuration file:
 * list every key, fill in the ones that have been measured, and leave the rest
 * visibly blank."
 *
 * Both spellings are checked, because the header offers both and an operator
 * will use whichever their editor left behind. This is the single most important
 * test in this file: a parser that defaulted here would put an invented number
 * into a flight. */
static void test_a_key_with_no_value_stays_unset(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char text[] = "apogee_confirm_samples =\n"
                               "prelaunch_ring_ms\n"
                               "sample_rate_hz = 200\n";

    assert(parse(text, &cfg, &report) == OA_OK);

    assert(oa_config_is_set(&cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES) == false);
    assert(oa_config_is_set(&cfg, OA_CFG_PRELAUNCH_RING_MS) == false);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == true);

    {
        oa_tunable_t v = 0;

        assert(oa_config_get(&cfg, OA_CFG_APOGEE_CONFIRM_SAMPLES, &v) == OA_OK);
        assert(v == OA_UNSET);

        assert(oa_config_get(&cfg, OA_CFG_SAMPLE_RATE_HZ, &v) == OA_OK);
        assert(v == 200);
    }

    /* CLAIM: "keys_seen: lines that carried a key, whether or not that key
     * carried a value" and "keys_set: keys that carried a value. The difference
     * between this and keys_seen is how many fields the operator has written
     * down but not yet measured, which is a number worth printing." */
    assert(report.keys_seen == 3u);
    assert(report.keys_set == 1u);
    assert(report.error == OA_PARSE_OK);
    assert(report.line == 0u);
    assert(report.text[0] == '\0');
}

/* CLAIM: "It will not guess at a misspelled key either: an unknown key is an
 * error that names the line, since a typo in a threshold name that was quietly
 * ignored would leave that threshold unset and the operator would find out at
 * the rail."
 *
 * The line number and the echoed line are both checked, because "names the line"
 * is the part that makes this useful rather than merely correct. */
static void test_an_unknown_key_is_an_error_that_names_the_line(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char text[] = "# a configuration file\n"
                               "sample_rate_hz = 200\n"
                               "\n"
                               "apogee_confrim_samples = 3   # transposed\n"
                               "batt_low_mv = 3600\n";

    assert(parse(text, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_UNKNOWN_KEY);
    assert(report.line == 4u);

    /* Echoed with the comment removed and the line trimmed, because what the
     * operator has to fix is the setting, not the note they wrote next to it. */
    assert(str_equals(report.text, "apogee_confrim_samples = 3"));

    /* A message a console can print without composing one. */
    assert(oa_config_parse_error_text(report.error) != NULL);
}

/* CLAIM: "ALL OR NOTHING. The text is validated completely before anything is
 * written, so a file with an error on its last line leaves cfg exactly as it
 * was."
 *
 * Checked by comparing the whole struct byte for byte, so that a field written
 * by the failed parse cannot hide behind an accessor nobody thought to call. */
static void test_a_failed_parse_writes_nothing(void)
{
    oa_config_t              before;
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char good[] = "sample_rate_hz = 200\n"
                               "batt_low_mv    = 3600\n"
                               "simulated      = true\n"
                               "callsign       = N0CALL\n";

    /* A second file that sets different values and then fails on its last line. */
    static const char bad[] = "sample_rate_hz = 100\n"
                              "batt_low_mv    = 3300\n"
                              "landing_hold_ms\n"
                              "not_a_setting  = 1\n";

    assert(parse(good, &cfg, &report) == OA_OK);
    memcpy(&before, &cfg, sizeof before);

    assert(parse(bad, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_UNKNOWN_KEY);
    assert(report.line == 4u);
    assert(memcmp(&before, &cfg, sizeof before) == 0);
}

/* CLAIM: "On success cfg is initialised with oa_config_init and then filled in,
 * so every key absent from the file, and every key present with no value, is
 * unset rather than left over from a previous parse."
 *
 * A payload that reloaded its configuration and kept a threshold from the
 * previous file would fly a number that is in no file anywhere. */
static void test_a_successful_parse_starts_from_unset(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char first[]  = "sample_rate_hz = 200\nbatt_low_mv = 3600\nsimulated = true\n"
                                 "callsign = N0CALL\n";
    static const char second[] = "batt_low_mv = 3300\n";

    assert(parse(first, &cfg, &report) == OA_OK);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == true);
    assert(cfg.simulated == true);
    assert(str_equals(cfg.callsign, "N0CALL"));

    assert(parse(second, &cfg, &report) == OA_OK);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == false);
    assert(oa_config_is_set(&cfg, OA_CFG_BATT_LOW_MV) == true);

    /* simulated and callsign are members of oa_config_t outside the field table,
     * and oa_config_init owns their starting state too: a bench run that set the
     * SIM bit must not leave it set for the flight that follows. */
    assert(cfg.simulated == false);
    assert(cfg.callsign[0] == '\0');
}

/* CLAIM: "A key that appears twice is an error. Last-one-wins is the kind of
 * quiet behaviour that produces a flight tuned by whichever line the operator
 * forgot to delete." */
static void test_a_duplicate_key_is_an_error(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char tunable[] = "sample_rate_hz = 200\n"
                                  "batt_low_mv = 3600\n"
                                  "sample_rate_hz = 100\n";

    /* Including the case where the second mention carries no value, which is
     * how a half-finished edit usually looks. */
    static const char blanked[] = "sample_rate_hz = 200\n"
                                  "sample_rate_hz\n";

    static const char sim[]  = "simulated = true\nsimulated = false\n";
    static const char call[] = "callsign = N0CALL\ncallsign = N0CALL\n";

    assert(parse(tunable, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_DUPLICATE_KEY);
    assert(report.line == 3u);

    assert(parse(blanked, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_DUPLICATE_KEY);
    assert(report.line == 2u);

    assert(parse(sim, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_DUPLICATE_KEY);

    assert(parse(call, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_DUPLICATE_KEY);
}

/* CLAIM: "Values are decimal integers with an optional leading + or -. No hex,
 * no underscores, no floating point."
 *
 * The rejections matter more than the acceptances. A configuration file
 * containing 0x64 that parsed as 0, or 1_000 that parsed as 1, would be a
 * threshold silently different from the one the operator wrote down. */
static void test_the_integer_grammar(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;
    oa_tunable_t             v = 0;

    static const char plus[]  = "burnout_accel_hysteresis_cg = +25\n";
    static const char minus[] = "landing_alt_band_cm = -50\n";
    static const char zero[]  = "led_brightness_pct = 0\n";

    static const char *const rejected[] = {
        "sample_rate_hz = 0x64\n",   /* hex */
        "sample_rate_hz = 1_000\n",  /* underscores */
        "sample_rate_hz = 200.0\n",  /* floating point */
        "sample_rate_hz = 200Hz\n",  /* a unit the table already states */
        "sample_rate_hz = two\n",    /* a word */
        "sample_rate_hz = -\n",      /* a lone sign */
        "sample_rate_hz = + 5\n",    /* a sign detached from its digits */
        "sample_rate_hz = 1 2\n"     /* two values */
    };
    size_t i;

    assert(parse(plus, &cfg, &report) == OA_OK);
    assert(oa_config_get(&cfg, OA_CFG_BURNOUT_ACCEL_HYSTERESIS_CG, &v) == OA_OK);
    assert(v == 25);

    assert(parse(minus, &cfg, &report) == OA_OK);
    assert(oa_config_get(&cfg, OA_CFG_LANDING_ALT_BAND_CM, &v) == OA_OK);
    assert(v == -50);

    /* Zero is a measurement, not an absence. The set bit is what says a field
     * has a value, and it has to be raised for a value of zero too. */
    assert(parse(zero, &cfg, &report) == OA_OK);
    assert(oa_config_is_set(&cfg, OA_CFG_LED_BRIGHTNESS_PCT) == true);
    assert(oa_config_get(&cfg, OA_CFG_LED_BRIGHTNESS_PCT, &v) == OA_OK);
    assert(v == 0);

    for (i = 0u; i < (sizeof rejected / sizeof rejected[0]); i++) {
        assert(parse(rejected[i], &cfg, &report) == OA_ERR_RANGE);
        assert(report.error == OA_PARSE_ERR_VALUE_SYNTAX);
        assert(report.line == 1u);
    }
}

/* CLAIM: "OA_PARSE_ERR_VALUE_RANGE: the value is a well-formed integer that does
 * not fit in oa_tunable_t", and the boundaries either side of it. */
static void test_integer_range(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;
    oa_tunable_t             v = 0;

    assert(parse("sample_rate_hz = 2147483647\n", &cfg, &report) == OA_OK);
    assert(oa_config_get(&cfg, OA_CFG_SAMPLE_RATE_HZ, &v) == OA_OK);
    assert(v == 2147483647);

    assert(parse("sample_rate_hz = -2147483647\n", &cfg, &report) == OA_OK);
    assert(oa_config_get(&cfg, OA_CFG_SAMPLE_RATE_HZ, &v) == OA_OK);
    assert(v == -2147483647);

    assert(parse("sample_rate_hz = 2147483648\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_RANGE);

    assert(parse("sample_rate_hz = -2147483649\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_RANGE);

    /* Long enough to overflow the accumulator several times over, which is the
     * case a naive multiply-and-add gets wrong by wrapping into a plausible
     * number rather than by failing. */
    assert(parse("sample_rate_hz = 99999999999999999999\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_RANGE);
}

/* CLAIM: "OA_PARSE_ERR_VALUE_SENTINEL: the value is exactly OA_UNSET. It is a
 * legal int32 and it is the sentinel that means no measurement, so it cannot
 * also mean a measurement. Its own error rather than a range error, because the
 * operator who typed it deserves to be told which of those two things went
 * wrong." */
static void test_the_unset_sentinel_cannot_be_typed_in(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    assert(parse("landing_alt_band_cm = -2147483648\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_SENTINEL);
    assert(report.line == 1u);

    /* And it is a different message from the one an out of range value gets. */
    assert(!str_equals(oa_config_parse_error_text(OA_PARSE_ERR_VALUE_SENTINEL),
                       oa_config_parse_error_text(OA_PARSE_ERR_VALUE_RANGE)));
}

/* CLAIM: "A hash starts a comment that runs to the end of the line", "a blank
 * line, or a line that is only a comment, is skipped", and "a line is trimmed of
 * leading and trailing spaces and tabs". */
static void test_comments_blanks_and_whitespace(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;
    oa_tunable_t             v = 0;

    static const char text[] = "# leading comment\n"
                               "   \t  \n"
                               "\tsample_rate_hz\t=\t200\t# trailing note\n"
                               "        # an indented comment\n"
                               "batt_low_mv=3600\n";

    assert(parse(text, &cfg, &report) == OA_OK);
    assert(report.keys_seen == 2u);
    assert(report.keys_set == 2u);

    assert(oa_config_get(&cfg, OA_CFG_SAMPLE_RATE_HZ, &v) == OA_OK);
    assert(v == 200);
    assert(oa_config_get(&cfg, OA_CFG_BATT_LOW_MV, &v) == OA_OK);
    assert(v == 3600);
}

/* CLAIM: "CRLF and LF both end a line, so a file edited on Windows over USB
 * parses the same as one edited on a laptop."
 *
 * The two files are compared field for field rather than merely both succeeding,
 * because a stray carriage return left on the end of a value would be a syntax
 * error on every line of a Windows-edited file, and the operator would have no
 * idea why. */
static void test_crlf_parses_the_same_as_lf(void)
{
    oa_config_t              lf;
    oa_config_t              crlf;
    oa_config_parse_report_t report;

    static const char lf_text[]   = "sample_rate_hz = 200\nsimulated = true\ncallsign = N0CALL\n";
    static const char crlf_text[] = "sample_rate_hz = 200\r\nsimulated = true\r\ncallsign = N0CALL\r\n";

    assert(parse(lf_text, &lf, &report) == OA_OK);
    assert(parse(crlf_text, &crlf, &report) == OA_OK);
    assert(memcmp(&lf, &crlf, sizeof lf) == 0);
}

/* CLAIM: "Total over the input. Any byte sequence of any length, including one
 * with embedded NULs, no trailing newline, or no valid line at all, produces a
 * defined result and reads nothing outside [text, text + len). The text does not
 * have to be NUL terminated and is never scanned past len."
 *
 * The bytes after the span are poisoned with a line that would parse and set a
 * field. If the parser reads past its length, that field comes back set, which
 * is a threshold arriving from memory the operator never wrote. */
static void test_the_parser_never_reads_past_its_length(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char buffer[] = "sample_rate_hz = 200\nbatt_low_mv = 3600\n";
    const size_t      cut      = 21u; /* Exactly the first line, including its newline. */

    assert(oa_config_parse(buffer, cut, &cfg, &report) == OA_OK);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == true);
    assert(oa_config_is_set(&cfg, OA_CFG_BATT_LOW_MV) == false);
    assert(report.keys_seen == 1u);

    /* A final line with no newline after it is a complete line. Text editors
     * disagree about whether to add one, and a file that lost its last setting
     * depending on which editor wrote it would be a very hard fault to find. */
    assert(oa_config_parse("sample_rate_hz = 200", 20u, &cfg, &report) == OA_OK);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == true);

    /* An empty file is a valid configuration: everything unset, nothing set, and
     * a payload that will refuse to arm and say exactly what is missing. */
    assert(oa_config_parse("", 0u, &cfg, &report) == OA_OK);
    assert(report.keys_seen == 0u);
    assert(report.keys_set == 0u);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == false);
}

/* CLAIM: the printable-only rule on keys, whose stated reason is that "a key
 * containing an embedded NUL, which a corrupted flash read can produce, would be
 * copied into a NUL terminated buffer and would match the field whose name is
 * its prefix. That is a threshold set by a corrupted file, silently."
 *
 * This is the test that would have caught it. */
static void test_a_corrupted_key_does_not_match_by_prefix(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    /* "sample_rate_hz" followed by a NUL and more bytes, then a value. A parser
     * that terminated the key at the NUL would set sample_rate_hz from a line
     * that does not say sample_rate_hz. */
    static const char text[] = "sample_rate_hz\0junk = 200\n";

    assert(oa_config_parse(text, sizeof text - 1u, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_UNKNOWN_KEY);
    assert(oa_config_is_set(&cfg, OA_CFG_SAMPLE_RATE_HZ) == false);

    /* The echo is safe to print: the header says bytes that are not printable
     * are replaced rather than copied, because this string goes to a console and
     * an embedded NUL would truncate the message at the interesting part. */
    assert(strlen(report.text) >= strlen("sample_rate_hz"));
    {
        size_t i;

        for (i = 0u; report.text[i] != '\0'; i++) {
            assert(report.text[i] >= 0x20 && report.text[i] < 0x7F);
        }
    }
}

/* CLAIM: "OA_PARSE_ERR_NO_KEY: the line begins with '=' or is otherwise a value
 * with no key", and "OA_PARSE_ERR_SEPARATOR: something follows the key that is
 * not '='". */
static void test_structural_errors(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    assert(parse("= 200\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_NO_KEY);

    assert(parse("   =\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_NO_KEY);

    assert(parse("sample_rate_hz 200\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_SEPARATOR);

    assert(parse("sample_rate_hz : 200\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_SEPARATOR);

    /* A key longer than any field name cannot be one, and is reported as unknown
     * by the same route a misspelling is. */
    {
        char line[OA_CONFIG_PARSE_KEY_MAX + 16u];

        memset(line, 'k', sizeof line);
        line[sizeof line - 2u] = '\n';
        line[sizeof line - 1u] = '\0';

        assert(parse(line, &cfg, &report) == OA_ERR_RANGE);
        assert(report.error == OA_PARSE_ERR_UNKNOWN_KEY);
    }
}

/* CLAIM: "simulated = true | false | 1 | 0" and, from oa_config.h, that
 * simulated "is the one bit that stops a bench run being published as a flight".
 * It has a meaningful default, false, and a file that does not mention it gets
 * that default rather than an error. */
static void test_simulated(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    assert(parse("simulated = true\n", &cfg, &report) == OA_OK);
    assert(cfg.simulated == true);

    assert(parse("simulated = 1\n", &cfg, &report) == OA_OK);
    assert(cfg.simulated == true);

    assert(parse("simulated = false\n", &cfg, &report) == OA_OK);
    assert(cfg.simulated == false);

    assert(parse("simulated = 0\n", &cfg, &report) == OA_OK);
    assert(cfg.simulated == false);

    assert(parse("sample_rate_hz = 200\n", &cfg, &report) == OA_OK);
    assert(cfg.simulated == false);

    /* A word that is neither is an error rather than a guess. "yes" is the one
     * an operator will type, and reading it as false would publish a bench run
     * as a flight. */
    assert(parse("simulated = yes\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_SYNTAX);

    assert(parse("simulated = TRUE\n", &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_SYNTAX);
}

/* CLAIM: "callsign = up to 15 characters, no spaces", and
 * "OA_PARSE_ERR_VALUE_TOO_LONG: truncating instead would transmit a station
 * identification that is not the operator's, under a legal regime where
 * identification is the requirement." */
static void test_callsign(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    static const char fifteen[]  = "callsign = ABCDEFGHIJKLMNO\n";
    static const char sixteen[]  = "callsign = ABCDEFGHIJKLMNOP\n";
    static const char spaced[]   = "callsign = N0 CALL\n";

    assert(parse("callsign = N0CALL\n", &cfg, &report) == OA_OK);
    assert(str_equals(cfg.callsign, "N0CALL"));

    assert(parse(fifteen, &cfg, &report) == OA_OK);
    assert(str_equals(cfg.callsign, "ABCDEFGHIJKLMNO"));
    assert(strlen(cfg.callsign) == OA_CONFIG_PARSE_CALLSIGN_MAX);

    assert(parse(sixteen, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_TOO_LONG);

    assert(parse(spaced, &cfg, &report) == OA_ERR_RANGE);
    assert(report.error == OA_PARSE_ERR_VALUE_SYNTAX);

    /* Empty is a valid state: it is the state of every operator flying under an
     * unlicensed allocation, where there is nothing to identify with. */
    assert(parse("callsign =\n", &cfg, &report) == OA_OK);
    assert(cfg.callsign[0] == '\0');
}

/* CLAIM: every key in OA_CONFIG_FIELDS is reachable by its own name from the
 * file, which is what makes the field table the single home for the tunables
 * rather than a list the parser has a second opinion about.
 *
 * Built by walking the table, so a field added to oa_config.h is covered here
 * the day it lands and nobody has to remember to add a case. */
static void test_every_field_in_the_table_is_settable_by_name(void)
{
    int i;

    for (i = 0; i < (int)OA_CFG_FIELD_COUNT; i++) {
        const oa_config_field_t  field = (oa_config_field_t)i;
        const char              *name  = oa_config_field_name(field);
        char                     line[OA_CONFIG_PARSE_KEY_MAX + 32u];
        oa_config_t              cfg;
        oa_config_parse_report_t report;
        oa_tunable_t             v = OA_UNSET;
        size_t                   n;

        assert(name != NULL);
        n = strlen(name);
        assert(n + 8u < sizeof line);

        memcpy(line, name, n);
        memcpy(line + n, " = 7\n", 6u);

        assert(parse(line, &cfg, &report) == OA_OK);
        assert(report.keys_seen == 1u);
        assert(report.keys_set == 1u);
        assert(oa_config_is_set(&cfg, field) == true);
        assert(oa_config_get(&cfg, field, &v) == OA_OK);
        assert(v == 7);

        /* And the same key with no value leaves that same field unset. */
        memcpy(line + n, "\n", 2u);
        assert(parse(line, &cfg, &report) == OA_OK);
        assert(report.keys_seen == 1u);
        assert(report.keys_set == 0u);
        assert(oa_config_is_set(&cfg, field) == false);
    }
}

/* CLAIM: "Returns OA_OK, OA_ERR_NULL if text or cfg is NULL", and "report may be
 * NULL, in which case the failure is reported only as a result code."
 *
 * A NULL text is an error even with a length of zero. An empty file is a real
 * pointer and a length of zero; a NULL pointer is a caller that did not read the
 * file, and telling those apart is the difference between an empty
 * configuration and a storage fault. */
static void test_null_arguments(void)
{
    oa_config_t              cfg;
    oa_config_parse_report_t report;

    assert(oa_config_parse(NULL, 0u, &cfg, &report) == OA_ERR_NULL);
    assert(report.line == 0u);
    assert(report.error == OA_PARSE_OK);

    assert(oa_config_parse("sample_rate_hz = 200\n", 21u, NULL, &report) == OA_ERR_NULL);

    /* Without a report, both outcomes are still reported as a result code. */
    assert(oa_config_parse("sample_rate_hz = 200\n", 21u, &cfg, NULL) == OA_OK);
    assert(oa_config_parse("not_a_setting = 1\n", 18u, &cfg, NULL) == OA_ERR_RANGE);
}

/* CLAIM: "One line of English for an error code, for the console. Never NULL: an
 * unrecognised code returns a string saying so rather than a null pointer that a
 * print path would have to special-case."
 *
 * Every enumerator, plus a value that is not one. A payload that faulted while
 * printing why it will not arm would be the worst possible time to fault. */
static void test_error_text_is_never_null(void)
{
    int code;

    for (code = 0; code <= 8; code++) {
        assert(oa_config_parse_error_text((oa_parse_error_t)code) != NULL);
    }
    assert(oa_config_parse_error_text((oa_parse_error_t)99) != NULL);
    assert(oa_config_parse_error_text((oa_parse_error_t)-1) != NULL);
}

/* CLAIM: "Zero a report. Safe to call on a report that is about to be filled in
 * anyway; it exists so a caller can print a report it never passed to the parser
 * without reading uninitialised memory." */
static void test_report_init(void)
{
    oa_config_parse_report_t report;

    memset(&report, 0xAA, sizeof report);
    oa_config_parse_report_init(&report);

    assert(report.line == 0u);
    assert(report.error == OA_PARSE_OK);
    assert(report.text[0] == '\0');
    assert(report.keys_seen == 0u);
    assert(report.keys_set == 0u);

    oa_config_parse_report_init(NULL);
}

int main(void)
{
    test_a_key_with_no_value_stays_unset();
    test_an_unknown_key_is_an_error_that_names_the_line();
    test_a_failed_parse_writes_nothing();
    test_a_successful_parse_starts_from_unset();
    test_a_duplicate_key_is_an_error();
    test_the_integer_grammar();
    test_integer_range();
    test_the_unset_sentinel_cannot_be_typed_in();
    test_comments_blanks_and_whitespace();
    test_crlf_parses_the_same_as_lf();
    test_the_parser_never_reads_past_its_length();
    test_a_corrupted_key_does_not_match_by_prefix();
    test_structural_errors();
    test_simulated();
    test_callsign();
    test_every_field_in_the_table_is_settable_by_name();
    test_null_arguments();
    test_error_text_is_never_null();
    test_report_init();

    printf("test_oa_config_parse: all checks passed\n");
    return 0;
}

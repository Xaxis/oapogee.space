/*
 * oApogee onboard log: meta.json.
 *
 * The manifest is what makes this format self-describing. A reader takes the
 * offsets, types and scales from here and decodes the records without ever
 * having read docs/spec/log-format.md, which is the property that stops a log
 * written today becoming unreadable in ten years.
 *
 * That property is only worth anything if the manifest and the records agree.
 * So the stream layout below is expanded from oapogee/oa_log_fields.def, the
 * same table oa_log_record.c expands to pack the bytes. There is no second
 * description of the layout to keep in step: a field that moves moves in the
 * table, and the manifest and the packer move together or neither moves. A
 * manifest that disagreed with the packer would produce files that decode
 * wrongly and look fine, and that is the failure this arrangement makes
 * structurally impossible rather than merely unlikely.
 *
 * Source of truth: docs/spec/log-format.md, spec_version 1. That document is
 * normative. Where it and this file disagree, this file is wrong.
 *
 * NO FLOATING POINT, anywhere in this writer. Scales are copied verbatim from
 * the table's string literals, so the exact text the spec publishes is the text
 * that is written: 1e-7 printed through a double formatter on a part whose
 * floating point support nobody has characterised is a way to turn an exact
 * scale factor into an approximate one, in the one file that tells a reader how
 * to interpret every other byte. The summary's scaled values are composed from
 * integer division and remainder for the same reason.
 *
 * Nothing is buffered and nothing is allocated. The JSON is written through the
 * sink as it is produced, a literal at a time, because core allocates nothing
 * and a manifest large enough to need a buffer would need one sized for the
 * longest device id anybody ever configures.
 *
 * NOTHING HERE HAS RUN ON HARDWARE. No flight has been logged, so no meta.json
 * produced by this file has ever been read by a tool.
 */

#include "oapogee/oa_log.h"

/* Every write can fail: the flash can fill up mid-manifest. Stopping at the
 * first failure leaves a truncated meta.json, which is unparseable and
 * therefore obviously broken. Continuing would produce a complete-looking
 * manifest with a hole in it. */
#define OA_TRY(expr)                          \
    do {                                      \
        const oa_result_t oa_res = (expr);    \
        if (oa_res != OA_OK) {                \
            return oa_res;                    \
        }                                     \
    } while (0)

/* ---------------------------------------------------------------------------
 * Number and string emission.
 *
 * There is no snprintf here and no libc beyond what core is allowed. Both are
 * deliberate: a printf pulled into core drags in a formatter, a locale and,
 * on some toolchains, floating point support, and all three would appear in the
 * check-undefined output as symbols core has no business referencing.
 * ------------------------------------------------------------------------ */

/* Magnitude of a signed value as an unsigned one. Written the long way because
 * negating INT32_MIN is undefined, and the sentinel that means "not measured"
 * is exactly INT32_MIN. */
static uint32_t oa_magnitude_u32(int32_t value)
{
    if (value < 0) {
        return (uint32_t)(-(value + 1)) + 1u;
    }
    return (uint32_t)value;
}

static oa_result_t oa_emit_u32(oa_sink_t *sink, uint32_t value)
{
    uint8_t digits[10]; /* 4294967295 is ten digits, so this cannot overflow. */
    size_t  count = 0;
    size_t  i;

    do {
        digits[count] = (uint8_t)('0' + (value % 10u));
        count++;
        value /= 10u;
    } while (value != 0u);

    for (i = 0; i < count / 2u; i++) {
        const uint8_t swap      = digits[i];
        digits[i]               = digits[count - 1u - i];
        digits[count - 1u - i]  = swap;
    }

    return oa_sink_write(sink, digits, count);
}

static oa_result_t oa_emit_i32(oa_sink_t *sink, int32_t value)
{
    if (value < 0) {
        OA_TRY(oa_sink_write_str(sink, "-"));
    }
    return oa_emit_u32(sink, oa_magnitude_u32(value));
}

/* A scaled decimal composed from integer arithmetic. `divisor` is the number of
 * stored units in one published unit, 100 for centimetres to metres, and it is
 * a property of the format rather than a tunable.
 *
 * The result is exact. Every scale in this format is a power of ten and the
 * stored quantity is an integer, so there is nothing to round and no reason for
 * a float to be involved. */
static oa_result_t oa_emit_fixed(oa_sink_t *sink, int32_t value, uint32_t divisor)
{
    const uint32_t magnitude = oa_magnitude_u32(value);
    uint32_t       place;

    if (value < 0) {
        OA_TRY(oa_sink_write_str(sink, "-"));
    }
    OA_TRY(oa_emit_u32(sink, magnitude / divisor));
    OA_TRY(oa_sink_write_str(sink, "."));

    /* Emit the fraction digit by digit so that 5 cm is 0.05 m and not 0.5 m. */
    for (place = divisor / 10u; place > 0u; place /= 10u) {
        const uint8_t digit = (uint8_t)('0' + ((magnitude / place) % 10u));
        OA_TRY(oa_sink_write(sink, &digit, 1));
    }
    return OA_OK;
}

/* OA_UNSET is written as JSON null. The spec requires this and a reader is
 * required to treat it as not known, which is what stops an unfinished flight
 * being read as a measured zero. */
static oa_result_t oa_emit_tunable(oa_sink_t *sink, oa_tunable_t value)
{
    if (!OA_IS_SET(value)) {
        return oa_sink_write_str(sink, "null");
    }
    return oa_emit_i32(sink, value);
}

static oa_result_t oa_emit_tunable_fixed(oa_sink_t *sink, oa_tunable_t value, uint32_t divisor)
{
    if (!OA_IS_SET(value)) {
        return oa_sink_write_str(sink, "null");
    }
    return oa_emit_fixed(sink, value, divisor);
}

static oa_result_t oa_emit_bool(oa_sink_t *sink, bool value)
{
    return oa_sink_write_str(sink, value ? "true" : "false");
}

/* A JSON string, or null for a NULL pointer.
 *
 * NULL is the correct value for hw_rev on a board that has not been fabricated
 * and for armed_utc on a build with no GNSS, and it is different from an empty
 * string, which would claim a revision that is blank.
 *
 * The escaping is not decoration. These strings come from outside core: a device
 * id derived from the microcontroller, a git hash, a tier name from a build
 * script. One stray quote or backslash in any of them would produce a meta.json
 * that no reader can parse, in the file that describes every other byte in the
 * directory. */
static oa_result_t oa_emit_json_string(oa_sink_t *sink, const char *str)
{
    static const char hex[] = "0123456789abcdef";
    const char       *p;

    if (str == NULL) {
        return oa_sink_write_str(sink, "null");
    }

    OA_TRY(oa_sink_write_str(sink, "\""));
    for (p = str; *p != '\0'; p++) {
        const unsigned char ch = (unsigned char)*p;

        if (ch == (unsigned char)'"') {
            OA_TRY(oa_sink_write_str(sink, "\\\""));
        } else if (ch == (unsigned char)'\\') {
            OA_TRY(oa_sink_write_str(sink, "\\\\"));
        } else if (ch == (unsigned char)'\n') {
            OA_TRY(oa_sink_write_str(sink, "\\n"));
        } else if (ch == (unsigned char)'\r') {
            OA_TRY(oa_sink_write_str(sink, "\\r"));
        } else if (ch == (unsigned char)'\t') {
            OA_TRY(oa_sink_write_str(sink, "\\t"));
        } else if (ch < 0x20u) {
            const uint8_t esc[6] = { (uint8_t)'\\',
                                     (uint8_t)'u',
                                     (uint8_t)'0',
                                     (uint8_t)'0',
                                     (uint8_t)hex[(ch >> 4) & 0x0Fu],
                                     (uint8_t)hex[ch & 0x0Fu] };
            OA_TRY(oa_sink_write(sink, esc, sizeof esc));
        } else {
            /* Bytes at or above 0x80 pass through unchanged. A UTF-8 sequence is
             * already valid JSON and re-encoding it here would need a decoder
             * core has no reason to carry. */
            const uint8_t raw = (uint8_t)ch;
            OA_TRY(oa_sink_write(sink, &raw, 1));
        }
    }
    return oa_sink_write_str(sink, "\"");
}

/* ---------------------------------------------------------------------------
 * The stream objects, expanded from the record table.
 *
 * One emitter per stream, named from the table, so the manifest lists exactly
 * the streams the packers can write and describes them with exactly the offsets
 * the packers use. `#offset`, `#count` and `#wire` are the same tokens
 * oa_log_record.c pastes into the stores, stringised. They cannot disagree
 * because they are not two descriptions; they are one.
 *
 * The comma between field objects is written before each field except the
 * first rather than after each except the last, because the table cannot say
 * which line is last and a trailing comma is not valid JSON.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes)                        \
    static oa_result_t oa_log_emit_stream_##sym(oa_sink_t *sink, oa_tunable_t nominal_hz) \
    {                                                                                \
        bool first = true;                                                           \
        OA_TRY(oa_sink_write_str(sink,                                               \
                                 "    \"" #sym "\": {\n"                             \
                                 "      \"file\": \"" filename "\",\n"               \
                                 "      \"record_bytes\": " #record_bytes ",\n"      \
                                 "      \"endian\": \"little\",\n"                   \
                                 "      \"nominal_hz\": "));                         \
        OA_TRY(oa_emit_tunable(sink, nominal_hz));                                   \
        OA_TRY(oa_sink_write_str(sink, ",\n      \"fields\": [\n"));
#define OA_LOG_FIELD(name, wire, offset, scale, unit)                              \
    if (!first) {                                                                  \
        OA_TRY(oa_sink_write_str(sink, ",\n"));                                    \
    }                                                                              \
    first = false;                                                                 \
    OA_TRY(oa_sink_write_str(sink,                                                 \
                             "        { \"name\": \"" #name "\", \"offset\": " #offset \
                             ", \"type\": \"" #wire "\", \"scale\": " scale        \
                             ", \"unit\": \"" unit "\" }"));
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)                   \
    if (!first) {                                                                  \
        OA_TRY(oa_sink_write_str(sink, ",\n"));                                    \
    }                                                                              \
    first = false;                                                                 \
    OA_TRY(oa_sink_write_str(sink,                                                 \
                             "        { \"name\": \"" #name "\", \"offset\": " #offset \
                             ", \"type\": \"" #wire "\", \"count\": " #count       \
                             ", \"scale\": " scale ", \"unit\": \"" unit "\" }"));
#define OA_LOG_STREAM_END(SYM, sym)                            \
    OA_TRY(oa_sink_write_str(sink, "\n      ]\n    }"));       \
    return OA_OK;                                              \
    }
#include "oapogee/oa_log_fields.def"

/* ---------------------------------------------------------------------------
 * meta.json.
 * ------------------------------------------------------------------------ */

oa_result_t oa_log_write_manifest(const oa_log_manifest_t *manifest,
                                  const oa_log_summary_t  *summary,
                                  oa_sink_t               *sink)
{
    /* A NULL summary is the manifest written when the flight directory is
     * created, and the format requires that one to carry a summary object whose
     * every member is null with landed false. Substituting an all-unset summary
     * rather than branching on NULL at each member is what makes the two paths
     * produce the same shape by construction: there is one emitter, so the
     * creation-time manifest cannot drift from the one written at LANDED. */
    static const oa_log_summary_t unmeasured = {
        OA_UNSET, OA_UNSET, OA_UNSET, OA_UNSET, OA_UNSET, false
    };
    const oa_log_summary_t *sum = (summary != NULL) ? summary : &unmeasured;
    size_t                  axis;

    if (manifest == NULL || sink == NULL) {
        return OA_ERR_NULL;
    }

    OA_TRY(oa_sink_write_str(sink, "{\n  \"spec_version\": "));
    OA_TRY(oa_emit_u32(sink, (uint32_t)OA_LOG_SPEC_VERSION));
    OA_TRY(oa_sink_write_str(sink, ",\n  \"flight\": "));
    OA_TRY(oa_emit_u32(sink, manifest->flight));

    /* device. The id is derived from the microcontroller's unique identifier and
     * is stable across reflashes, which is what lets a flight archive group
     * flights by airframe. */
    OA_TRY(oa_sink_write_str(sink, ",\n  \"device\": {\n    \"id\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->device_id));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"tier\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->tier));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"path\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->path));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"hw_rev\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->hw_rev));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"fw_version\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->fw_version));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"fw_git\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->fw_git));

    /* session. armed_utc stays null unless a GNSS fix supplied it: there is no
     * real-time clock on this board, so every time in the log is relative to
     * arming and absolute time is attached at offload.
     *
     * simulated is not decorative. A bench run produces a complete, plausible
     * log, and without this bit a flight archive will eventually publish one as
     * a flight. */
    OA_TRY(oa_sink_write_str(sink, "\n  },\n  \"session\": {\n    \"armed_utc\": "));
    OA_TRY(oa_emit_json_string(sink, manifest->armed_utc));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"armed_uptime_ms\": "));
    OA_TRY(oa_emit_u32(sink, manifest->armed_uptime_ms));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"simulated\": "));
    OA_TRY(oa_emit_bool(sink, manifest->simulated));

    /* calibration, in the integer units the field names carry. pad_pressure_pa
     * is the zero the entire altitude column is measured against, and
     * publishing it with its sample count is what lets an analyst recompute
     * altitude from pressure_pa rather than discard a flight whose reference was
     * taken badly. */
    OA_TRY(oa_sink_write_str(sink, "\n  },\n  \"calibration\": {\n    \"pad_pressure_pa\": "));
    OA_TRY(oa_emit_i32(sink, manifest->pad_pressure_pa));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"pad_pressure_samples\": "));
    OA_TRY(oa_emit_u32(sink, manifest->pad_pressure_samples));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"pad_temperature_dc\": "));
    OA_TRY(oa_emit_i32(sink, (int32_t)manifest->pad_temperature_dc));

    OA_TRY(oa_sink_write_str(sink, ",\n    \"accel_bias_mg\": ["));
    for (axis = 0; axis < 3u; axis++) {
        if (axis != 0u) {
            OA_TRY(oa_sink_write_str(sink, ", "));
        }
        OA_TRY(oa_emit_i32(sink, (int32_t)manifest->accel_bias_mg[axis]));
    }
    OA_TRY(oa_sink_write_str(sink, "],\n    \"gyro_bias_cdps\": ["));
    for (axis = 0; axis < 3u; axis++) {
        if (axis != 0u) {
            OA_TRY(oa_sink_write_str(sink, ", "));
        }
        OA_TRY(oa_emit_i32(sink, (int32_t)manifest->gyro_bias_cdps[axis]));
    }
    OA_TRY(oa_sink_write_str(sink, "]"));

    /* streams lists exactly the files present in the directory and nothing else,
     * so a build with no GNSS receiver carries no gnss key at all. A reader
     * discovers streams by enumerating this object, never by assuming a name. */
    OA_TRY(oa_sink_write_str(sink, "\n  },\n  \"streams\": {\n"));
    OA_TRY(oa_log_emit_stream_flight(sink, manifest->flight_nominal_hz));
    if (manifest->has_gnss_stream) {
        OA_TRY(oa_sink_write_str(sink, ",\n"));
        OA_TRY(oa_log_emit_stream_gnss(sink, manifest->gnss_nominal_hz));
    }
    OA_TRY(oa_sink_write_str(sink, "\n  },\n"));

    /* summary. The struct carries integers in the units the rest of the firmware
     * works in; the spec publishes metres, g and m/s under these names, so the
     * scaling happens here and exactly once. A null member means not known,
     * which is different from zero, and landed false in a manifest is itself
     * information: the flight ended before landing was detected. */
    OA_TRY(oa_sink_write_str(sink, "  \"summary\": {\n    \"apogee_m\": "));
    OA_TRY(oa_emit_tunable_fixed(sink, sum->apogee_cm, 100u));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"t_apogee_ms\": "));
    OA_TRY(oa_emit_tunable(sink, sum->t_apogee_ms));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"max_accel_g\": "));
    OA_TRY(oa_emit_tunable_fixed(sink, sum->max_accel_cg, 100u));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"max_velocity_m_s\": "));
    OA_TRY(oa_emit_tunable_fixed(sink, sum->max_velocity_dm_s, 10u));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"flight_duration_ms\": "));
    OA_TRY(oa_emit_tunable(sink, sum->flight_duration_ms));
    OA_TRY(oa_sink_write_str(sink, ",\n    \"landed\": "));
    OA_TRY(oa_emit_bool(sink, sum->landed));

    /* The trailing newline is for the human who cats this file over USB while
     * working out why a flight looks wrong. */
    return oa_sink_write_str(sink, "\n  }\n}\n");
}

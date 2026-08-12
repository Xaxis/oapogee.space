/*
 * Conformance test: oa_log_manifest.c.
 *
 * Two kinds of check live here.
 *
 * The first compares the emitted meta.json against the document in
 * docs/spec/log-format.md, typed out again by hand. The manifest generates
 * itself from oa_log_fields.def, so a test that expanded that same table would
 * be checking the table against itself and a wrong table would pass. The
 * specification is restated here so that something compares the table to the
 * normative document.
 *
 * The second is the one that matters. It takes the offsets and types out of the
 * emitted manifest, as a third-party reader would, and decodes a record the
 * packer produced. That is the whole claim the manifest makes: a reader that has
 * never seen the specification can decode these files correctly. If the manifest
 * and the packer ever disagreed, this is where it would show up as a wrong
 * number rather than as a file that looks fine.
 *
 * Plain C and assert, no framework.
 *
 * NOTHING UNDER TEST HAS RUN ON HARDWARE, and no flight has been logged. These
 * checks prove the writer agrees with a specification and with its own packer.
 */

#include "oapogee/oa_log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * A sink over a fixed buffer, NUL-terminated so the result can be compared as
 * text. The manifest itself is written without a terminator: it is a file, not
 * a C string.
 * ------------------------------------------------------------------------ */

typedef struct {
    char   buf[8192];
    size_t used;
} text_sink_t;

static oa_result_t text_write(void *ctx, const uint8_t *data, size_t len)
{
    text_sink_t *s = (text_sink_t *)ctx;
    if (s->used + len + 1u > sizeof s->buf) {
        return OA_ERR_SINK_FULL;
    }
    memcpy(s->buf + s->used, data, len);
    s->used += len;
    s->buf[s->used] = '\0';
    return OA_OK;
}

static size_t text_space(void *ctx)
{
    const text_sink_t *s = (const text_sink_t *)ctx;
    return sizeof s->buf - s->used - 1u;
}

static oa_result_t text_sync(void *ctx)
{
    (void)ctx;
    return OA_OK;
}

static const oa_sink_vtable_t text_vt = { text_write, text_space, text_sync };

static void text_sink_init(text_sink_t *s, oa_sink_t *sink)
{
    memset(s, 0, sizeof *s);
    sink->vt  = &text_vt;
    sink->ctx = s;
}

/* The Track manifest from the specification's example, minus the values that
 * are illustrative. Nothing here was measured: no payload has run, so
 * armed_uptime_ms reports nothing and the pad pressure is a plausible sea level
 * figure used to exercise the writer. */
static void fill_track_manifest(oa_log_manifest_t *m)
{
    memset(m, 0, sizeof *m);
    m->flight               = 1u;
    m->device_id            = "oapogee-000000000000";
    m->tier                 = "track";
    m->path                 = "board";
    m->hw_rev               = NULL; /* No board has been fabricated. */
    m->fw_version           = "0.1.0";
    m->fw_git               = "0000000";
    m->armed_utc            = NULL; /* No fix has supplied UTC. */
    m->armed_uptime_ms      = 128394u;
    m->simulated            = false;
    m->pad_pressure_pa      = 101325;
    m->pad_pressure_samples = 0u;
    m->pad_temperature_dc   = 0;
    m->has_gnss_stream      = true;
    m->flight_nominal_hz    = OA_UNSET; /* No log rate has been chosen. */
    m->gnss_nominal_hz      = OA_UNSET;
}

/* ---------------------------------------------------------------------------
 * The specification, restated.
 * ------------------------------------------------------------------------ */

static const char *const expected_track_manifest =
    "{\n"
    "  \"spec_version\": 1,\n"
    "  \"flight\": 1,\n"
    "  \"device\": {\n"
    "    \"id\": \"oapogee-000000000000\",\n"
    "    \"tier\": \"track\",\n"
    "    \"path\": \"board\",\n"
    "    \"hw_rev\": null,\n"
    "    \"fw_version\": \"0.1.0\",\n"
    "    \"fw_git\": \"0000000\"\n"
    "  },\n"
    "  \"session\": {\n"
    "    \"armed_utc\": null,\n"
    "    \"armed_uptime_ms\": 128394,\n"
    "    \"simulated\": false\n"
    "  },\n"
    "  \"calibration\": {\n"
    "    \"pad_pressure_pa\": 101325,\n"
    "    \"pad_pressure_samples\": 0,\n"
    "    \"pad_temperature_dc\": 0,\n"
    "    \"accel_bias_mg\": [0, 0, 0],\n"
    "    \"gyro_bias_cdps\": [0, 0, 0]\n"
    "  },\n"
    "  \"streams\": {\n"
    "    \"flight\": {\n"
    "      \"file\": \"flight.bin\",\n"
    "      \"record_bytes\": 36,\n"
    "      \"endian\": \"little\",\n"
    "      \"nominal_hz\": null,\n"
    "      \"fields\": [\n"
    "        { \"name\": \"t_ms\", \"offset\": 0, \"type\": \"u32\", \"scale\": 0.001, \"unit\": \"s\" },\n"
    "        { \"name\": \"pressure_pa\", \"offset\": 4, \"type\": \"i32\", \"scale\": 1, \"unit\": \"Pa\" },\n"
    "        { \"name\": \"temp_dc\", \"offset\": 8, \"type\": \"i16\", \"scale\": 0.1, \"unit\": \"degC\" },\n"
    "        { \"name\": \"accel_mg\", \"offset\": 10, \"type\": \"i16\", \"count\": 3, \"scale\": 0.001, \"unit\": \"g\" },\n"
    "        { \"name\": \"gyro_cdps\", \"offset\": 16, \"type\": \"i16\", \"count\": 3, \"scale\": 0.01, \"unit\": \"deg/s\" },\n"
    "        { \"name\": \"hg_accel_dg\", \"offset\": 22, \"type\": \"i16\", \"count\": 3, \"scale\": 0.1, \"unit\": \"g\" },\n"
    "        { \"name\": \"alt_cm\", \"offset\": 28, \"type\": \"i32\", \"scale\": 0.01, \"unit\": \"m\" },\n"
    "        { \"name\": \"vel_dm_s\", \"offset\": 32, \"type\": \"i16\", \"scale\": 0.1, \"unit\": \"m/s\" },\n"
    "        { \"name\": \"state\", \"offset\": 34, \"type\": \"u8\", \"scale\": 1, \"unit\": \"enum\" },\n"
    "        { \"name\": \"flags\", \"offset\": 35, \"type\": \"u8\", \"scale\": 1, \"unit\": \"bits\" }\n"
    "      ]\n"
    "    },\n"
    "    \"gnss\": {\n"
    "      \"file\": \"gnss.bin\",\n"
    "      \"record_bytes\": 16,\n"
    "      \"endian\": \"little\",\n"
    "      \"nominal_hz\": null,\n"
    "      \"fields\": [\n"
    "        { \"name\": \"t_ms\", \"offset\": 0, \"type\": \"u32\", \"scale\": 0.001, \"unit\": \"s\" },\n"
    "        { \"name\": \"lat_e7\", \"offset\": 4, \"type\": \"i32\", \"scale\": 1e-7, \"unit\": \"deg\" },\n"
    "        { \"name\": \"lon_e7\", \"offset\": 8, \"type\": \"i32\", \"scale\": 1e-7, \"unit\": \"deg\" },\n"
    "        { \"name\": \"alt_m\", \"offset\": 12, \"type\": \"i16\", \"scale\": 1, \"unit\": \"m\" },\n"
    "        { \"name\": \"sats\", \"offset\": 14, \"type\": \"u8\", \"scale\": 1, \"unit\": \"count\" },\n"
    "        { \"name\": \"fix\", \"offset\": 15, \"type\": \"u8\", \"scale\": 1, \"unit\": \"enum\" }\n"
    "      ]\n"
    "    }\n"
    "  },\n"
    "  \"summary\": {\n"
    "    \"apogee_m\": null,\n"
    "    \"t_apogee_ms\": null,\n"
    "    \"max_accel_g\": null,\n"
    "    \"max_velocity_m_s\": null,\n"
    "    \"flight_duration_ms\": null,\n"
    "    \"landed\": false\n"
    "  }\n"
    "}\n";

/* Spec claim: the manifest states the record layout in full, every field with
 * its offset, type and scaling, and a writer must write it before the first
 * record with every summary member null and landed false.
 *
 * Compared as a whole document rather than field by field, because the shape is
 * as normative as the values: a reader is a JSON parser, and a missing brace is
 * as fatal as a wrong offset. */
static void test_creation_time_manifest_matches_the_spec(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);

    /* NULL summary is the manifest written when the flight directory is created,
     * before any record exists. */
    assert(oa_log_write_manifest(&m, NULL, &sink) == OA_OK);

    if (strcmp(store.buf, expected_track_manifest) != 0) {
        printf("--- emitted ---\n%s\n--- expected ---\n%s\n", store.buf, expected_track_manifest);
        assert(0 && "meta.json does not match docs/spec/log-format.md");
    }

    assert(oa_log_write_manifest(NULL, NULL, &sink) == OA_ERR_NULL);
    assert(oa_log_write_manifest(&m, NULL, NULL) == OA_ERR_NULL);
}

/* Spec claim: a writer must not list a stream in `streams` that it did not write
 * a file for. A Solo or Link build has no GNSS receiver, writes no gnss.bin, and
 * carries no gnss key at all. This is the one place the structure varies by
 * build variant. */
static void test_no_gnss_stream_on_a_build_without_one(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);
    m.tier            = "solo";
    m.has_gnss_stream = false;

    assert(oa_log_write_manifest(&m, NULL, &sink) == OA_OK);

    /* Not a key, not a filename, not a mention. A reader enumerating streams
     * must find exactly one. */
    assert(strstr(store.buf, "gnss") == NULL);
    assert(strstr(store.buf, "\"flight\": {") != NULL);
    assert(strstr(store.buf, "\"file\": \"flight.bin\"") != NULL);

    /* The flight stream keeps its full 36 byte layout regardless of which
     * footprints are populated, so hg_accel_dg is still described on a build
     * with no high-g part. */
    assert(strstr(store.buf, "\"name\": \"hg_accel_dg\"") != NULL);
    assert(strstr(store.buf, "\"record_bytes\": 36") != NULL);
}

/* Spec claim: the summary is rewritten at LANDED, and its members are published
 * in metres, g and metres per second under the names in the specification. The
 * firmware holds them as integers in cm, cg and dm/s, so the scaling happens in
 * the writer, exactly, with no floating point anywhere. */
static void test_summary_scaling_is_exact(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;
    oa_log_summary_t  summary;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);

    summary.apogee_cm          = 45678;  /* 456.78 m */
    summary.t_apogee_ms        = 8123;
    summary.max_accel_cg       = 1234;   /* 12.34 g */
    summary.max_velocity_dm_s  = 2345;   /* 234.5 m/s */
    summary.flight_duration_ms = 60123;
    summary.landed             = true;

    assert(oa_log_write_manifest(&m, &summary, &sink) == OA_OK);

    assert(strstr(store.buf,
                  "  \"summary\": {\n"
                  "    \"apogee_m\": 456.78,\n"
                  "    \"t_apogee_ms\": 8123,\n"
                  "    \"max_accel_g\": 12.34,\n"
                  "    \"max_velocity_m_s\": 234.5,\n"
                  "    \"flight_duration_ms\": 60123,\n"
                  "    \"landed\": true\n"
                  "  }\n"
                  "}\n") != NULL);

    /* A value smaller than one published unit keeps its leading zero, and a
     * negative one keeps its sign. Negative is not a bug: a barometric zero
     * taken on the pad reports negative altitude if the rocket lands below the
     * pad. */
    text_sink_init(&store, &sink);
    summary.apogee_cm         = -5;
    summary.max_velocity_dm_s = -1;
    assert(oa_log_write_manifest(&m, &summary, &sink) == OA_OK);
    assert(strstr(store.buf, "\"apogee_m\": -0.05,") != NULL);
    assert(strstr(store.buf, "\"max_velocity_m_s\": -0.1,") != NULL);

    /* An individually unknown member is null even when the flight landed, so a
     * reader never sees a measured zero where nothing was measured. */
    text_sink_init(&store, &sink);
    summary.apogee_cm   = OA_UNSET;
    summary.max_accel_cg = OA_UNSET;
    assert(oa_log_write_manifest(&m, &summary, &sink) == OA_OK);
    assert(strstr(store.buf, "\"apogee_m\": null,") != NULL);
    assert(strstr(store.buf, "\"max_accel_g\": null,") != NULL);
    assert(strstr(store.buf, "\"landed\": true") != NULL);
}

/* nominal_hz is the configured rate, not the achieved one, and no log rate has
 * been chosen because choosing one needs measured flash write bandwidth. Unset
 * is written as null rather than as a plausible round number. */
static void test_nominal_hz_round_trip(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);
    m.flight_nominal_hz = 100;
    m.gnss_nominal_hz   = 5;

    assert(oa_log_write_manifest(&m, NULL, &sink) == OA_OK);
    assert(strstr(store.buf, "\"record_bytes\": 36,\n      \"endian\": \"little\",\n"
                             "      \"nominal_hz\": 100,\n") != NULL);
    assert(strstr(store.buf, "\"record_bytes\": 16,\n      \"endian\": \"little\",\n"
                             "      \"nominal_hz\": 5,\n") != NULL);
}

/* The strings in the manifest come from outside core: an id derived from the
 * microcontroller, a git hash, a tier name from a build script. A stray quote or
 * backslash in any of them must not produce a meta.json that no reader can
 * parse, in the file that describes every other byte in the directory. */
static void test_strings_are_escaped(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);
    m.fw_git = "a\"b\\c\td";

    assert(oa_log_write_manifest(&m, NULL, &sink) == OA_OK);
    assert(strstr(store.buf, "\"fw_git\": \"a\\\"b\\\\c\\td\"") != NULL);
}

/* ---------------------------------------------------------------------------
 * Reading the records with nothing but the manifest.
 *
 * This is what a third-party tool does, and it is the entire argument for the
 * manifest existing. The offsets and types below are taken out of the emitted
 * JSON, not out of the specification and not out of the field table.
 * ------------------------------------------------------------------------ */

/* The text between "flight": { and the end of that object. t_ms appears in both
 * streams, so a search that was not confined to one stream could take an offset
 * from the wrong record. */
static const char *flight_section(const char *json)
{
    const char *start = strstr(json, "\"flight\": {");
    assert(start != NULL);
    return start;
}

static const char *section_end(const char *section)
{
    const char *gnss = strstr(section, "\"gnss\": {");
    return (gnss != NULL) ? gnss : (section + strlen(section));
}

static long parse_long(const char **p)
{
    long        value = 0;
    const char *s     = *p;
    int         neg   = 0;

    if (*s == '-') {
        neg = 1;
        s++;
    }
    assert(*s >= '0' && *s <= '9');
    while (*s >= '0' && *s <= '9') {
        value = (value * 10) + (*s - '0');
        s++;
    }
    *p = s;
    return neg ? -value : value;
}

/* Find one field object by name and return what a reader would learn from it:
 * where the field starts, how wide each element is, whether it is signed, and
 * how many elements there are. */
static void manifest_field(const char *json,
                           const char *name,
                           long       *out_offset,
                           size_t     *out_width,
                           int        *out_signed,
                           long       *out_count)
{
    char        needle[128];
    const char *section = flight_section(json);
    const char *limit   = section_end(section);
    const char *at;
    const char *cursor;

    (void)snprintf(needle, sizeof needle, "{ \"name\": \"%s\", \"offset\": ", name);
    at = strstr(section, needle);
    assert(at != NULL && at < limit);

    cursor      = at + strlen(needle);
    *out_offset = parse_long(&cursor);

    assert(strncmp(cursor, ", \"type\": \"", 11) == 0);
    cursor += 11;
    *out_signed = (*cursor == 'i');
    assert(*cursor == 'i' || *cursor == 'u');
    cursor++;
    if (strncmp(cursor, "8\"", 2) == 0) {
        *out_width = 1;
    } else if (strncmp(cursor, "16\"", 3) == 0) {
        *out_width = 2;
    } else if (strncmp(cursor, "32\"", 3) == 0) {
        *out_width = 4;
    } else {
        assert(0 && "manifest names a type this reader does not know");
        *out_width = 0;
    }

    cursor = strchr(cursor, ',');
    assert(cursor != NULL);
    if (strncmp(cursor, ", \"count\": ", 11) == 0) {
        cursor += 11;
        *out_count = parse_long(&cursor);
    } else {
        *out_count = 1;
    }
}

/* Decode one element, little-endian, exactly as the manifest's "endian" member
 * says to. */
static long decode(const uint8_t *record, long offset, size_t width, int is_signed, long index)
{
    const uint8_t *p = record + offset + ((long)width * index);
    unsigned long  raw = 0;
    size_t         i;

    for (i = 0; i < width; i++) {
        raw |= ((unsigned long)p[i]) << (8u * i);
    }

    if (is_signed) {
        const unsigned long sign_bit = 1ul << ((8u * width) - 1u);
        if ((raw & sign_bit) != 0ul) {
            return (long)raw - (long)(sign_bit << 1);
        }
    }
    return (long)raw;
}

static long read_field(const char *json, const uint8_t *record, const char *name, long index)
{
    long   offset;
    size_t width;
    int    is_signed;
    long   count;

    manifest_field(json, name, &offset, &width, &is_signed, &count);
    assert(index < count);
    return decode(record, offset, width, is_signed, index);
}

/* The claim: a reader that has only the manifest decodes the packer's bytes
 * correctly. Every value below is recovered using the offset and type the
 * manifest published, never the offset in the specification or in the field
 * table, so a manifest that drifted from the packer fails here. */
static void test_manifest_decodes_the_packed_record(void)
{
    text_sink_t       store;
    oa_sink_t         sink;
    oa_log_manifest_t m;
    oa_log_flight_t   rec;
    uint8_t           packed[OA_LOG_FLIGHT_RECORD_BYTES];
    long              record_bytes;
    const char       *at;

    text_sink_init(&store, &sink);
    fill_track_manifest(&m);
    assert(oa_log_write_manifest(&m, NULL, &sink) == OA_OK);

    memset(&rec, 0, sizeof rec);
    rec.t_ms           = 16909060u;
    rec.pressure_pa    = 98765;
    rec.temp_dc        = -123;
    rec.accel_mg[0]    = 1001;
    rec.accel_mg[1]    = -1002;
    rec.accel_mg[2]    = 1003;
    rec.gyro_cdps[0]   = 2001;
    rec.gyro_cdps[1]   = 2002;
    rec.gyro_cdps[2]   = -2003;
    rec.hg_accel_dg[0] = -3001;
    rec.hg_accel_dg[1] = 3002;
    rec.hg_accel_dg[2] = 3003;
    rec.alt_cm         = -654321;
    rec.vel_dm_s       = 4321;
    rec.state          = 5;
    rec.flags          = 0x41u;

    assert(oa_log_pack_flight(&rec, packed, sizeof packed) == OA_OK);

    /* The reader sizes the array from record_bytes in the manifest, so check the
     * packer wrote exactly that many bytes. */
    at = strstr(flight_section(store.buf), "\"record_bytes\": ");
    assert(at != NULL);
    at += strlen("\"record_bytes\": ");
    record_bytes = parse_long(&at);
    assert((size_t)record_bytes == sizeof packed);
    assert((size_t)record_bytes == oa_log_stream_record_bytes(OA_LOG_STREAM_FLIGHT));

    assert(read_field(store.buf, packed, "t_ms", 0) == 16909060L);
    assert(read_field(store.buf, packed, "pressure_pa", 0) == 98765L);
    assert(read_field(store.buf, packed, "temp_dc", 0) == -123L);
    assert(read_field(store.buf, packed, "accel_mg", 0) == 1001L);
    assert(read_field(store.buf, packed, "accel_mg", 1) == -1002L);
    assert(read_field(store.buf, packed, "accel_mg", 2) == 1003L);
    assert(read_field(store.buf, packed, "gyro_cdps", 0) == 2001L);
    assert(read_field(store.buf, packed, "gyro_cdps", 1) == 2002L);
    assert(read_field(store.buf, packed, "gyro_cdps", 2) == -2003L);
    assert(read_field(store.buf, packed, "hg_accel_dg", 0) == -3001L);
    assert(read_field(store.buf, packed, "hg_accel_dg", 1) == 3002L);
    assert(read_field(store.buf, packed, "hg_accel_dg", 2) == 3003L);
    assert(read_field(store.buf, packed, "alt_cm", 0) == -654321L);
    assert(read_field(store.buf, packed, "vel_dm_s", 0) == 4321L);
    assert(read_field(store.buf, packed, "state", 0) == 5L);
    assert(read_field(store.buf, packed, "flags", 0) == 0x41L);

    /* Every byte of the record is claimed by exactly one field the manifest
     * describes. A gap would be a byte a reader cannot interpret; an overlap
     * would be two fields decoding the same bytes, and both are the drift this
     * arrangement exists to prevent. */
    {
        static const char *const names[] = { "t_ms",        "pressure_pa", "temp_dc",
                                             "accel_mg",    "gyro_cdps",   "hg_accel_dg",
                                             "alt_cm",      "vel_dm_s",    "state",
                                             "flags" };
        uint8_t claimed[OA_LOG_FLIGHT_RECORD_BYTES];
        size_t  i;
        size_t  f;

        memset(claimed, 0, sizeof claimed);
        for (f = 0; f < sizeof names / sizeof names[0]; f++) {
            long   offset;
            size_t width;
            int    is_signed;
            long   count;
            long   byte;

            manifest_field(store.buf, names[f], &offset, &width, &is_signed, &count);
            for (byte = 0; byte < count * (long)width; byte++) {
                const size_t index = (size_t)(offset + byte);
                assert(index < sizeof claimed);
                assert(claimed[index] == 0u); /* overlap */
                claimed[index] = 1u;
            }
        }
        for (i = 0; i < sizeof claimed; i++) {
            assert(claimed[i] == 1u); /* gap */
        }
    }
}

int main(void)
{
    test_creation_time_manifest_matches_the_spec();
    test_no_gnss_stream_on_a_build_without_one();
    test_summary_scaling_is_exact();
    test_nominal_hz_round_trip();
    test_strings_are_escaped();
    test_manifest_decodes_the_packed_record();

    printf("oa_log_manifest: meta.json matches docs/spec/log-format.md and decodes the packer's bytes\n");
    return 0;
}

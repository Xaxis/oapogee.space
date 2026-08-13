/*
 * Conformance test: oa_log_record.c.
 *
 * Every offset, width and sign convention checked below is restated here by
 * hand from docs/spec/log-format.md, deliberately. The packer generates its
 * layout from oa_log_fields.def; if this test expanded that table too, it would
 * only be checking the table against itself and a wrong table would pass. The
 * specification's two tables, "flight.bin record, 36 bytes" and "gnss.bin
 * record, 16 bytes", are typed out again here so that the table and the
 * normative document are compared against each other by something.
 *
 * Plain C and assert, no framework. Run it and look at the exit status.
 *
 * NOTHING UNDER TEST HAS RUN ON HARDWARE. These tests prove the packer agrees
 * with a specification. They prove nothing about a flash part, a filesystem, or
 * a flight, because no board exists and no flight has been logged.
 */

#include "oapogee/oa_log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * A sink over a fixed buffer, and readers that decode little-endian by hand.
 *
 * The readers are written out rather than memcpy'd over a struct for the same
 * reason the packer is: a struct overlay would test whether this host is
 * little-endian, not whether the packer wrote little-endian.
 * ------------------------------------------------------------------------ */

typedef struct {
    uint8_t buf[512];
    size_t  used;
    size_t  cap;   /* Artificial limit, so the full case can be exercised. */
    size_t  syncs;
} buf_sink_t;

static oa_result_t buf_write(void *ctx, const uint8_t *data, size_t len)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    if (s->used + len > s->cap) {
        return OA_ERR_SINK_FULL;
    }
    memcpy(s->buf + s->used, data, len);
    s->used += len;
    return OA_OK;
}

static size_t buf_space(void *ctx)
{
    const buf_sink_t *s = (const buf_sink_t *)ctx;
    return s->cap - s->used;
}

static oa_result_t buf_sync(void *ctx)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    s->syncs++;
    return OA_OK;
}

static const oa_sink_vtable_t buf_vt = { buf_write, buf_space, buf_sync };

static void buf_sink_init(buf_sink_t *s, oa_sink_t *sink, size_t cap)
{
    memset(s, 0, sizeof *s);
    s->cap    = cap;
    sink->vt  = &buf_vt;
    sink->ctx = s;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t rd_i32(const uint8_t *p)
{
    return (int32_t)rd_u32(p);
}

static int16_t rd_i16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* ---------------------------------------------------------------------------
 * Tests.
 * ------------------------------------------------------------------------ */

/* Spec claim: "flight.bin record, 36 bytes" and "gnss.bin record, 16 bytes",
 * and the directory layout names those two files. A record width that drifted
 * would shift every record in the file against the manifest describing it. */
static void test_record_widths(void)
{
    assert(OA_LOG_FLIGHT_RECORD_BYTES == 36);
    assert(OA_LOG_GNSS_RECORD_BYTES == 16);

    assert(oa_log_stream_record_bytes(OA_LOG_STREAM_FLIGHT) == 36u);
    assert(oa_log_stream_record_bytes(OA_LOG_STREAM_GNSS) == 16u);

    assert(strcmp(oa_log_stream_filename(OA_LOG_STREAM_FLIGHT), "flight.bin") == 0);
    assert(strcmp(oa_log_stream_filename(OA_LOG_STREAM_GNSS), "gnss.bin") == 0);

    /* A value that is not a stream returns nothing usable rather than something
     * printable, so a caller cannot name a file that will never be created. */
    assert(oa_log_stream_filename(OA_LOG_STREAM_COUNT) == NULL);
    assert(oa_log_stream_record_bytes(OA_LOG_STREAM_COUNT) == 0u);
}

/* Spec claim: the flight.bin field table. Offsets 0, 4, 8, 10, 16, 22, 28, 32,
 * 34, 35, little-endian, with three-element arrays for the accelerometer, the
 * gyroscope and the high-g part.
 *
 * Every field is given a distinct value, so a field written at another field's
 * offset fails here rather than being masked by a shared value. */
static void test_flight_field_offsets(void)
{
    oa_log_flight_t rec;
    uint8_t         out[OA_LOG_FLIGHT_RECORD_BYTES];

    memset(&rec, 0, sizeof rec);
    rec.t_ms           = 0x01020304u;
    rec.pressure_pa    = 101325;
    rec.temp_dc        = 214;
    rec.accel_mg[0]    = 1001;
    rec.accel_mg[1]    = 1002;
    rec.accel_mg[2]    = 1003;
    rec.gyro_cdps[0]   = 2001;
    rec.gyro_cdps[1]   = 2002;
    rec.gyro_cdps[2]   = 2003;
    rec.hg_accel_dg[0] = 3001;
    rec.hg_accel_dg[1] = 3002;
    rec.hg_accel_dg[2] = 3003;
    rec.alt_cm         = 123456;
    rec.vel_dm_s       = 4321;
    rec.state          = 4; /* APOGEE, from data/flight-phases.yaml. */
    rec.flags          = 0x21u;

    assert(oa_log_pack_flight(&rec, out, sizeof out) == OA_OK);

    assert(rd_u32(out + 0) == 0x01020304u);
    assert(rd_i32(out + 4) == 101325);
    assert(rd_i16(out + 8) == 214);
    assert(rd_i16(out + 10) == 1001);
    assert(rd_i16(out + 12) == 1002);
    assert(rd_i16(out + 14) == 1003);
    assert(rd_i16(out + 16) == 2001);
    assert(rd_i16(out + 18) == 2002);
    assert(rd_i16(out + 20) == 2003);
    assert(rd_i16(out + 22) == 3001);
    assert(rd_i16(out + 24) == 3002);
    assert(rd_i16(out + 26) == 3003);
    assert(rd_i32(out + 28) == 123456);
    assert(rd_i16(out + 32) == 4321);
    assert(out[34] == 4u);
    assert(out[35] == 0x21u);

    /* Little-endian, stated as bytes rather than as a decoded value, because
     * the decoders above would agree with a big-endian packer on a big-endian
     * host and the format is little-endian on every host. */
    assert(out[0] == 0x04u && out[1] == 0x03u && out[2] == 0x02u && out[3] == 0x01u);
}

/* Spec claim: the gnss.bin field table. Offsets 0, 4, 8, 12, 14, 15. */
static void test_gnss_field_offsets(void)
{
    oa_log_gnss_t rec;
    uint8_t       out[OA_LOG_GNSS_RECORD_BYTES];

    memset(&rec, 0, sizeof rec);
    rec.t_ms   = 900u;
    rec.lat_e7 = 515074000;   /* Any coordinate. Nothing has flown. */
    rec.lon_e7 = -1278000;
    rec.alt_m  = 1234;
    rec.sats   = 9u;
    rec.fix    = 3u;          /* u-blox convention: 3 is three-dimensional. */

    assert(oa_log_pack_gnss(&rec, out, sizeof out) == OA_OK);

    assert(rd_u32(out + 0) == 900u);
    assert(rd_i32(out + 4) == 515074000);
    assert(rd_i32(out + 8) == -1278000);
    assert(rd_i16(out + 12) == 1234);
    assert(out[14] == 9u);
    assert(out[15] == 3u);
}

/* Spec claim: alt_cm is signed and negative values are legitimate rather than a
 * bug, because a barometric zero taken on the pad reports negative altitude if
 * the rocket lands below the pad or if pressure rises during the flight. The
 * same applies to a southern or western coordinate in gnss.bin. */
static void test_negative_values_are_twos_complement(void)
{
    oa_log_flight_t rec;
    uint8_t         out[OA_LOG_FLIGHT_RECORD_BYTES];

    memset(&rec, 0, sizeof rec);
    rec.alt_cm   = -1;
    rec.vel_dm_s = -4321;
    rec.temp_dc  = -100;

    assert(oa_log_pack_flight(&rec, out, sizeof out) == OA_OK);
    assert(rd_i32(out + 28) == -1);
    assert(out[28] == 0xFFu && out[29] == 0xFFu && out[30] == 0xFFu && out[31] == 0xFFu);
    assert(rd_i16(out + 32) == -4321);
    assert(rd_i16(out + 8) == -100);
}

/* Header contract rule 4: every byte of the record is written, so a record never
 * carries stale bytes from a previous one. Pack a zeroed record over a buffer
 * full of 0xAA and require that nothing of the 0xAA survives.
 *
 * Today this passes whether or not the packer clears the buffer first, because
 * both records are covered end to end by named fields and every one of them is
 * written. That is the point: this check is what fails the first time a record
 * gains a reserved gap and the clear is not there, which is the first time an
 * unwritten byte becomes a leak of the previous sample into the log. */
static void test_no_stale_bytes(void)
{
    oa_log_flight_t flight;
    oa_log_gnss_t   gnss;
    uint8_t         out[64];
    size_t          i;

    memset(&flight, 0, sizeof flight);
    memset(&gnss, 0, sizeof gnss);

    memset(out, 0xAA, sizeof out);
    assert(oa_log_pack_flight(&flight, out, sizeof out) == OA_OK);
    for (i = 0; i < (size_t)OA_LOG_FLIGHT_RECORD_BYTES; i++) {
        assert(out[i] == 0x00u);
    }
    /* And not one byte past the record, because the caller's buffer past the
     * record length is none of the packer's business. */
    assert(out[OA_LOG_FLIGHT_RECORD_BYTES] == 0xAAu);

    memset(out, 0xAA, sizeof out);
    assert(oa_log_pack_gnss(&gnss, out, sizeof out) == OA_OK);
    for (i = 0; i < (size_t)OA_LOG_GNSS_RECORD_BYTES; i++) {
        assert(out[i] == 0x00u);
    }
    assert(out[OA_LOG_GNSS_RECORD_BYTES] == 0xAAu);
}

/* Header contract rule 3: nothing is written at all if the buffer is too small.
 * A partial record in a flat array of fixed-width records shifts every record
 * after it, and the caller would have no way to know. */
static void test_short_buffer_writes_nothing(void)
{
    oa_log_flight_t rec;
    uint8_t         out[OA_LOG_FLIGHT_RECORD_BYTES];
    size_t          i;

    memset(&rec, 0, sizeof rec);
    rec.t_ms = 0xDEADBEEFu;
    memset(out, 0xAA, sizeof out);

    assert(oa_log_pack_flight(&rec, out, sizeof out - 1u) == OA_ERR_BUFFER);
    for (i = 0; i < sizeof out; i++) {
        assert(out[i] == 0xAAu);
    }

    assert(oa_log_pack_gnss((const oa_log_gnss_t *)NULL, out, sizeof out) == OA_ERR_NULL);
    assert(oa_log_pack_flight(&rec, NULL, sizeof out) == OA_ERR_NULL);
}

/* The write path packs the same bytes the packer produces and appends exactly
 * one record length. Two records back to back are what makes the file the flat
 * array the format promises: floor(file_size / record_bytes) records, loadable
 * in one call. */
static void test_write_appends_whole_records(void)
{
    buf_sink_t      store;
    oa_sink_t       sink;
    oa_log_flight_t rec;
    uint8_t         expect[OA_LOG_FLIGHT_RECORD_BYTES];

    buf_sink_init(&store, &sink, sizeof store.buf);

    memset(&rec, 0, sizeof rec);
    rec.t_ms   = 10u;
    rec.alt_cm = 500;
    assert(oa_log_pack_flight(&rec, expect, sizeof expect) == OA_OK);

    assert(oa_log_write_flight(&rec, &sink) == OA_OK);
    assert(store.used == (size_t)OA_LOG_FLIGHT_RECORD_BYTES);
    assert(memcmp(store.buf, expect, sizeof expect) == 0);

    rec.t_ms   = 20u;
    rec.alt_cm = 1000;
    assert(oa_log_pack_flight(&rec, expect, sizeof expect) == OA_OK);
    assert(oa_log_write_flight(&rec, &sink) == OA_OK);
    assert(store.used == 2u * (size_t)OA_LOG_FLIGHT_RECORD_BYTES);
    assert(memcmp(store.buf + OA_LOG_FLIGHT_RECORD_BYTES, expect, sizeof expect) == 0);

    assert(oa_log_write_flight(&rec, NULL) == OA_ERR_NULL);
    assert(oa_log_write_flight(NULL, &sink) == OA_ERR_NULL);
}

/* Spec claim: a reader must treat a trailing partial record as absent, and that
 * case is meant to describe a payload that lost power on impact. A store that
 * fills up must therefore stop on a record boundary rather than write half a
 * record, so a record that does not fit is not started at all.
 *
 * OA_ERR_SINK_FULL is what the flight code turns into OA_FLAG_LOG_FULL. A flight
 * does not end because the flash filled up. */
static void test_full_sink_stops_on_a_record_boundary(void)
{
    buf_sink_t      store;
    oa_sink_t       sink;
    oa_log_flight_t rec;

    /* Room for one record and most of a second. */
    buf_sink_init(&store, &sink, (size_t)OA_LOG_FLIGHT_RECORD_BYTES + 20u);

    memset(&rec, 0, sizeof rec);
    assert(oa_log_write_flight(&rec, &sink) == OA_OK);
    assert(store.used == (size_t)OA_LOG_FLIGHT_RECORD_BYTES);

    assert(oa_log_write_flight(&rec, &sink) == OA_ERR_SINK_FULL);
    assert(store.used % (size_t)OA_LOG_FLIGHT_RECORD_BYTES == 0u);
    assert(store.used == (size_t)OA_LOG_FLIGHT_RECORD_BYTES);
}

/* CLAIM, from oa_log.h: a GNSS altitude outside the range of an i16 of whole
 * metres is "clamped at the endpoints rather than wrapping", because a wrapped
 * altitude decodes as a plausible value with the wrong sign rather than as an
 * obvious fault. */
static void test_altitude_clamps_rather_than_wraps(void)
{
    assert(oa_log_clamp_altitude_m(0) == 0);
    assert(oa_log_clamp_altitude_m(1234) == 1234);
    assert(oa_log_clamp_altitude_m(-500) == -500);

    assert(oa_log_clamp_altitude_m(INT16_MAX) == INT16_MAX);
    assert(oa_log_clamp_altitude_m(INT16_MIN) == INT16_MIN);

    /* The cases that matter. A plain narrowing cast turns 32768 into -32768,
     * which is a deep valley rather than a high mountain. */
    assert(oa_log_clamp_altitude_m(INT16_MAX + 1) == INT16_MAX);
    assert(oa_log_clamp_altitude_m(INT16_MIN - 1) == INT16_MIN);
    assert(oa_log_clamp_altitude_m(INT32_MAX) == INT16_MAX);
    assert(oa_log_clamp_altitude_m(INT32_MIN) == INT16_MIN);
}

int main(void)
{
    test_altitude_clamps_rather_than_wraps();
    test_record_widths();
    test_flight_field_offsets();
    test_gnss_field_offsets();
    test_negative_values_are_twos_complement();
    test_no_stale_bytes();
    test_short_buffer_writes_nothing();
    test_write_appends_whole_records();
    test_full_sink_stops_on_a_record_boundary();

    printf("oa_log_record: all record layout checks agree with docs/spec/log-format.md\n");
    return 0;
}

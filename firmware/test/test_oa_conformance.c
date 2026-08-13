/*
 * oApogee: THE CONFORMANCE TEST.
 *
 * Every other test in this directory checks a module against its own header.
 * This one checks the firmware against the two specifications, and it is the one
 * to read first when several tests fail at once.
 *
 *   docs/spec/telemetry-packet.md   the wire format, spec_version 1
 *   docs/spec/log-format.md         the onboard log format, spec_version 1
 *   data/flight-phases.yaml         the flight states and their transitions
 *
 * THE ONE RULE THIS FILE IS BUILT ON
 *
 * Nothing here is expanded from oa_packet_fields.def or oa_log_fields.def.
 *
 * Those tables are how the firmware avoids disagreeing with itself, and they do
 * that job well: the encoder, the body structs and the manifest all come from
 * one line each, so they cannot drift apart. But that is exactly why a test
 * expanded from them proves nothing about whether they match the specification.
 * A digit mistyped in the .def file would move the field, move the struct, move
 * the manifest, and move the test's expectation, all together, and every
 * assertion would still pass while the payload transmitted a packet no receiver
 * could read.
 *
 * So every offset, length, type, scale and name below is transcribed by hand
 * from the tables in those two documents, and the decoder in the second half is
 * transcribed from the reference decoder the packet spec publishes rather than
 * derived from the encoder. Two independent statements of the same format. If
 * they disagree, one of them is wrong, and this file is where that is found out.
 *
 * Where this file and a specification disagree, the specification is right.
 *
 * NOTHING HERE HAS RUN ON HARDWARE. No board has been fabricated, no packet has
 * been transmitted, no flight has been logged, and no threshold in this firmware
 * has been measured. Every number this test feeds in is a fixture chosen to make
 * the arithmetic checkable, and not one of them ships.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oa_states.h"
#include "oapogee/oa_battery.h"
#include "oapogee/oa_config.h"
#include "oapogee/oa_crc.h"
#include "oapogee/oa_flags.h"
#include "oapogee/oa_log.h"
#include "oapogee/oa_packet.h"
#include "oapogee/oa_state.h"

/* ===========================================================================
 * PART ONE: the specifications, transcribed by hand.
 *
 * Read these against the documents. That is the whole point of them being here
 * rather than generated.
 * ======================================================================== */

/* docs/spec/telemetry-packet.md, "Packet types":
 *
 *   | Type | Name     | Body | Total |
 *   | 0x1  | STATUS   | 3 B  | 13 B  |
 *   | 0x2  | FLIGHT   | 9 B  | 19 B  |
 *   | 0x3  | APOGEE   | 8 B  | 18 B  |
 *   | 0x4  | BEACON   | 12 B | 22 B  |
 *   | 0x5  | POSITION | 9 B  | 19 B  |
 */
typedef struct {
    const char *name;
    int         code;
    size_t      body_bytes;
    size_t      total_bytes;
} spec_packet_type_t;

static const spec_packet_type_t k_spec_packet_types[] = {
    { "STATUS",   0x1,  3u, 13u },
    { "FLIGHT",   0x2,  9u, 19u },
    { "APOGEE",   0x3,  8u, 18u },
    { "BEACON",   0x4, 12u, 22u },
    { "POSITION", 0x5,  9u, 19u },
};

#define SPEC_PACKET_TYPE_COUNT (sizeof k_spec_packet_types / sizeof k_spec_packet_types[0])

/* docs/spec/telemetry-packet.md, "Header, 8 bytes":
 *
 *   | 0 | 1 | u8  | hdr   |
 *   | 1 | 1 | u8  | flags |
 *   | 2 | 1 | u8  | seq   |
 *   | 3 | 1 | u8  | state |
 *   | 4 | 4 | u32 | t_ms  |
 */
#define SPEC_OFF_HDR   (0u)
#define SPEC_OFF_FLAGS (1u)
#define SPEC_OFF_SEQ   (2u)
#define SPEC_OFF_STATE (3u)
#define SPEC_OFF_T_MS  (4u)
#define SPEC_HEADER_BYTES (8u)

/* "The last two bytes of every packet are a CRC-16/CCITT-FALSE over all
 * preceding bytes of the packet, transmitted little-endian." */
#define SPEC_CRC_BYTES (2u)

/* "Version 1 with a FLIGHT packet is 0x12." */
#define SPEC_VERSION (1u)

/* Body field offsets, one block per type, from the tables in the packet spec.
 * Offsets are bytes from the start of the packet, as the spec states them. */

/* 0x1 STATUS: | 8 | 2 | u16 | pad_pressure_pa_off | ; | 10 | 1 | u8 | batt | */
#define SPEC_STATUS_OFF_PAD_PRESSURE (8u)
#define SPEC_STATUS_OFF_BATT         (10u)

/* 0x2 FLIGHT: alt_cm i32 @8, vel_dm_s i16 @12, accel_cg i16 @14, batt u8 @16 */
#define SPEC_FLIGHT_OFF_ALT_CM   (8u)
#define SPEC_FLIGHT_OFF_VEL_DM_S (12u)
#define SPEC_FLIGHT_OFF_ACCEL_CG (14u)
#define SPEC_FLIGHT_OFF_BATT     (16u)

/* 0x3 APOGEE: apogee_cm i32 @8, t_apogee_ms u32 @12 */
#define SPEC_APOGEE_OFF_APOGEE_CM   (8u)
#define SPEC_APOGEE_OFF_T_APOGEE_MS (12u)

/* 0x4 BEACON: lat_e7 i32 @8, lon_e7 i32 @12, apogee_cm i32 @16 */
#define SPEC_BEACON_OFF_LAT_E7    (8u)
#define SPEC_BEACON_OFF_LON_E7    (12u)
#define SPEC_BEACON_OFF_APOGEE_CM (16u)

/* 0x5 POSITION: lat_e7 i32 @8, lon_e7 i32 @12, sats u8 @16 */
#define SPEC_POSITION_OFF_LAT_E7 (8u)
#define SPEC_POSITION_OFF_LON_E7 (12u)
#define SPEC_POSITION_OFF_SATS   (16u)

/* "the transmitted value is pressure_pa - 50000, and the 65536 representable
 * values cover 50000 to 115535 Pa [...] A reading below 50000 Pa is transmitted
 * as 0, a reading above 115535 Pa is transmitted as 65535, and BARO_FAULT is set
 * in either case." */
#define SPEC_PAD_PRESSURE_OFFSET (50000)
#define SPEC_PAD_PRESSURE_MIN    (50000)
#define SPEC_PAD_PRESSURE_MAX    (115535)

/* "One byte, battery_volts = 2.5 + batt / 100. Range 2.50 V to 5.05 V in 10 mV
 * steps." */
#define SPEC_BATTERY_MIN_MV  (2500)
#define SPEC_BATTERY_MAX_MV  (5050)
#define SPEC_BATTERY_STEP_MV (10)

/* "lat_e7 and lon_e7 carry INT32_MIN in both BEACON and POSITION" when there is
 * no fix. Written out rather than taken from the header, because the sentinel is
 * part of the format a receiver implements. */
#define SPEC_NO_FIX (INT32_MIN)

/* docs/spec/telemetry-packet.md, the flags table. Transcribed with the mask
 * column as published, so that a mask edited in oa_flags.h fails here. */
typedef struct {
    const char *name;
    uint8_t     mask;
} spec_flag_t;

static const spec_flag_t k_spec_flags[] = {
    { "GNSS_FIX",   0x01u }, { "HIGH_G",   0x02u },
    { "BARO_FAULT", 0x04u }, { "IMU_FAULT", 0x08u },
    { "LOG_FULL",   0x10u }, { "LOW_BATT", 0x20u },
    { "SIM",        0x40u },
    /* Bit 7 is "reserved. Must be transmitted as 0 and ignored on receive." */
};

#define SPEC_FLAG_COUNT (sizeof k_spec_flags / sizeof k_spec_flags[0])
#define SPEC_FLAG_RESERVED (0x80u)

/* docs/spec/telemetry-packet.md, "Flight state", and data/flight-phases.yaml
 * `order`. The same enumeration serves the packet header, the log record and the
 * homepage animation, and this is the third independent copy of it. */
typedef struct {
    const char *id;
    int         order;
} spec_state_t;

static const spec_state_t k_spec_states[] = {
    { "PAD_IDLE", 0 }, { "ARMED",   1 }, { "BOOST",  2 }, { "COAST", 3 },
    { "APOGEE",   4 }, { "DESCENT", 5 }, { "LANDED", 6 },
};

#define SPEC_STATE_COUNT (sizeof k_spec_states / sizeof k_spec_states[0])

/* docs/spec/log-format.md, "flight.bin record, 36 bytes" and "gnss.bin record,
 * 16 bytes", plus the scale and unit columns from the meta.json example in the
 * same document. Transcribed as one table so that the manifest can be checked
 * against it field for field. */
typedef struct {
    const char *name;
    size_t      offset;
    const char *type;
    size_t      count; /* 1 for a scalar */
    const char *scale;
    const char *unit;
} spec_log_field_t;

static const spec_log_field_t k_spec_flight_fields[] = {
    { "t_ms",        0u,  "u32", 1u, "0.001", "s"     },
    { "pressure_pa", 4u,  "i32", 1u, "1",     "Pa"    },
    { "temp_dc",     8u,  "i16", 1u, "0.1",   "degC"  },
    { "accel_mg",    10u, "i16", 3u, "0.001", "g"     },
    { "gyro_cdps",   16u, "i16", 3u, "0.01",  "deg/s" },
    { "hg_accel_dg", 22u, "i16", 3u, "0.1",   "g"     },
    { "alt_cm",      28u, "i32", 1u, "0.01",  "m"     },
    { "vel_dm_s",    32u, "i16", 1u, "0.1",   "m/s"   },
    { "state",       34u, "u8",  1u, "1",     "enum"  },
    { "flags",       35u, "u8",  1u, "1",     "bits"  },
};

static const spec_log_field_t k_spec_gnss_fields[] = {
    { "t_ms",   0u,  "u32", 1u, "0.001", "s"     },
    { "lat_e7", 4u,  "i32", 1u, "1e-7",  "deg"   },
    { "lon_e7", 8u,  "i32", 1u, "1e-7",  "deg"   },
    { "alt_m",  12u, "i16", 1u, "1",     "m"     },
    { "sats",   14u, "u8",  1u, "1",     "count" },
    { "fix",    15u, "u8",  1u, "1",     "enum"  },
};

#define SPEC_FLIGHT_FIELD_COUNT (sizeof k_spec_flight_fields / sizeof k_spec_flight_fields[0])
#define SPEC_GNSS_FIELD_COUNT   (sizeof k_spec_gnss_fields / sizeof k_spec_gnss_fields[0])

#define SPEC_FLIGHT_RECORD_BYTES (36u)
#define SPEC_GNSS_RECORD_BYTES   (16u)
#define SPEC_FLIGHT_FILENAME     "flight.bin"
#define SPEC_GNSS_FILENAME       "gnss.bin"

/* ===========================================================================
 * PART TWO: an independent implementation of the format.
 *
 * Transcribed from the reference decoder printed in docs/spec/telemetry-packet.md,
 * line by line, and from the prose that says integers are little-endian and
 * two's complement. Nothing in this section calls into the firmware.
 * ======================================================================== */

/* From the spec's Python:
 *
 *     crc = 0xFFFF
 *     for byte in data:
 *         crc ^= byte << 8
 *         for _ in range(8):
 *             crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
 */
static uint16_t ref_crc16(const uint8_t *data, size_t len)
{
    unsigned crc = 0xFFFFu;
    size_t   i;

    for (i = 0u; i < len; i++) {
        int bit;

        crc ^= (unsigned)data[i] << 8;
        for (bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (((crc << 1) ^ 0x1021u) & 0xFFFFu) : ((crc << 1) & 0xFFFFu);
        }
    }

    return (uint16_t)crc;
}

/* "All multi-byte integers are little-endian [...] Integers are two's complement
 * where signed." Assembled from bytes rather than cast through a pointer, so
 * these readers are correct on a big-endian host too, which is the property the
 * spec asks a portable decoder to have. */
static uint16_t ref_u16(const uint8_t *p, size_t off)
{
    return (uint16_t)((unsigned)p[off] | ((unsigned)p[off + 1u] << 8));
}

static uint32_t ref_u32(const uint8_t *p, size_t off)
{
    return (uint32_t)p[off] | ((uint32_t)p[off + 1u] << 8) | ((uint32_t)p[off + 2u] << 16)
           | ((uint32_t)p[off + 3u] << 24);
}

/* The sign reconstruction is written out rather than done with a cast. Casting an
 * out-of-range unsigned to a signed type is implementation-defined in C11, and a
 * decoder in a specification test should not depend on the compiler agreeing with
 * the specification about what two's complement means. */
static int16_t ref_i16(const uint8_t *p, size_t off)
{
    const uint16_t u = ref_u16(p, off);

    return (u < 0x8000u) ? (int16_t)u : (int16_t)((int32_t)u - 65536);
}

static int32_t ref_i32(const uint8_t *p, size_t off)
{
    const uint32_t u = ref_u32(p, off);

    if (u < 0x80000000u) {
        return (int32_t)u;
    }

    return (int32_t)(u - 0x80000000u) - 2147483647 - 1;
}

/* The decoded form of any packet, following the dict the reference decoder
 * returns. Members not carried by a given type are left untouched. */
typedef struct {
    unsigned version;
    unsigned ptype;
    uint8_t  flags;
    bool     gnss_fix, high_g, baro_fault, imu_fault, log_full, low_batt, simulated;
    uint8_t  seq;
    uint8_t  state;
    uint32_t t_ms;

    int32_t  pad_pressure_pa;
    uint8_t  batt;
    int32_t  alt_cm;
    int16_t  vel_dm_s;
    int16_t  accel_cg;
    int32_t  apogee_cm;
    uint32_t t_apogee_ms;
    int32_t  lat_e7;
    int32_t  lon_e7;
    uint8_t  sats;
    bool     no_fix; /* the decoder's own verdict, from the INT32_MIN sentinel */
} ref_packet_t;

/* Returns NULL on success, or the message the reference decoder would raise.
 * Structured the same way as the Python, including the order of the checks, so
 * that a reader can hold the two side by side. */
static const char *ref_decode(const uint8_t *packet, size_t len, ref_packet_t *out)
{
    static const size_t body_len[6] = { 0u, 3u, 9u, 8u, 12u, 9u };
    size_t              expected;
    uint16_t            received_crc;

    memset(out, 0, sizeof *out);

    if (len < 10u) {
        return "short packet";
    }

    out->version = (unsigned)packet[0] >> 4;
    out->ptype   = (unsigned)packet[0] & 0x0Fu;

    if (out->version != SPEC_VERSION) {
        return "unsupported version";
    }
    if (out->ptype < 1u || out->ptype > 5u) {
        return "unknown packet type";
    }

    expected = SPEC_HEADER_BYTES + body_len[out->ptype] + SPEC_CRC_BYTES;
    if (len != expected) {
        return "wrong length for this type";
    }

    received_crc = ref_u16(packet, len - 2u);
    if (ref_crc16(packet, len - 2u) != received_crc) {
        return "CRC mismatch";
    }

    out->flags     = packet[SPEC_OFF_FLAGS];
    out->gnss_fix  = (out->flags & 0x01u) != 0u;
    out->high_g    = (out->flags & 0x02u) != 0u;
    out->baro_fault = (out->flags & 0x04u) != 0u;
    out->imu_fault = (out->flags & 0x08u) != 0u;
    out->log_full  = (out->flags & 0x10u) != 0u;
    out->low_batt  = (out->flags & 0x20u) != 0u;
    out->simulated = (out->flags & 0x40u) != 0u;
    out->seq       = packet[SPEC_OFF_SEQ];
    out->state     = packet[SPEC_OFF_STATE];
    out->t_ms      = ref_u32(packet, SPEC_OFF_T_MS);

    switch (out->ptype) {
        case 1u: /* STATUS */
            out->pad_pressure_pa =
                SPEC_PAD_PRESSURE_OFFSET + (int32_t)ref_u16(packet, SPEC_STATUS_OFF_PAD_PRESSURE);
            out->batt = packet[SPEC_STATUS_OFF_BATT];
            break;

        case 2u: /* FLIGHT */
            out->alt_cm   = ref_i32(packet, SPEC_FLIGHT_OFF_ALT_CM);
            out->vel_dm_s = ref_i16(packet, SPEC_FLIGHT_OFF_VEL_DM_S);
            out->accel_cg = ref_i16(packet, SPEC_FLIGHT_OFF_ACCEL_CG);
            out->batt     = packet[SPEC_FLIGHT_OFF_BATT];
            break;

        case 3u: /* APOGEE */
            out->apogee_cm   = ref_i32(packet, SPEC_APOGEE_OFF_APOGEE_CM);
            out->t_apogee_ms = ref_u32(packet, SPEC_APOGEE_OFF_T_APOGEE_MS);
            break;

        case 4u: /* BEACON */
            out->lat_e7    = ref_i32(packet, SPEC_BEACON_OFF_LAT_E7);
            out->lon_e7    = ref_i32(packet, SPEC_BEACON_OFF_LON_E7);
            out->apogee_cm = ref_i32(packet, SPEC_BEACON_OFF_APOGEE_CM);
            out->no_fix    = (out->lat_e7 == SPEC_NO_FIX) || (out->lon_e7 == SPEC_NO_FIX);
            break;

        case 5u: /* POSITION */
            out->lat_e7 = ref_i32(packet, SPEC_POSITION_OFF_LAT_E7);
            out->lon_e7 = ref_i32(packet, SPEC_POSITION_OFF_LON_E7);
            out->sats   = packet[SPEC_POSITION_OFF_SATS];
            out->no_fix = (out->lat_e7 == SPEC_NO_FIX) || (out->lon_e7 == SPEC_NO_FIX);
            break;

        default:
            /* Unreachable, and it exists for the reason the spec's own decoder
             * gives: adding a type to the length table without a body branch
             * should fail loudly rather than return a header-only decode that
             * looks like a packet with no body. */
            return "no body decoder for this type";
    }

    return NULL;
}

static bool str_eq(const char *a, const char *b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

/* ===========================================================================
 * PART THREE: fixtures.
 *
 * Every value below exists to make an assertion checkable. None is a
 * measurement, none is a proposal, and none of them ships.
 * ======================================================================== */

/* A header with every byte distinguishable, so that a field written to the wrong
 * offset lands on a value that is obviously not its own. State is BOOST, which is
 * not PAD_IDLE, so t_ms is carried rather than forced to zero. */
static oa_packet_header_t fixture_header(void)
{
    oa_packet_header_t hdr;

    hdr.flags = (uint8_t)(OA_FLAG_GNSS_FIX | OA_FLAG_HIGH_G | OA_FLAG_SIM);
    hdr.seq   = 0xA5u;
    hdr.state = (uint8_t)OA_STATE_BOOST;
    hdr.t_ms  = 0x11223344u;

    return hdr;
}

/* ---------------------------------------------------------------------------
 * A sink over a fixed buffer, which is the whole reason oa_sink_t exists: the
 * same packing code writes to a laptop's memory here and to a flash part on a
 * board that has not been fabricated.
 * ------------------------------------------------------------------------ */

typedef struct {
    char   buf[16384];
    size_t len;
} buf_sink_t;

static oa_result_t buf_write(void *ctx, const uint8_t *data, size_t len)
{
    buf_sink_t *s = (buf_sink_t *)ctx;

    if (s->len + len > sizeof s->buf) {
        return OA_ERR_SINK_FULL;
    }
    memcpy(s->buf + s->len, data, len);
    s->len += len;

    return OA_OK;
}

static size_t buf_space(void *ctx)
{
    const buf_sink_t *s = (const buf_sink_t *)ctx;

    return sizeof s->buf - s->len;
}

static oa_result_t buf_sync(void *ctx)
{
    (void)ctx;
    return OA_OK;
}

static const oa_sink_vtable_t buf_vt = { buf_write, buf_space, buf_sync };

/* ===========================================================================
 * PART FOUR: the tests.
 * ======================================================================== */

/* CLAIM: the packet type table. "Total length is fixed per packet type, so a
 * receiver knows how many bytes to expect from the type field alone."
 *
 * Checked against the hand-transcribed table above, so a length edited in
 * oa_packet_fields.def stops agreeing with the document. The header's own
 * _Static_assert already checks that header + body + CRC equals the total; what
 * it cannot check is whether either number is the one the spec published. */
static void test_packet_lengths_match_the_spec_table(void)
{
    size_t i;

    for (i = 0u; i < SPEC_PACKET_TYPE_COUNT; i++) {
        const spec_packet_type_t *s    = &k_spec_packet_types[i];
        const oa_packet_type_t    type = (oa_packet_type_t)s->code;

        assert(oa_packet_body_bytes(type) == s->body_bytes);
        assert(oa_packet_total_bytes(type) == s->total_bytes);
        assert(str_eq(oa_packet_type_name(type), s->name));

        /* The arithmetic the spec states twice, checked once here. */
        assert(s->body_bytes + SPEC_HEADER_BYTES + SPEC_CRC_BYTES == s->total_bytes);
    }

    /* "Types 0x0 and 0x6 to 0xF are reserved. There are no enumerators for
     * them: a reserved type is not something the payload can be asked to send."
     * Zero rather than an error, because every caller of these is sizing a
     * buffer and zero fails that safely. */
    assert(oa_packet_total_bytes((oa_packet_type_t)0x0) == 0u);
    assert(oa_packet_body_bytes((oa_packet_type_t)0x0) == 0u);
    assert(oa_packet_type_name((oa_packet_type_t)0x0) == NULL);

    assert(oa_packet_total_bytes((oa_packet_type_t)0x6) == 0u);
    assert(oa_packet_total_bytes((oa_packet_type_t)0xFu) == 0u);

    /* "The longest packet in version 1 is BEACON at 22 bytes." A caller sizing a
     * buffer with OA_PACKET_MAX_BYTES must be able to build any type into it. */
    assert(OA_PACKET_MAX_BYTES == 22u);
}

/* CLAIM: "hdr packs version and type into one byte [...] Version 1 with a FLIGHT
 * packet is 0x12." The spec gives one worked example and this is it, plus the
 * same arithmetic for every other type. */
static void test_header_byte_packs_version_and_type(void)
{
    size_t i;

    assert(oa_packet_hdr_byte(1u, OA_PKT_FLIGHT) == 0x12u);

    for (i = 0u; i < SPEC_PACKET_TYPE_COUNT; i++) {
        const spec_packet_type_t *s = &k_spec_packet_types[i];
        const uint8_t expected = (uint8_t)((SPEC_VERSION << 4) | (unsigned)s->code);

        assert(oa_packet_hdr_byte((uint8_t)SPEC_VERSION, (oa_packet_type_t)s->code) == expected);
    }
}

/* CLAIM: the flags table, mask column, and "Bit 7 [...] Must be transmitted as 0
 * and ignored on receive."
 *
 * The same byte appears at offset 1 of every packet and at offset 35 of every
 * flight.bin record, and the two specs state it separately. This is the third
 * statement of it. */
static void test_flag_masks_match_the_spec_table(void)
{
    size_t  i;
    uint8_t union_of_transmitted = 0u;

    for (i = 0u; i < SPEC_FLAG_COUNT; i++) {
        const oa_flag_t flag = (oa_flag_t)k_spec_flags[i].mask;

        assert(str_eq(oa_flag_name(flag), k_spec_flags[i].name));
        assert(oa_flag_description(flag) != NULL);
        assert(oa_flag_description(flag)[0] != '\0');

        /* A bit that survives sanitising is a bit that goes on the air. */
        assert(oa_flags_sanitise(k_spec_flags[i].mask) == k_spec_flags[i].mask);
        union_of_transmitted = (uint8_t)(union_of_transmitted | k_spec_flags[i].mask);
    }

    /* Seven defined bits, and the reserved one is not among them. */
    assert(union_of_transmitted == 0x7Fu);
    assert(oa_flags_sanitise(SPEC_FLAG_RESERVED) == 0u);
    assert(oa_flags_sanitise(0xFFu) == 0x7Fu);

    /* "Returns NULL if flag is not exactly one defined bit, rather than a
     * placeholder string, so that a caller cannot print something that looks
     * like a flag name for a value that is not a flag." */
    assert(oa_flag_name((oa_flag_t)0x03u) == NULL);
    assert(oa_flag_name((oa_flag_t)0x00u) == NULL);
    assert(oa_flag_description((oa_flag_t)0x03u) == NULL);
}

/* CLAIM: "Values match `order` in data/flight-phases.yaml, which is the same
 * enumeration the firmware and the log format use", and "Values 7 to 255 are
 * reserved. A receiver encountering one should display the number rather than
 * guessing, and must not treat it as an error." */
static void test_state_values_match_the_spec_table(void)
{
    size_t i;

    for (i = 0u; i < SPEC_STATE_COUNT; i++) {
        const oa_state_t state = (oa_state_t)k_spec_states[i].order;

        assert(str_eq(oa_state_id(state), k_spec_states[i].id));
        assert(oa_state_name(state) != NULL);
        assert(oa_state_is_defined((uint8_t)k_spec_states[i].order) == true);
    }

    /* The reserved range is not defined and does not have a name. It is also not
     * an error: the packet builders pass an unrecognised state byte through. */
    assert(oa_state_is_defined(7u) == false);
    assert(oa_state_is_defined(255u) == false);
    assert(oa_state_id((oa_state_t)7) == NULL);
    assert(oa_state_name((oa_state_t)255) == NULL);
}

/* Shared by the five per-type tests below: the header is identical in every
 * packet type, so it is checked once per packet rather than once per type. */
static void check_header_bytes(const uint8_t *p, const oa_packet_header_t *hdr, int code)
{
    assert(p[SPEC_OFF_HDR] == (uint8_t)((SPEC_VERSION << 4) | (unsigned)code));
    assert(p[SPEC_OFF_FLAGS] == (uint8_t)(hdr->flags & 0x7Fu));
    assert(p[SPEC_OFF_SEQ] == hdr->seq);
    assert(p[SPEC_OFF_STATE] == hdr->state);
    assert(ref_u32(p, SPEC_OFF_T_MS) == hdr->t_ms);
}

/* Shared: "The last two bytes of every packet are a CRC-16/CCITT-FALSE over all
 * preceding bytes of the packet, transmitted little-endian."
 *
 * Computed by the transcription of the spec's Python, not by oa_crc16, so an
 * error shared between the builder and the firmware's CRC cannot cancel out. */
static void check_crc(const uint8_t *p, size_t len)
{
    const uint16_t expected = ref_crc16(p, len - SPEC_CRC_BYTES);

    assert(p[len - 2u] == (uint8_t)(expected & 0xFFu));
    assert(p[len - 1u] == (uint8_t)(expected >> 8));

    /* Worth stating, because it is a trap: the zero-residue trick does NOT work
     * on a packet in this format. Running the CRC over a whole received packet
     * including its own CRC gives zero only when the CRC was appended most
     * significant byte first, and this format transmits it little-endian, as the
     * spec's own reference decoder shows by unpacking it with "<H" and comparing.
     * A receiver written to check the residue instead would reject every valid
     * packet. Verified here rather than merely asserted. */
    assert(ref_crc16(p, len) != 0u);
    {
        uint8_t swapped[OA_PACKET_MAX_BYTES];

        memcpy(swapped, p, len);
        swapped[len - 2u] = (uint8_t)(expected >> 8);
        swapped[len - 1u] = (uint8_t)(expected & 0xFFu);
        assert(ref_crc16(swapped, len) == 0u);
    }
}

/* CLAIM: the STATUS body table, and the STATUS worked example of the pad
 * pressure offset. */
static void test_status_packet_bytes(void)
{
    const oa_packet_header_t hdr = fixture_header();
    oa_status_body_t         body;
    uint8_t                  p[OA_PACKET_MAX_BYTES];
    size_t                   len = 0u;
    ref_packet_t             dec;

    /* 101325 Pa is standard sea level pressure, quoted in the log format's own
     * meta.json example. Encoded it is 101325 - 50000 = 51325. */
    body.pad_pressure_pa_off = (uint16_t)(101325 - SPEC_PAD_PRESSURE_OFFSET);
    body.batt                = 200u; /* 2.5 + 200/100 = 4.50 V */

    assert(oa_packet_build_status(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(len == 13u);

    check_header_bytes(p, &hdr, 0x1);
    assert(ref_u16(p, SPEC_STATUS_OFF_PAD_PRESSURE) == 51325u);
    assert(p[SPEC_STATUS_OFF_BATT] == 200u);
    check_crc(p, len);

    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.ptype == 1u);
    assert(dec.pad_pressure_pa == 101325);
    assert(dec.batt == 200u);
    assert(dec.seq == hdr.seq);
    assert(dec.state == hdr.state);
    assert(dec.t_ms == hdr.t_ms);
    assert(dec.gnss_fix && dec.high_g && dec.simulated);
    assert(!dec.baro_fault && !dec.imu_fault && !dec.log_full && !dec.low_batt);
}

/* CLAIM: the FLIGHT body table, and "alt_cm is signed, and negative values are
 * legitimate rather than a bug." */
static void test_flight_packet_bytes(void)
{
    const oa_packet_header_t hdr = fixture_header();
    oa_flight_body_t         body;
    uint8_t                  p[OA_PACKET_MAX_BYTES];
    size_t                   len = 0u;
    ref_packet_t             dec;

    /* Negative in all three signed fields: a rocket below the pad, descending,
     * decelerating. A receiver that clamps any of these is hiding a real
     * measurement. */
    body.alt_cm   = -123456;
    body.vel_dm_s = -2048;
    body.accel_cg = -1234;
    body.batt     = 37u;

    assert(oa_packet_build_flight(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(len == 19u);

    check_header_bytes(p, &hdr, 0x2);
    assert(ref_i32(p, SPEC_FLIGHT_OFF_ALT_CM) == -123456);
    assert(ref_i16(p, SPEC_FLIGHT_OFF_VEL_DM_S) == -2048);
    assert(ref_i16(p, SPEC_FLIGHT_OFF_ACCEL_CG) == -1234);
    assert(p[SPEC_FLIGHT_OFF_BATT] == 37u);
    check_crc(p, len);

    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.ptype == 2u);
    assert(dec.alt_cm == -123456);
    assert(dec.vel_dm_s == -2048);
    assert(dec.accel_cg == -1234);
    assert(dec.batt == 37u);

    /* The extremes of each container, since these are the values a saturating
     * sensor or a wrapped intermediate would produce. */
    body.alt_cm   = INT32_MIN;
    body.vel_dm_s = INT16_MIN;
    body.accel_cg = INT16_MAX;
    assert(oa_packet_build_flight(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.alt_cm == INT32_MIN);
    assert(dec.vel_dm_s == INT16_MIN);
    assert(dec.accel_cg == INT16_MAX);
}

/* CLAIM: the APOGEE body table, and "t_apogee_ms is the estimated time of the
 * apogee event, which is earlier than the t_ms in the header of the packet
 * reporting it." */
static void test_apogee_packet_bytes(void)
{
    const oa_packet_header_t hdr = fixture_header();
    oa_apogee_body_t         body;
    uint8_t                  p[OA_PACKET_MAX_BYTES];
    size_t                   len = 0u;
    ref_packet_t             dec;

    body.apogee_cm   = 45678;
    body.t_apogee_ms = hdr.t_ms - 250u; /* the detection lag, whatever it turns out to be */

    assert(oa_packet_build_apogee(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(len == 18u);

    check_header_bytes(p, &hdr, 0x3);
    assert(ref_i32(p, SPEC_APOGEE_OFF_APOGEE_CM) == 45678);
    assert(ref_u32(p, SPEC_APOGEE_OFF_T_APOGEE_MS) == hdr.t_ms - 250u);
    check_crc(p, len);

    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.ptype == 3u);
    assert(dec.apogee_cm == 45678);
    assert(dec.t_apogee_ms == hdr.t_ms - 250u);

    /* The event time is carried separately from the packet time precisely so a
     * receiver can show the difference. It has to be the earlier of the two. */
    assert(dec.t_apogee_ms < dec.t_ms);
}

/* CLAIM: the BEACON body table, and "A Solo or Link build with no GNSS transmits
 * INT32_MIN in both position fields and leaves GNSS_FIX clear. INT32_MIN is used
 * rather than zero because zero is a real coordinate." */
static void test_beacon_packet_bytes(void)
{
    oa_packet_header_t hdr = fixture_header();
    oa_beacon_body_t   body;
    uint8_t            p[OA_PACKET_MAX_BYTES];
    size_t             len = 0u;
    ref_packet_t       dec;

    hdr.state = (uint8_t)OA_STATE_LANDED;

    body.lat_e7    = 515074000;  /* 51.5074 N */
    body.lon_e7    = -1278000;   /* 0.1278 W, and negative, which zero is not */
    body.apogee_cm = 45678;

    assert(oa_packet_build_beacon(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(len == 22u);
    assert(len == OA_PACKET_MAX_BYTES); /* BEACON is the longest type in version 1 */

    check_header_bytes(p, &hdr, 0x4);
    assert(ref_i32(p, SPEC_BEACON_OFF_LAT_E7) == 515074000);
    assert(ref_i32(p, SPEC_BEACON_OFF_LON_E7) == -1278000);
    assert(ref_i32(p, SPEC_BEACON_OFF_APOGEE_CM) == 45678);
    check_crc(p, len);

    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.ptype == 4u);
    assert(dec.lat_e7 == 515074000);
    assert(dec.lon_e7 == -1278000);
    assert(dec.apogee_cm == 45678);
    assert(dec.no_fix == false);

    /* The no-fix case, which is what every Solo and Link build transmits. The
     * apogee still has to arrive: "the beacon repeats the flight's apogee so
     * that a walkaway recovery still yields the number even if the onboard log
     * is unreadable afterwards." */
    hdr.flags   = (uint8_t)(hdr.flags & ~(unsigned)OA_FLAG_GNSS_FIX);
    body.lat_e7 = SPEC_NO_FIX;
    body.lon_e7 = SPEC_NO_FIX;

    assert(oa_packet_build_beacon(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.no_fix == true);
    assert(dec.gnss_fix == false);
    assert(dec.apogee_cm == 45678);
}

/* CLAIM: the POSITION body table, and "When GNSS_FIX is clear, both position
 * fields carry INT32_MIN and sats carries zero." */
static void test_position_packet_bytes(void)
{
    oa_packet_header_t hdr = fixture_header();
    oa_position_body_t body;
    uint8_t            p[OA_PACKET_MAX_BYTES];
    size_t             len = 0u;
    ref_packet_t       dec;

    body.lat_e7 = 515074000;
    body.lon_e7 = -1278000;
    body.sats   = 11u;

    assert(oa_packet_build_position(&hdr, &body, p, sizeof p, &len) == OA_OK);
    assert(len == 19u);

    check_header_bytes(p, &hdr, 0x5);
    assert(ref_i32(p, SPEC_POSITION_OFF_LAT_E7) == 515074000);
    assert(ref_i32(p, SPEC_POSITION_OFF_LON_E7) == -1278000);
    assert(p[SPEC_POSITION_OFF_SATS] == 11u);
    check_crc(p, len);

    assert(ref_decode(p, len, &dec) == NULL);
    assert(dec.ptype == 5u);
    assert(dec.lat_e7 == 515074000);
    assert(dec.lon_e7 == -1278000);
    assert(dec.sats == 11u);

    /* POSITION and FLIGHT are both 19 bytes, which is a coincidence of the
     * format and not a licence to confuse them. The type nibble is what tells
     * them apart, and it is in the byte the CRC covers first. */
    assert(oa_packet_total_bytes(OA_PKT_POSITION) == oa_packet_total_bytes(OA_PKT_FLIGHT));
    assert(p[SPEC_OFF_HDR] == 0x15u);
}

/* CLAIM: "Before arming there is no elapsed time to report, so t_ms is
 * transmitted as 0 in every packet whose state is PAD_IDLE. It is not the
 * power-on uptime: the payload's uptime counter is a firmware implementation
 * detail and is never on the air."
 *
 * Checked on every packet type, because the rule is about the header and the
 * header is in all five. */
static void test_pad_idle_transmits_t_ms_as_zero(void)
{
    oa_packet_header_t hdr = fixture_header();
    uint8_t            p[OA_PACKET_MAX_BYTES];
    size_t             len = 0u;
    ref_packet_t       dec;

    /* A large uptime, which is exactly what a payload sitting on the pad has and
     * exactly what must not appear on the air. */
    hdr.t_ms  = 987654321u;
    hdr.state = (uint8_t)OA_STATE_PAD_IDLE;

    {
        oa_status_body_t body = { 51325u, 200u };
        assert(oa_packet_build_status(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 0u);
    }
    {
        oa_flight_body_t body = { 0, 0, 0, 0u };
        assert(oa_packet_build_flight(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 0u);
    }
    {
        oa_apogee_body_t body = { 0, 0u };
        assert(oa_packet_build_apogee(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 0u);
    }
    {
        oa_beacon_body_t body = { SPEC_NO_FIX, SPEC_NO_FIX, 0 };
        assert(oa_packet_build_beacon(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 0u);
    }
    {
        oa_position_body_t body = { SPEC_NO_FIX, SPEC_NO_FIX, 0u };
        assert(oa_packet_build_position(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 0u);
    }

    /* ARMED is the state where the clock starts, so the same value is carried
     * there. The rule is about PAD_IDLE alone, not about "before flight". */
    hdr.state = (uint8_t)OA_STATE_ARMED;
    {
        oa_status_body_t body = { 51325u, 200u };
        assert(oa_packet_build_status(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert(ref_decode(p, len, &dec) == NULL);
        assert(dec.t_ms == 987654321u);
    }
}

/* CLAIM: bit 7 "Must be transmitted as 0 and ignored on receive", enforced by the
 * builder rather than by every call site remembering. */
static void test_reserved_flag_bit_never_reaches_the_wire(void)
{
    oa_packet_header_t hdr = fixture_header();
    oa_status_body_t   body = { 51325u, 200u };
    uint8_t            p[OA_PACKET_MAX_BYTES];
    size_t             len = 0u;
    unsigned           value;

    for (value = 0u; value <= 0xFFu; value++) {
        hdr.flags = (uint8_t)value;
        assert(oa_packet_build_status(&hdr, &body, p, sizeof p, &len) == OA_OK);
        assert((p[SPEC_OFF_FLAGS] & SPEC_FLAG_RESERVED) == 0u);
        assert(p[SPEC_OFF_FLAGS] == (uint8_t)(value & 0x7Fu));
    }
}

/* CLAIM: "nothing is written to `out` at all if the buffer is too small [...] A
 * caller that reuses one buffer for every packet type would otherwise be handed a
 * half-overwritten previous packet with a valid CRC from the previous build still
 * on the end of it."
 *
 * That failure mode is the reason this is a conformance concern and not tidiness:
 * the result would decode as a valid packet. */
static void test_a_short_buffer_leaves_the_previous_packet_intact(void)
{
    const oa_packet_header_t hdr  = fixture_header();
    oa_beacon_body_t         bbody = { 515074000, -1278000, 45678 };
    oa_status_body_t         sbody = { 51325u, 200u };
    uint8_t                  p[OA_PACKET_MAX_BYTES];
    uint8_t                  before[OA_PACKET_MAX_BYTES];
    size_t                   len = 0u;
    ref_packet_t             dec;

    assert(oa_packet_build_beacon(&hdr, &bbody, p, sizeof p, &len) == OA_OK);
    assert(len == 22u);
    memcpy(before, p, sizeof p);

    /* One byte short of a STATUS packet, into the same buffer. */
    len = 999u;
    assert(oa_packet_build_status(&hdr, &sbody, p, 12u, &len) == OA_ERR_BUFFER);
    assert(memcmp(before, p, sizeof p) == 0);

    /* The BEACON that was there is still a whole, valid BEACON. */
    assert(ref_decode(p, 22u, &dec) == NULL);
    assert(dec.ptype == 4u);
    assert(dec.apogee_cm == 45678);

    /* Exactly the right size succeeds, so the boundary is at the length the spec
     * states and not one either side of it. */
    assert(oa_packet_build_status(&hdr, &sbody, p, 13u, &len) == OA_OK);
    assert(len == 13u);
}

/* CLAIM: "A reading below 50000 Pa is transmitted as 0, a reading above 115535 Pa
 * is transmitted as 65535, and BARO_FAULT is set in either case."
 *
 * Both clamp endpoints and both sides of each, because an off-by-one here would
 * mark a legitimate launch site reading as a sensor fault, or worse, transmit a
 * clamped reference as if it were a measurement. The reference is the zero the
 * entire altitude column is measured against. */
static void test_pad_pressure_clamps_at_both_endpoints(void)
{
    bool fault;

    /* Below the floor. */
    fault = false;
    assert(oa_packet_encode_pad_pressure(SPEC_PAD_PRESSURE_MIN - 1, &fault) == 0u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(0, &fault) == 0u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(INT32_MIN, &fault) == 0u);
    assert(fault == true);

    /* Exactly at the floor is representable and is not a fault. */
    fault = true;
    assert(oa_packet_encode_pad_pressure(SPEC_PAD_PRESSURE_MIN, &fault) == 0u);
    assert(fault == false);

    /* One pascal above the floor is one count above zero: the field carries
     * whole pascals, and that is what earns the resolution. */
    fault = true;
    assert(oa_packet_encode_pad_pressure(SPEC_PAD_PRESSURE_MIN + 1, &fault) == 1u);
    assert(fault == false);

    /* Exactly at the ceiling. */
    fault = true;
    assert(oa_packet_encode_pad_pressure(SPEC_PAD_PRESSURE_MAX, &fault) == 65535u);
    assert(fault == false);

    /* Above the ceiling. */
    fault = false;
    assert(oa_packet_encode_pad_pressure(SPEC_PAD_PRESSURE_MAX + 1, &fault) == 65535u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(INT32_MAX, &fault) == 65535u);
    assert(fault == true);

    /* And the offset itself, checked against the spec's own worked description:
     * "the transmitted value is pressure_pa - 50000". */
    fault = true;
    assert(oa_packet_encode_pad_pressure(101325, &fault) == 51325u);
    assert(fault == false);

    /* The band is exactly 65536 values wide, which is what makes the offset
     * worth its arithmetic. */
    assert(SPEC_PAD_PRESSURE_MAX - SPEC_PAD_PRESSURE_MIN + 1 == 65536);
}

/* CLAIM: "One byte, battery_volts = 2.5 + batt / 100. Range 2.50 V to 5.05 V in
 * 10 mV steps", and from oa_battery.h, clamping rather than failing at both ends
 * with rounding to nearest.
 *
 * All 256 values, which is cheap and rules out the whole class of off-by-one
 * scaling bugs at once. */
static void test_battery_clamps_at_both_endpoints_and_round_trips(void)
{
    unsigned b;

    /* Below the floor and at it. */
    assert(oa_battery_encode_mv(SPEC_BATTERY_MIN_MV - 1) == 0u);
    assert(oa_battery_encode_mv(0) == 0u);
    assert(oa_battery_encode_mv(INT32_MIN) == 0u);
    assert(oa_battery_encode_mv(SPEC_BATTERY_MIN_MV) == 0u);

    /* At the ceiling and above it. */
    assert(oa_battery_encode_mv(SPEC_BATTERY_MAX_MV) == 255u);
    assert(oa_battery_encode_mv(SPEC_BATTERY_MAX_MV + 1) == 255u);
    assert(oa_battery_encode_mv(INT32_MAX) == 255u);

    /* Every representable byte decodes to the millivolts the spec's formula
     * gives, and encodes back to itself. */
    for (b = 0u; b <= 255u; b++) {
        const int32_t mv = SPEC_BATTERY_MIN_MV + (int32_t)b * SPEC_BATTERY_STEP_MV;

        assert(oa_battery_decode_mv((uint8_t)b) == mv);
        assert(oa_battery_encode_mv(mv) == (uint8_t)b);
    }

    /* "Rounds to nearest rather than truncating, so that a decoded value is never
     * more than 5 mV from the reading." Checked across the whole band rather than
     * at a chosen point, since truncation and rounding agree at the step
     * boundaries and only differ between them. */
    {
        int32_t mv;

        for (mv = SPEC_BATTERY_MIN_MV; mv <= SPEC_BATTERY_MAX_MV; mv++) {
            const int32_t decoded = oa_battery_decode_mv(oa_battery_encode_mv(mv));
            const int32_t error   = (decoded > mv) ? (decoded - mv) : (mv - decoded);

            assert(error <= SPEC_BATTERY_STEP_MV / 2);
        }
    }

    /* The spec's own worked example: 2.5 + batt/100 volts. batt of 200 is 4.50 V,
     * which is 4500 mV. */
    assert(oa_battery_decode_mv(200u) == 4500);
    assert(oa_battery_encode_mv(4500) == 200u);
}

/* CLAIM: "flight.bin record, 36 bytes", field for field, and "every multi-byte
 * field is little-endian".
 *
 * The values are chosen so that no two fields could be confused if one landed at
 * another's offset: every one is distinct, and the signed ones alternate sign. */
static void test_flight_record_offsets_and_size(void)
{
    oa_log_flight_t rec;
    uint8_t         buf[SPEC_FLIGHT_RECORD_BYTES];
    size_t          i;
    size_t          covered = 0u;

    assert(OA_LOG_FLIGHT_RECORD_BYTES == SPEC_FLIGHT_RECORD_BYTES);
    assert(oa_log_stream_record_bytes(OA_LOG_STREAM_FLIGHT) == SPEC_FLIGHT_RECORD_BYTES);
    assert(str_eq(oa_log_stream_filename(OA_LOG_STREAM_FLIGHT), SPEC_FLIGHT_FILENAME));

    memset(&rec, 0, sizeof rec);
    rec.t_ms           = 0x01020304u;
    rec.pressure_pa    = -2000000;
    rec.temp_dc        = -321;
    rec.accel_mg[0]    = 1000;
    rec.accel_mg[1]    = -2000;
    rec.accel_mg[2]    = 3000;
    rec.gyro_cdps[0]   = -4000;
    rec.gyro_cdps[1]   = 5000;
    rec.gyro_cdps[2]   = -6000;
    rec.hg_accel_dg[0] = 7000;
    rec.hg_accel_dg[1] = -8000;
    rec.hg_accel_dg[2] = 9000;
    rec.alt_cm         = -123456;
    rec.vel_dm_s       = -789;
    rec.state          = (uint8_t)OA_STATE_APOGEE;
    rec.flags          = 0x2Du;

    memset(buf, 0xEE, sizeof buf);
    assert(oa_log_pack_flight(&rec, buf, sizeof buf) == OA_OK);

    /* Every field at the offset the spec's table states. */
    assert(ref_u32(buf, 0u) == 0x01020304u);
    assert(ref_i32(buf, 4u) == -2000000);
    assert(ref_i16(buf, 8u) == -321);
    assert(ref_i16(buf, 10u) == 1000);
    assert(ref_i16(buf, 12u) == -2000);
    assert(ref_i16(buf, 14u) == 3000);
    assert(ref_i16(buf, 16u) == -4000);
    assert(ref_i16(buf, 18u) == 5000);
    assert(ref_i16(buf, 20u) == -6000);
    assert(ref_i16(buf, 22u) == 7000);
    assert(ref_i16(buf, 24u) == -8000);
    assert(ref_i16(buf, 26u) == 9000);
    assert(ref_i32(buf, 28u) == -123456);
    assert(ref_i16(buf, 32u) == -789);
    assert(buf[34] == 4u);
    assert(buf[35] == 0x2Du);

    /* Every one of the 36 bytes is claimed by exactly one field. A gap would be a
     * byte that carries whatever the previous record left there, and an overlap
     * would be two fields writing the same byte. */
    for (i = 0u; i < SPEC_FLIGHT_FIELD_COUNT; i++) {
        const spec_log_field_t *f     = &k_spec_flight_fields[i];
        const size_t            width = (str_eq(f->type, "u8") ? 1u
                                         : str_eq(f->type, "i16") || str_eq(f->type, "u16") ? 2u
                                                                                            : 4u);
        covered += width * f->count;
    }
    assert(covered == SPEC_FLIGHT_RECORD_BYTES);

    /* "nothing is written at all if the buffer is too small", one byte short. */
    {
        uint8_t small[SPEC_FLIGHT_RECORD_BYTES];

        memset(small, 0x5A, sizeof small);
        assert(oa_log_pack_flight(&rec, small, SPEC_FLIGHT_RECORD_BYTES - 1u) == OA_ERR_BUFFER);
        for (i = 0u; i < sizeof small; i++) {
            assert(small[i] == 0x5A);
        }
    }
}

/* CLAIM: "gnss.bin record, 16 bytes", field for field. */
static void test_gnss_record_offsets_and_size(void)
{
    oa_log_gnss_t rec;
    uint8_t       buf[SPEC_GNSS_RECORD_BYTES];
    size_t        i;

    assert(OA_LOG_GNSS_RECORD_BYTES == SPEC_GNSS_RECORD_BYTES);
    assert(oa_log_stream_record_bytes(OA_LOG_STREAM_GNSS) == SPEC_GNSS_RECORD_BYTES);
    assert(str_eq(oa_log_stream_filename(OA_LOG_STREAM_GNSS), SPEC_GNSS_FILENAME));

    memset(&rec, 0, sizeof rec);
    rec.t_ms   = 0x0A0B0C0Du;
    rec.lat_e7 = 515074000;
    rec.lon_e7 = -1278000;
    rec.alt_m  = -321; /* below the ellipsoid, which is a real place to be */
    rec.sats   = 11u;
    rec.fix    = 3u;   /* u-blox convention: 3 is three-dimensional */

    memset(buf, 0xEE, sizeof buf);
    assert(oa_log_pack_gnss(&rec, buf, sizeof buf) == OA_OK);

    assert(ref_u32(buf, 0u) == 0x0A0B0C0Du);
    assert(ref_i32(buf, 4u) == 515074000);
    assert(ref_i32(buf, 8u) == -1278000);
    assert(ref_i16(buf, 12u) == -321);
    assert(buf[14] == 11u);
    assert(buf[15] == 3u);

    {
        uint8_t small[SPEC_GNSS_RECORD_BYTES];

        memset(small, 0x5A, sizeof small);
        assert(oa_log_pack_gnss(&rec, small, SPEC_GNSS_RECORD_BYTES - 1u) == OA_ERR_BUFFER);
        for (i = 0u; i < sizeof small; i++) {
            assert(small[i] == 0x5A);
        }
    }
}

/* ---------------------------------------------------------------------------
 * A scanner for the emitted manifest.
 *
 * Not a general JSON parser. It reads the shape oa_log_manifest.c writes, which
 * is enough to pull the layout back out and decode a record with it, and that is
 * the only thing this test needs it for. The byte-for-byte comparison of a whole
 * manifest against the spec's published example is test_oa_log_manifest.c's job.
 *
 * It is deliberately strict: every lookup asserts rather than returning a
 * default, because a manifest missing a key it should carry is exactly the
 * failure this test exists to catch, and a scanner that quietly returned zero
 * would report it as an offset of zero instead.
 * ------------------------------------------------------------------------ */

/* The first occurrence of `needle` at or after `from`, or NULL. */
static const char *scan_find(const char *from, const char *needle)
{
    return strstr(from, needle);
}

/* Reads a non-negative integer immediately after `"key": `. */
static const char *scan_int(const char *from, const char *key, long *out)
{
    char        pattern[64];
    const char *at;
    long        value = 0;
    bool        any   = false;

    assert(strlen(key) + 6u < sizeof pattern);
    pattern[0] = '\0';
    strcat(pattern, "\"");
    strcat(pattern, key);
    strcat(pattern, "\": ");

    at = scan_find(from, pattern);
    assert(at != NULL);
    at += strlen(pattern);

    while (*at >= '0' && *at <= '9') {
        value = value * 10 + (*at - '0');
        any   = true;
        at++;
    }
    assert(any);

    *out = value;
    return at;
}

/* Reads a quoted string immediately after `"key": `. */
static const char *scan_str(const char *from, const char *key, char *out, size_t cap)
{
    char        pattern[64];
    const char *at;
    size_t      n = 0u;

    assert(strlen(key) + 6u < sizeof pattern);
    pattern[0] = '\0';
    strcat(pattern, "\"");
    strcat(pattern, key);
    strcat(pattern, "\": \"");

    at = scan_find(from, pattern);
    assert(at != NULL);
    at += strlen(pattern);

    while (*at != '"') {
        assert(*at != '\0');
        assert(n + 1u < cap);
        out[n++] = *at++;
    }
    out[n] = '\0';

    return at;
}

/* Reads a bare JSON token after `"key": `, stopping at the comma or the closing
 * brace. `scale` is a JSON number rather than a string, which is correct for the
 * format and is why it cannot be read with scan_str: a reader does arithmetic
 * with it. The firmware emits the exact text from its table rather than
 * formatting a float, so comparing the token as text is the right check. Anything
 * else would be asking whether a double round-trips, which is a different
 * question and not the one the spec cares about. */
static const char *scan_token(const char *from, const char *key, char *out, size_t cap)
{
    char        pattern[64];
    const char *at;
    size_t      n = 0u;

    assert(strlen(key) + 6u < sizeof pattern);
    pattern[0] = '\0';
    strcat(pattern, "\"");
    strcat(pattern, key);
    strcat(pattern, "\": ");

    at = scan_find(from, pattern);
    assert(at != NULL);
    at += strlen(pattern);

    while (*at != ',' && *at != '}' && *at != '\n' && *at != '\0') {
        assert(n + 1u < cap);
        out[n++] = *at++;
    }
    while (n > 0u && out[n - 1u] == ' ') {
        n--;
    }
    out[n] = '\0';

    return at;
}

static size_t wire_bytes(const char *type)
{
    if (str_eq(type, "u8") || str_eq(type, "i8")) {
        return 1u;
    }
    if (str_eq(type, "u16") || str_eq(type, "i16")) {
        return 2u;
    }
    if (str_eq(type, "u32") || str_eq(type, "i32")) {
        return 4u;
    }
    assert(0 && "manifest names a wire type this format does not define");
    return 0u;
}

/* Decode one element of a field, at the offset and type the manifest gave, using
 * the readers written from the spec. Returned widened to i64 so one comparison
 * covers every type. */
static long long manifest_read(const uint8_t *rec, size_t offset, const char *type)
{
    if (str_eq(type, "u8")) {
        return (long long)rec[offset];
    }
    if (str_eq(type, "i16")) {
        return (long long)ref_i16(rec, offset);
    }
    if (str_eq(type, "u16")) {
        return (long long)ref_u16(rec, offset);
    }
    if (str_eq(type, "i32")) {
        return (long long)ref_i32(rec, offset);
    }
    if (str_eq(type, "u32")) {
        return (long long)ref_u32(rec, offset);
    }
    assert(0 && "manifest names a wire type this format does not define");
    return 0;
}

/* Walk one stream object in the manifest and check it against the hand
 * transcribed spec table, then against the bytes the packer actually wrote.
 *
 * Returns a pointer past the stream, so the caller can look for the next one. */
static const char *check_stream(const char             *stream_start,
                                const char             *expect_file,
                                size_t                  expect_record_bytes,
                                const spec_log_field_t *spec_fields,
                                size_t                  spec_field_count,
                                const uint8_t          *packed,
                                const long long        *expect_values,
                                const size_t           *expect_value_index)
{
    char        text[64];
    long        record_bytes = 0;
    const char *cursor;
    const char *fields_start;
    size_t      i;
    bool        claimed[64];

    memset(claimed, 0, sizeof claimed);

    (void)scan_str(stream_start, "file", text, sizeof text);
    assert(str_eq(text, expect_file));

    (void)scan_int(stream_start, "record_bytes", &record_bytes);
    assert((size_t)record_bytes == expect_record_bytes);

    /* "endian": "little" is what makes the little-endian readers above the right
     * ones. A manifest that said otherwise would describe a different file. */
    (void)scan_str(stream_start, "endian", text, sizeof text);
    assert(str_eq(text, "little"));

    fields_start = scan_find(stream_start, "\"fields\": [");
    assert(fields_start != NULL);

    cursor = fields_start;
    for (i = 0u; i < spec_field_count; i++) {
        const spec_log_field_t *spec = &spec_fields[i];
        char                    name[64];
        char                    type[16];
        char                    scale[16];
        char                    unit[16];
        long                    offset = 0;
        long                    count  = 1;
        const char             *entry;
        const char             *entry_end;
        size_t                  e;

        entry = scan_find(cursor, "{ \"name\": \"");
        assert(entry != NULL);
        entry_end = scan_find(entry, "}");
        assert(entry_end != NULL);

        cursor = scan_str(entry, "name", name, sizeof name);
        (void)scan_int(entry, "offset", &offset);
        (void)scan_str(entry, "type", type, sizeof type);
        (void)scan_token(entry, "scale", scale, sizeof scale);
        (void)scan_str(entry, "unit", unit, sizeof unit);

        /* "count" is present only on the array fields, and only within this
         * entry: searching past the closing brace would find the next field's. */
        {
            const char *count_at = scan_find(entry, "\"count\": ");

            if (count_at != NULL && count_at < entry_end) {
                (void)scan_int(entry, "count", &count);
            }
        }

        /* The manifest against the specification, field for field. */
        assert(str_eq(name, spec->name));
        assert((size_t)offset == spec->offset);
        assert(str_eq(type, spec->type));
        assert((size_t)count == spec->count);
        assert(str_eq(scale, spec->scale));
        assert(str_eq(unit, spec->unit));

        /* The manifest against the bytes. Every element is read back at the
         * offset and type the manifest published, with the reference readers,
         * and has to be the value that went in. This is the assertion that makes
         * the manifest more than a description: a reader following it recovers
         * the record. */
        for (e = 0u; e < (size_t)count; e++) {
            const size_t width = wire_bytes(type);
            const size_t at    = (size_t)offset + e * width;
            size_t       b;

            assert(manifest_read(packed, at, type)
                   == expect_values[expect_value_index[i] + e]);

            /* And no two fields may claim the same byte, and none may fall
             * outside the record. */
            for (b = 0u; b < width; b++) {
                assert(at + b < expect_record_bytes);
                assert(claimed[at + b] == false);
                claimed[at + b] = true;
            }
        }

        cursor = entry_end;
    }

    /* Every byte of the record is described. A gap would be a byte a reader
     * cannot interpret and a writer need never set. */
    for (i = 0u; i < expect_record_bytes; i++) {
        assert(claimed[i] == true);
    }

    return cursor;
}

/* CLAIM: "Each flight directory carries a meta.json that states the record layout
 * in full: every field, its offset, its type, and its scaling. A third-party tool
 * reads the manifest and can decode the records without knowing this document."
 *
 * That claim is only worth anything if the manifest and the packer agree, so this
 * test writes a manifest, packs a record, and then decodes the record using only
 * what the manifest said. The firmware generates both from one table, which makes
 * agreement structural; this checks that the one table is the one the spec
 * published. */
static void test_the_manifest_describes_the_bytes_the_packer_wrote(void)
{
    static buf_sink_t s;
    oa_sink_t         sink = { &buf_vt, &s };
    oa_log_manifest_t manifest;
    oa_log_flight_t   frec;
    oa_log_gnss_t     grec;
    uint8_t           fpacked[SPEC_FLIGHT_RECORD_BYTES];
    uint8_t           gpacked[SPEC_GNSS_RECORD_BYTES];
    const char       *streams;
    const char       *flight_stream;
    const char       *gnss_stream;
    long              spec_version = 0;

    /* One value per element, in table order, so the checker can index into them
     * with the field's position and the element's. */
    static const long long flight_values[] = {
        0x01020304, /* t_ms */
        -2000000,   /* pressure_pa */
        -321,       /* temp_dc */
        1000, -2000, 3000,    /* accel_mg */
        -4000, 5000, -6000,   /* gyro_cdps */
        7000, -8000, 9000,    /* hg_accel_dg */
        -123456,    /* alt_cm */
        -789,       /* vel_dm_s */
        4,          /* state, APOGEE */
        0x2D,       /* flags */
    };
    static const size_t flight_index[] = { 0u, 1u, 2u, 3u, 6u, 9u, 12u, 13u, 14u, 15u };

    static const long long gnss_values[] = {
        0x0A0B0C0D, 515074000, -1278000, -321, 11, 3,
    };
    static const size_t gnss_index[] = { 0u, 1u, 2u, 3u, 4u, 5u };

    /* Pack the two records first, so the manifest is checked against bytes that
     * already exist rather than against bytes written to match it. */
    memset(&frec, 0, sizeof frec);
    frec.t_ms           = 0x01020304u;
    frec.pressure_pa    = -2000000;
    frec.temp_dc        = -321;
    frec.accel_mg[0]    = 1000;
    frec.accel_mg[1]    = -2000;
    frec.accel_mg[2]    = 3000;
    frec.gyro_cdps[0]   = -4000;
    frec.gyro_cdps[1]   = 5000;
    frec.gyro_cdps[2]   = -6000;
    frec.hg_accel_dg[0] = 7000;
    frec.hg_accel_dg[1] = -8000;
    frec.hg_accel_dg[2] = 9000;
    frec.alt_cm         = -123456;
    frec.vel_dm_s       = -789;
    frec.state          = (uint8_t)OA_STATE_APOGEE;
    frec.flags          = 0x2Du;
    assert(oa_log_pack_flight(&frec, fpacked, sizeof fpacked) == OA_OK);

    memset(&grec, 0, sizeof grec);
    grec.t_ms   = 0x0A0B0C0Du;
    grec.lat_e7 = 515074000;
    grec.lon_e7 = -1278000;
    grec.alt_m  = -321;
    grec.sats   = 11u;
    grec.fix    = 3u;
    assert(oa_log_pack_gnss(&grec, gpacked, sizeof gpacked) == OA_OK);

    /* A Track manifest, which is the only variant that carries both streams. */
    memset(&manifest, 0, sizeof manifest);
    s.len = 0u;
    manifest.flight            = 1u;
    manifest.device_id         = "oapogee-000000000000";
    manifest.tier              = "track";
    manifest.path              = "board";
    manifest.hw_rev            = NULL; /* no board has been fabricated */
    manifest.fw_version        = "0.1.0";
    manifest.fw_git            = "0000000";
    manifest.armed_utc         = NULL;
    manifest.armed_uptime_ms   = 128394u;
    manifest.simulated         = false;
    manifest.pad_pressure_pa   = 101325;
    manifest.has_gnss_stream   = true;
    manifest.flight_nominal_hz = OA_UNSET; /* no log rate has been chosen */
    manifest.gnss_nominal_hz   = OA_UNSET;

    assert(oa_log_write_manifest(&manifest, NULL, &sink) == OA_OK);
    assert(s.len > 0u);
    assert(s.len < sizeof s.buf);
    s.buf[s.len] = '\0';

    /* "The spec version in the manifest tells you which document to consult when
     * something is ambiguous; it is not required to read the data." */
    (void)scan_int(s.buf, "spec_version", &spec_version);
    assert(spec_version == 1);
    assert(OA_LOG_SPEC_VERSION == 1);

    streams = scan_find(s.buf, "\"streams\": {");
    assert(streams != NULL);

    flight_stream = scan_find(streams, "\"flight\": {");
    assert(flight_stream != NULL);
    gnss_stream = scan_find(streams, "\"gnss\": {");
    assert(gnss_stream != NULL);
    assert(flight_stream < gnss_stream);

    (void)check_stream(flight_stream, SPEC_FLIGHT_FILENAME, SPEC_FLIGHT_RECORD_BYTES,
                       k_spec_flight_fields, SPEC_FLIGHT_FIELD_COUNT, fpacked, flight_values,
                       flight_index);

    (void)check_stream(gnss_stream, SPEC_GNSS_FILENAME, SPEC_GNSS_RECORD_BYTES,
                       k_spec_gnss_fields, SPEC_GNSS_FIELD_COUNT, gpacked, gnss_values,
                       gnss_index);

    /* "A conforming writer must write the summary object in the manifest it
     * writes before the first record, with every member null and landed false."
     * A manifest whose landed is still false is itself information: the flight
     * ended before landing was detected. */
    assert(scan_find(s.buf, "\"summary\"") != NULL);
    assert(scan_find(s.buf, "\"apogee_m\": null") != NULL);
    assert(scan_find(s.buf, "\"t_apogee_ms\": null") != NULL);
    assert(scan_find(s.buf, "\"max_accel_g\": null") != NULL);
    assert(scan_find(s.buf, "\"max_velocity_m_s\": null") != NULL);
    assert(scan_find(s.buf, "\"flight_duration_ms\": null") != NULL);
    assert(scan_find(s.buf, "\"landed\": false") != NULL);

    /* "streams lists exactly the files present in the flight directory, and
     * nothing else. A build with no GNSS receiver writes no gnss.bin and carries
     * no gnss key." A Solo or Link log is the same manifest without that key. */
    s.len                    = 0u;
    manifest.has_gnss_stream = false;
    manifest.tier            = "solo";
    assert(oa_log_write_manifest(&manifest, NULL, &sink) == OA_OK);
    s.buf[s.len] = '\0';
    assert(scan_find(s.buf, "\"flight\": {") != NULL);
    assert(scan_find(s.buf, "\"gnss\"") == NULL);
    assert(scan_find(s.buf, "gnss.bin") == NULL);

    /* And the record layout within a stream never varies by build variant, which
     * is why hg_accel_dg keeps its six bytes on a build with no high-g part. */
    (void)check_stream(scan_find(s.buf, "\"flight\": {"), SPEC_FLIGHT_FILENAME,
                       SPEC_FLIGHT_RECORD_BYTES, k_spec_flight_fields, SPEC_FLIGHT_FIELD_COUNT,
                       fpacked, flight_values, flight_index);
}

/* ---------------------------------------------------------------------------
 * The state machine, and the refusal that is the point of the whole
 * configuration design.
 * ------------------------------------------------------------------------ */

/* Set every field this build requires to 1, walking the field table rather than
 * naming fields, so a threshold added to oa_config.h is covered here the day it
 * lands. The 1 is not a measurement and not a proposal: it is the smallest value
 * that is not the unset sentinel, and its only job is to make the field set. */
static void fixture_flightworthy(oa_config_t *cfg, oa_features_t features)
{
    size_t i;

    oa_config_init(cfg);

    for (i = 0u; i < OA_CFG_FIELD_COUNT; i++) {
        const oa_config_field_t field = (oa_config_field_t)i;

        if (oa_config_field_is_required(field, features)) {
            assert(oa_config_set(cfg, field, 1) == OA_OK);
        }
    }

    assert(oa_config_is_flightworthy(cfg, features, NULL) == true);
}

/* CLAIM, from data/flight-phases.yaml: "Every numeric threshold here is null [...]
 * the actual numbers depend on measured sensor noise on real hardware. Choosing
 * them from intuition and publishing them as if they were tuned would be the
 * worst kind of wrong: plausible, specific, and untested."
 *
 * And from oa_state.h: "PAD_IDLE -> ARMED: operator_armed, and only if the
 * configuration is flightworthy for this build's features. A payload whose
 * thresholds are unmeasured refuses to arm, and refusing is correct."
 *
 * This is the test that turns that sentence into behaviour. It clears one
 * required field at a time, across every build variant, and asserts the machine
 * stays on the pad each time. Every required field, not a chosen one, because a
 * gate that only covered the fields somebody remembered is a gate that lets the
 * forgotten one fly. */
static void test_the_state_machine_will_not_arm_with_any_threshold_unset(void)
{
    static const oa_features_t variants[] = {
        OA_FEATURE_NONE,                                        /* Solo  */
        OA_FEATURE_RADIO,                                       /* Link  */
        OA_FEATURE_RADIO | OA_FEATURE_GNSS,                     /* Track */
        OA_FEATURE_RADIO | OA_FEATURE_GNSS | OA_FEATURE_HIGH_G, /* Track, high-g fitted */
    };
    size_t v;
    size_t checked = 0u;

    for (v = 0u; v < sizeof variants / sizeof variants[0]; v++) {
        const oa_features_t features = variants[v];
        size_t              i;

        for (i = 0u; i < OA_CFG_FIELD_COUNT; i++) {
            const oa_config_field_t field = (oa_config_field_t)i;
            oa_config_t             cfg;
            oa_state_ctx_t          ctx;
            oa_state_input_t        in;
            oa_state_output_t       out;
            oa_config_report_t      report;

            if (!oa_config_field_is_required(field, features)) {
                continue;
            }

            fixture_flightworthy(&cfg, features);
            assert(oa_config_clear(&cfg, field) == OA_OK);

            /* The validator says no, and says which one, with what would settle
             * it. A payload that refuses to arm is only useful if it can tell the
             * operator what is missing. */
            assert(oa_config_is_flightworthy(&cfg, features, &report) == false);
            assert(report.missing_count == 1u);
            assert(oa_config_field_name(field) != NULL);
            assert(oa_config_field_why(field) != NULL);
            assert(oa_config_field_why(field)[0] != '\0');

            /* And the machine refuses, with the arming switch closed. */
            assert(oa_state_init(&ctx) == OA_OK);
            assert(oa_state_current(&ctx) == OA_STATE_PAD_IDLE);

            memset(&in, 0, sizeof in);
            in.operator_armed = true;
            in.baro_valid     = true;

            assert(oa_state_step(&ctx, &cfg, features, &in, &out) == OA_OK);
            assert(out.state == OA_STATE_PAD_IDLE);
            assert(out.changed == false);
            assert(oa_state_current(&ctx) == OA_STATE_PAD_IDLE);

            /* Holding the switch closed does not wear the refusal down. */
            {
                int step;

                for (step = 0; step < 100; step++) {
                    in.t_ms = (uint32_t)step;
                    assert(oa_state_step(&ctx, &cfg, features, &in, &out) == OA_OK);
                    assert(out.state == OA_STATE_PAD_IDLE);
                }
            }

            /* Setting that one field back is all it takes, which is what makes
             * this a missing measurement and not a broken machine. */
            assert(oa_config_set(&cfg, field, 1) == OA_OK);
            assert(oa_config_is_flightworthy(&cfg, features, &report) == true);
            assert(report.missing_count == 0u);

            assert(oa_state_step(&ctx, &cfg, features, &in, &out) == OA_OK);
            assert(out.state == OA_STATE_ARMED);
            assert(out.changed == true);

            checked++;
        }
    }

    /* A count, so that a table emptied by a bad edit cannot make this test pass
     * by checking nothing at all. */
    assert(checked > 0u);
    assert(OA_CFG_FIELD_COUNT > 0u);
}

/* CLAIM, from oa_state.h: "There is no transition that skips a state and no
 * transition backwards. The machine cannot be commanded into a state, and there
 * is no entry point that sets one, because the only thing outside the payload
 * that can reach this machine is the arming switch."
 *
 * The passive payload boundary, checked at the level of the state machine's own
 * interface: arming is the only input that advances anything, and it advances
 * exactly one step. */
static void test_arming_is_the_only_operator_input(void)
{
    oa_config_t       cfg;
    oa_state_ctx_t    ctx;
    oa_state_input_t  in;
    oa_state_output_t out;

    fixture_flightworthy(&cfg, OA_FEATURE_NONE);
    assert(oa_state_init(&ctx) == OA_OK);

    memset(&in, 0, sizeof in);
    in.baro_valid = true;

    /* Without the switch, a flightworthy configuration on its own arms nothing. */
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_PAD_IDLE);

    /* With it, exactly one state, not two. ARMED does not run on into BOOST on
     * the same sample, because BOOST needs a locked pad reference and confirmed
     * acceleration and this sample has neither. */
    in.operator_armed = true;
    assert(oa_state_step(&ctx, &cfg, OA_FEATURE_NONE, &in, &out) == OA_OK);
    assert(out.state == OA_STATE_ARMED);
    assert(out.changed == true);

    /* And no peak has been claimed yet: "Returns OA_ERR_EMPTY before any sample
     * has been seen, rather than reporting a peak of zero, because zero is a
     * legitimate altitude and no measurement yet is not the same thing." */
    {
        int32_t  alt = 12345;
        uint32_t t   = 999u;

        if (oa_state_peak(&ctx, &alt, &t) == OA_ERR_EMPTY) {
            assert(alt == 12345);
            assert(t == 999u);
        }
    }
}

int main(void)
{
    test_packet_lengths_match_the_spec_table();
    test_header_byte_packs_version_and_type();
    test_flag_masks_match_the_spec_table();
    test_state_values_match_the_spec_table();

    test_status_packet_bytes();
    test_flight_packet_bytes();
    test_apogee_packet_bytes();
    test_beacon_packet_bytes();
    test_position_packet_bytes();

    test_pad_idle_transmits_t_ms_as_zero();
    test_reserved_flag_bit_never_reaches_the_wire();
    test_a_short_buffer_leaves_the_previous_packet_intact();

    test_pad_pressure_clamps_at_both_endpoints();
    test_battery_clamps_at_both_endpoints_and_round_trips();

    test_flight_record_offsets_and_size();
    test_gnss_record_offsets_and_size();
    test_the_manifest_describes_the_bytes_the_packer_wrote();

    test_the_state_machine_will_not_arm_with_any_threshold_unset();
    test_arming_is_the_only_operator_input();

    printf("test_oa_conformance: all checks passed\n");
    return 0;
}

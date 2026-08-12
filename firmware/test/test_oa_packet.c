/*
 * Conformance tests for the oApogee packet builders.
 *
 * Every offset, length, type code and constant in this file is written out as
 * a literal, taken by hand from the tables in docs/spec/telemetry-packet.md,
 * and deliberately not expanded from oa_packet_fields.def. That is the point of
 * the file. The .def table is the encoder's source, so a test that read the
 * layout from the same table would agree with the encoder no matter what the
 * table said, and a field typed into the wrong row would pass. This file is an
 * independent second statement of the layout, and the two have to agree.
 *
 * The CRC reference here is transcribed from the Python in the spec for the
 * same reason: the test should check the packet against what a receiver will
 * run, not against the function the encoder called.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware, and no
 * byte of this format has been observed on a radio.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_packet.h"

/* The five type codes and their lengths, from the packet type table in the
 * spec. Restated here as plain numbers so this file depends on the document
 * rather than on the firmware's own tables. */
#define SPEC_VERSION 1

#define SPEC_STATUS_CODE   0x1
#define SPEC_FLIGHT_CODE   0x2
#define SPEC_APOGEE_CODE   0x3
#define SPEC_BEACON_CODE   0x4
#define SPEC_POSITION_CODE 0x5

#define SPEC_STATUS_BODY   3
#define SPEC_FLIGHT_BODY   9
#define SPEC_APOGEE_BODY   8
#define SPEC_BEACON_BODY   12
#define SPEC_POSITION_BODY 9

#define SPEC_STATUS_TOTAL   13
#define SPEC_FLIGHT_TOTAL   19
#define SPEC_APOGEE_TOTAL   18
#define SPEC_BEACON_TOTAL   22
#define SPEC_POSITION_TOTAL 19

/* The state values, from the flight state table in the spec. */
#define SPEC_STATE_PAD_IDLE 0
#define SPEC_STATE_ARMED    1
#define SPEC_STATE_BOOST    2
#define SPEC_STATE_LANDED   6

/* INT32_MIN, spelled the way the spec's reference decoder spells it, so the
 * sentinel this file checks for is the one the document names. */
#define SPEC_INT32_MIN (-2147483647 - 1)

/* A byte value that is not zero and is not a plausible field value, used to
 * fill a buffer before a build so that "nothing was written" is checkable. */
#define CANARY 0xA5u

/* --------------------------------------------------------------------------
 * Readers. Field by field, little-endian, exactly as a portable receiver
 * would do it and exactly as the spec's reference decoder does it.
 * ----------------------------------------------------------------------- */

static uint16_t rd_u16(const uint8_t *p, size_t off)
{
    return (uint16_t)((uint16_t)p[off] | (uint16_t)((uint16_t)p[off + 1u] << 8));
}

static uint32_t rd_u32(const uint8_t *p, size_t off)
{
    return (uint32_t)p[off] | ((uint32_t)p[off + 1u] << 8) | ((uint32_t)p[off + 2u] << 16)
           | ((uint32_t)p[off + 3u] << 24);
}

/* Two's complement, which is what the spec requires of signed fields on the
 * wire. The host running this test is two's complement as well, so the cast is
 * the whole conversion. A receiver on a host that is not would need to do more
 * work here, which is why the spec states the representation rather than
 * assuming it. */
static int16_t rd_i16(const uint8_t *p, size_t off)
{
    return (int16_t)rd_u16(p, off);
}

static int32_t rd_i32(const uint8_t *p, size_t off)
{
    return (int32_t)rd_u32(p, off);
}

/* Transcribed from the reference decoder in docs/spec/telemetry-packet.md. */
static uint16_t ref_crc16(const uint8_t *data, size_t len)
{
    unsigned crc = 0xFFFFu;
    size_t   i;

    for (i = 0; i < len; i++) {
        crc ^= (unsigned)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (((crc << 1) ^ 0x1021u) & 0xFFFFu) : ((crc << 1) & 0xFFFFu);
        }
    }

    return (uint16_t)crc;
}

/* CLAIM: "The last two bytes of every packet are a CRC-16/CCITT-FALSE over all
 * preceding bytes of the packet, transmitted little-endian", and byte 0 packs
 * the version in the high nibble and the type in the low nibble. Checked on
 * every packet every test builds, because these two properties are what make a
 * buffer a packet. */
static void check_frame(const uint8_t *pkt, size_t total, unsigned type_code)
{
    uint16_t expected = ref_crc16(pkt, total - 2u);

    assert(pkt[0] == (uint8_t)((SPEC_VERSION << 4) | type_code));
    assert(pkt[total - 2u] == (uint8_t)(expected & 0xFFu));
    assert(pkt[total - 1u] == (uint8_t)(expected >> 8));
}

/* --------------------------------------------------------------------------
 * Tests.
 * ----------------------------------------------------------------------- */

/* CLAIM: "hdr packs version and type into one byte. Version 1 with a FLIGHT
 * packet is 0x12." That example is in the spec and is the one value a receiver
 * author will hardcode while testing, so it is checked literally. */
static void test_header_byte(void)
{
    assert(oa_packet_hdr_byte(1u, OA_PKT_STATUS) == 0x11u);
    assert(oa_packet_hdr_byte(1u, OA_PKT_FLIGHT) == 0x12u);
    assert(oa_packet_hdr_byte(1u, OA_PKT_APOGEE) == 0x13u);
    assert(oa_packet_hdr_byte(1u, OA_PKT_BEACON) == 0x14u);
    assert(oa_packet_hdr_byte(1u, OA_PKT_POSITION) == 0x15u);

    /* Each nibble is masked, so an out of range version cannot corrupt the
     * type nibble or the other way round. */
    assert(oa_packet_hdr_byte(0xFFu, OA_PKT_FLIGHT) == 0xF2u);
    assert(oa_packet_hdr_byte(2u, (oa_packet_type_t)0xFF) == 0x2Fu);
}

/* CLAIM: the type table's body and total lengths. "Total length is fixed per
 * packet type, so a receiver knows how many bytes to expect from the type
 * field alone." Types 0x0 and 0x6 to 0xF are reserved and are not something
 * the payload can be asked to send, so the length queries report 0 and the
 * name reports NULL rather than a placeholder. */
static void test_lengths_and_names(void)
{
    assert(oa_packet_total_bytes(OA_PKT_STATUS) == SPEC_STATUS_TOTAL);
    assert(oa_packet_total_bytes(OA_PKT_FLIGHT) == SPEC_FLIGHT_TOTAL);
    assert(oa_packet_total_bytes(OA_PKT_APOGEE) == SPEC_APOGEE_TOTAL);
    assert(oa_packet_total_bytes(OA_PKT_BEACON) == SPEC_BEACON_TOTAL);
    assert(oa_packet_total_bytes(OA_PKT_POSITION) == SPEC_POSITION_TOTAL);

    assert(oa_packet_body_bytes(OA_PKT_STATUS) == SPEC_STATUS_BODY);
    assert(oa_packet_body_bytes(OA_PKT_FLIGHT) == SPEC_FLIGHT_BODY);
    assert(oa_packet_body_bytes(OA_PKT_APOGEE) == SPEC_APOGEE_BODY);
    assert(oa_packet_body_bytes(OA_PKT_BEACON) == SPEC_BEACON_BODY);
    assert(oa_packet_body_bytes(OA_PKT_POSITION) == SPEC_POSITION_BODY);

    /* header + body + CRC, from the packet structure diagram. */
    assert(oa_packet_total_bytes(OA_PKT_FLIGHT) == 8u + oa_packet_body_bytes(OA_PKT_FLIGHT) + 2u);

    assert(strcmp(oa_packet_type_name(OA_PKT_STATUS), "STATUS") == 0);
    assert(strcmp(oa_packet_type_name(OA_PKT_FLIGHT), "FLIGHT") == 0);
    assert(strcmp(oa_packet_type_name(OA_PKT_APOGEE), "APOGEE") == 0);
    assert(strcmp(oa_packet_type_name(OA_PKT_BEACON), "BEACON") == 0);
    assert(strcmp(oa_packet_type_name(OA_PKT_POSITION), "POSITION") == 0);

    assert(oa_packet_total_bytes((oa_packet_type_t)0x0) == 0u);
    assert(oa_packet_total_bytes((oa_packet_type_t)0x6) == 0u);
    assert(oa_packet_total_bytes((oa_packet_type_t)0xF) == 0u);
    assert(oa_packet_body_bytes((oa_packet_type_t)0x6) == 0u);
    assert(oa_packet_type_name((oa_packet_type_t)0x0) == NULL);
    assert(oa_packet_type_name((oa_packet_type_t)0x6) == NULL);
}

/* CLAIM: the 0x1 STATUS body table. pad_pressure_pa_off is a u16 at offset 8
 * and batt is a u8 at offset 10, and the packet is 13 bytes. */
static void test_status_layout(void)
{
    oa_packet_header_t hdr  = { 0x00u, 7u, SPEC_STATE_ARMED, 4321u };
    oa_status_body_t   body = { 51325u, 0x5Au };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             len = 0;

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, &len) == OA_OK);
    assert(len == SPEC_STATUS_TOTAL);

    check_frame(pkt, SPEC_STATUS_TOTAL, SPEC_STATUS_CODE);
    assert(pkt[1] == 0x00u);
    assert(pkt[2] == 7u);
    assert(pkt[3] == SPEC_STATE_ARMED);
    assert(rd_u32(pkt, 4) == 4321u);
    assert(rd_u16(pkt, 8) == 51325u);
    assert(pkt[10] == 0x5Au);

    /* Nothing past the packet was touched. */
    assert(pkt[SPEC_STATUS_TOTAL] == CANARY);
}

/* CLAIM: the 0x2 FLIGHT body table. alt_cm i32 at 8, vel_dm_s i16 at 12,
 * accel_cg i16 at 14, batt u8 at 16, total 19.
 *
 * CLAIM: "alt_cm is signed, and negative values are legitimate rather than a
 * bug", so a negative altitude and a negative velocity are what this test
 * sends. A builder that clamped either at zero would be hiding a real
 * measurement. */
static void test_flight_layout_including_negatives(void)
{
    oa_packet_header_t hdr  = { 0x00u, 200u, SPEC_STATE_BOOST, 1234567u };
    oa_flight_body_t   body = { -12345, -321, 12345, 0xC3u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             len = 0;

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_flight(&hdr, &body, pkt, sizeof pkt, &len) == OA_OK);
    assert(len == SPEC_FLIGHT_TOTAL);

    check_frame(pkt, SPEC_FLIGHT_TOTAL, SPEC_FLIGHT_CODE);
    assert(pkt[2] == 200u);
    assert(pkt[3] == SPEC_STATE_BOOST);
    assert(rd_u32(pkt, 4) == 1234567u);
    assert(rd_i32(pkt, 8) == -12345);
    assert(rd_i16(pkt, 12) == -321);
    assert(rd_i16(pkt, 14) == 12345);
    assert(pkt[16] == 0xC3u);
    assert(pkt[SPEC_FLIGHT_TOTAL] == CANARY);

    /* The little-endian byte order of a negative i32, spelled out. -12345 is
     * 0xFFFFCFC7, so the low byte goes first. A big-endian store would still
     * pass the reader above if the reader were wrong in the same direction,
     * and this is what stops that. */
    assert(pkt[8] == 0xC7u);
    assert(pkt[9] == 0xCFu);
    assert(pkt[10] == 0xFFu);
    assert(pkt[11] == 0xFFu);
}

/* CLAIM: the 0x3 APOGEE body table. apogee_cm i32 at 8, t_apogee_ms u32 at 12,
 * total 18. t_apogee_ms is a u32 and is checked with a value above INT32_MAX,
 * because a signed store would turn a long flight's apogee time negative. */
static void test_apogee_layout(void)
{
    oa_packet_header_t hdr  = { 0x00u, 1u, 4 /* APOGEE state */, 30500u };
    oa_apogee_body_t   body = { 123456, 0xDEADBEEFu };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             len = 0;

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_apogee(&hdr, &body, pkt, sizeof pkt, &len) == OA_OK);
    assert(len == SPEC_APOGEE_TOTAL);

    check_frame(pkt, SPEC_APOGEE_TOTAL, SPEC_APOGEE_CODE);
    assert(rd_i32(pkt, 8) == 123456);
    assert(rd_u32(pkt, 12) == 0xDEADBEEFu);
    assert(pkt[SPEC_APOGEE_TOTAL] == CANARY);
}

/* CLAIM: the 0x4 BEACON body table. lat_e7 i32 at 8, lon_e7 i32 at 12,
 * apogee_cm i32 at 16, total 22. The coordinates here are arbitrary test
 * values, not a location and not a measurement. */
static void test_beacon_layout(void)
{
    oa_packet_header_t hdr  = { OA_FLAG_GNSS_FIX, 3u, SPEC_STATE_LANDED, 99000u };
    oa_beacon_body_t   body = { 476063000, -1223994000, 65432 };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             len = 0;

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_beacon(&hdr, &body, pkt, sizeof pkt, &len) == OA_OK);
    assert(len == SPEC_BEACON_TOTAL);
    assert(len == OA_PACKET_MAX_BYTES); /* BEACON is the longest packet in v1 */

    check_frame(pkt, SPEC_BEACON_TOTAL, SPEC_BEACON_CODE);
    assert(pkt[1] == OA_FLAG_GNSS_FIX);
    assert(rd_i32(pkt, 8) == 476063000);
    assert(rd_i32(pkt, 12) == -1223994000);
    assert(rd_i32(pkt, 16) == 65432);
}

/* CLAIM: the 0x5 POSITION body table. lat_e7 i32 at 8, lon_e7 i32 at 12, sats
 * u8 at 16, total 19. */
static void test_position_layout(void)
{
    oa_packet_header_t hdr  = { OA_FLAG_GNSS_FIX, 42u, SPEC_STATE_BOOST, 8000u };
    oa_position_body_t body = { 476063000, -1223994000, 9u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             len = 0;

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_position(&hdr, &body, pkt, sizeof pkt, &len) == OA_OK);
    assert(len == SPEC_POSITION_TOTAL);

    check_frame(pkt, SPEC_POSITION_TOTAL, SPEC_POSITION_CODE);
    assert(rd_i32(pkt, 8) == 476063000);
    assert(rd_i32(pkt, 12) == -1223994000);
    assert(pkt[16] == 9u);
    assert(pkt[SPEC_POSITION_TOTAL] == CANARY);
}

/* CLAIM: "with no fix there is no last value worth carrying, so lat_e7 and
 * lon_e7 carry INT32_MIN in both BEACON and POSITION", and for POSITION, "When
 * GNSS_FIX is clear, both position fields carry INT32_MIN and sats carries
 * zero, for the same reason BEACON uses that sentinel: zero is a real
 * coordinate."
 *
 * The builder writes what the caller hands it, so what this test checks is
 * that the sentinel survives the encoding: INT32_MIN must appear on the wire
 * as 00 00 00 80 and read back as INT32_MIN through the spec's "<i", with
 * GNSS_FIX clear beside it. A sign handling bug in the i32 store would turn
 * "no fix" into a coordinate near 111 degrees west, which is a real place. */
static void test_no_fix_sentinel_survives_encoding(void)
{
    oa_packet_header_t hdr      = { 0x00u, 5u, SPEC_STATE_LANDED, 120000u };
    oa_beacon_body_t   beacon   = { SPEC_INT32_MIN, SPEC_INT32_MIN, 123400 };
    oa_position_body_t position = { SPEC_INT32_MIN, SPEC_INT32_MIN, 0u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_beacon(&hdr, &beacon, pkt, sizeof pkt, NULL) == OA_OK);
    check_frame(pkt, SPEC_BEACON_TOTAL, SPEC_BEACON_CODE);

    assert((pkt[1] & OA_FLAG_GNSS_FIX) == 0u);
    assert(pkt[8] == 0x00u && pkt[9] == 0x00u && pkt[10] == 0x00u && pkt[11] == 0x80u);
    assert(pkt[12] == 0x00u && pkt[13] == 0x00u && pkt[14] == 0x00u && pkt[15] == 0x80u);
    assert(rd_i32(pkt, 8) == SPEC_INT32_MIN);
    assert(rd_i32(pkt, 12) == SPEC_INT32_MIN);
    assert(rd_i32(pkt, 16) == 123400); /* the apogee still goes out */

    memset(pkt, CANARY, sizeof pkt);
    assert(oa_packet_build_position(&hdr, &position, pkt, sizeof pkt, NULL) == OA_OK);
    check_frame(pkt, SPEC_POSITION_TOTAL, SPEC_POSITION_CODE);

    assert(rd_i32(pkt, 8) == SPEC_INT32_MIN);
    assert(rd_i32(pkt, 12) == SPEC_INT32_MIN);
    assert(pkt[16] == 0u);

    /* The sentinel is also what OA_LATLON_NO_FIX in the header means, and the
     * two must be the same number. */
    assert(OA_LATLON_NO_FIX == SPEC_INT32_MIN);
}

/* CLAIM: bit 7 of the flags byte is "reserved. Must be transmitted as 0 and
 * ignored on receive." The builder masks it, so a caller cannot transmit it by
 * accident, and every other bit passes through untouched. */
static void test_reserved_flag_bit_is_cleared(void)
{
    oa_packet_header_t hdr  = { 0xFFu, 0u, SPEC_STATE_ARMED, 1u };
    oa_status_body_t   body = { 0u, 0u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];

    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(pkt[1] == 0x7Fu);

    hdr.flags = OA_FLAG_GNSS_FIX | OA_FLAG_BARO_FAULT | OA_FLAG_SIM;
    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(pkt[1] == (0x01u | 0x04u | 0x40u));

    hdr.flags = 0x80u;
    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(pkt[1] == 0x00u);
}

/* CLAIM: "Before arming there is no elapsed time to report, so t_ms is
 * transmitted as 0 in every packet whose state is PAD_IDLE. It is not the
 * power-on uptime: the payload's uptime counter is a firmware implementation
 * detail and is never on the air."
 *
 * The builder is handed a large non-zero t_ms in PAD_IDLE, which is what a
 * caller passing uptime would do, and must transmit zero anyway. Out of
 * PAD_IDLE the value passes through unchanged, including for a state value
 * this firmware does not define. */
static void test_t_ms_is_zero_in_pad_idle(void)
{
    oa_packet_header_t hdr  = { 0x00u, 0u, SPEC_STATE_PAD_IDLE, 987654321u };
    oa_status_body_t   body = { 100u, 200u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];

    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(pkt[3] == SPEC_STATE_PAD_IDLE);
    assert(rd_u32(pkt, 4) == 0u);

    hdr.state = SPEC_STATE_ARMED;
    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(rd_u32(pkt, 4) == 987654321u);

    /* A u32 of milliseconds is about 49 days, and the top of that range must
     * survive the store. */
    hdr.t_ms = 0xFFFFFFFFu;
    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(rd_u32(pkt, 4) == 0xFFFFFFFFu);
}

/* CLAIM: "Values 7 to 255 are reserved. A receiver encountering one should
 * display the number rather than guessing, and must not treat it as an error."
 * The encoder passes a state value through unchanged for the same reason: what
 * to do with a reserved value is the receiver's decision, not the encoder's. */
static void test_reserved_state_passes_through(void)
{
    oa_packet_header_t hdr  = { 0x00u, 0u, 200u, 55u };
    oa_status_body_t   body = { 0u, 0u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];

    assert(oa_packet_build_status(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
    assert(pkt[3] == 200u);
    assert(rd_u32(pkt, 4) == 55u);
}

/* CLAIM, from oa_packet.h: "nothing is written to `out` at all if the buffer is
 * too small ... A caller that reuses one buffer for every packet type would
 * otherwise be handed a half-overwritten previous packet with a valid CRC from
 * the previous build still on the end of it."
 *
 * Checked at every capacity below the packet length, for the longest packet
 * and the shortest, with the whole buffer filled with a canary first. */
static void test_short_buffer_writes_nothing(void)
{
    oa_packet_header_t hdr    = { 0x00u, 1u, SPEC_STATE_LANDED, 1000u };
    oa_beacon_body_t   beacon = { 1, 2, 3 };
    oa_status_body_t   status = { 4u, 5u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];
    size_t             cap;
    size_t             len = 12345u;

    for (cap = 0; cap < SPEC_BEACON_TOTAL; cap++) {
        size_t i;

        memset(pkt, CANARY, sizeof pkt);
        assert(oa_packet_build_beacon(&hdr, &beacon, pkt, cap, &len) == OA_ERR_BUFFER);

        for (i = 0; i < sizeof pkt; i++) {
            assert(pkt[i] == CANARY);
        }

        /* out_len is untouched too, so a caller that ignored the result code
         * cannot then transmit a length it never got. */
        assert(len == 12345u);
    }

    for (cap = 0; cap < SPEC_STATUS_TOTAL; cap++) {
        memset(pkt, CANARY, sizeof pkt);
        assert(oa_packet_build_status(&hdr, &status, pkt, cap, NULL) == OA_ERR_BUFFER);
        assert(pkt[0] == CANARY);
    }

    /* Exactly the packet length is enough. The check is on the length, not on
     * a margin around it. */
    assert(oa_packet_build_status(&hdr, &status, pkt, SPEC_STATUS_TOTAL, &len) == OA_OK);
    assert(len == SPEC_STATUS_TOTAL);
}

/* CLAIM, from oa_packet.h: "out_len may be NULL; every other pointer argument
 * may not", returning OA_ERR_NULL. */
static void test_null_arguments(void)
{
    oa_packet_header_t hdr  = { 0x00u, 1u, SPEC_STATE_ARMED, 10u };
    oa_flight_body_t   body = { 1, 2, 3, 4u };
    uint8_t            pkt[OA_PACKET_MAX_BYTES];

    assert(oa_packet_build_flight(NULL, &body, pkt, sizeof pkt, NULL) == OA_ERR_NULL);
    assert(oa_packet_build_flight(&hdr, NULL, pkt, sizeof pkt, NULL) == OA_ERR_NULL);
    assert(oa_packet_build_flight(&hdr, &body, NULL, sizeof pkt, NULL) == OA_ERR_NULL);

    /* A NULL buffer is reported before the capacity is considered, because the
     * pointer is the caller's mistake either way. */
    assert(oa_packet_build_flight(&hdr, &body, NULL, 0, NULL) == OA_ERR_NULL);

    assert(oa_packet_build_flight(&hdr, &body, pkt, sizeof pkt, NULL) == OA_OK);
}

/* CLAIM: "the transmitted value is pressure_pa - 50000, and the 65536
 * representable values cover 50000 to 115535 Pa. A reading below 50000 Pa is
 * transmitted as 0, a reading above 115535 Pa is transmitted as 65535, and
 * BARO_FAULT is set in either case."
 *
 * Both endpoints are inside the band and must not raise the fault: 50000 Pa
 * encodes as 0 without a fault, and 0 with a fault is a different statement
 * about the same byte. That distinction is the whole reason the flag exists,
 * because this field is the zero the entire altitude column is measured
 * against. */
static void test_pad_pressure_encoding(void)
{
    bool fault = true;

    /* Bottom of the band. In range, no fault. */
    assert(oa_packet_encode_pad_pressure(50000, &fault) == 0u);
    assert(fault == false);

    /* Top of the band. In range, no fault. */
    fault = true;
    assert(oa_packet_encode_pad_pressure(115535, &fault) == 65535u);
    assert(fault == false);

    /* One pascal outside each end. Clamped, and flagged. */
    fault = false;
    assert(oa_packet_encode_pad_pressure(49999, &fault) == 0u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(115536, &fault) == 65535u);
    assert(fault == true);

    /* Far outside, including a negative reading, which is what a disconnected
     * barometer can produce. It must clamp rather than wrap into a plausible
     * pressure. */
    fault = false;
    assert(oa_packet_encode_pad_pressure(-1, &fault) == 0u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(INT32_MAX, &fault) == 65535u);
    assert(fault == true);

    fault = false;
    assert(oa_packet_encode_pad_pressure(INT32_MIN, &fault) == 0u);
    assert(fault == true);

    /* The reference decoder reads this field as 50000 + pad_off, so the
     * round trip has to land on the pressure that went in, in whole pascals,
     * everywhere inside the band. */
    for (int32_t pa = 50000; pa <= 115535; pa += 137) {
        bool     f       = true;
        uint16_t encoded = oa_packet_encode_pad_pressure(pa, &f);

        assert(f == false);
        assert(50000 + (int32_t)encoded == pa);
    }

    /* The flag pointer is documented as required. It is checked rather than
     * dereferenced blind, because a fault while reporting a pressure would
     * turn a clamped reading into a lost flight. */
    assert(oa_packet_encode_pad_pressure(101325, NULL) == 51325u);
}

/* CLAIM: "Every packet is independent. There is no state carried between
 * packets", and, from oa_packet.h, "a packet is a pure function of a header and
 * a body". Building the same inputs twice into a dirtied buffer must produce
 * identical bytes, and building a different type into the same buffer must not
 * leave any of the previous packet visible inside the new one. */
static void test_builds_are_independent_of_the_buffer(void)
{
    oa_packet_header_t hdr    = { OA_FLAG_SIM, 17u, SPEC_STATE_BOOST, 4242u };
    oa_flight_body_t   flight = { 100000, 250, -900, 0x99u };
    oa_beacon_body_t   beacon = { 1, 2, 3 };
    uint8_t            first[OA_PACKET_MAX_BYTES];
    uint8_t            second[OA_PACKET_MAX_BYTES];

    memset(first, 0x00, sizeof first);
    memset(second, 0xFFu, sizeof second);

    assert(oa_packet_build_flight(&hdr, &flight, first, sizeof first, NULL) == OA_OK);
    assert(oa_packet_build_flight(&hdr, &flight, second, sizeof second, NULL) == OA_OK);
    assert(memcmp(first, second, SPEC_FLIGHT_TOTAL) == 0);

    /* A longer packet built over a shorter one, then the shorter one again.
     * The 19 byte FLIGHT packet must be complete and correct even though bytes
     * 19 to 21 still hold the tail of the 22 byte BEACON. */
    assert(oa_packet_build_beacon(&hdr, &beacon, second, sizeof second, NULL) == OA_OK);
    assert(oa_packet_build_flight(&hdr, &flight, second, sizeof second, NULL) == OA_OK);
    assert(memcmp(first, second, SPEC_FLIGHT_TOTAL) == 0);
    check_frame(second, SPEC_FLIGHT_TOTAL, SPEC_FLIGHT_CODE);
}

int main(void)
{
    test_header_byte();
    test_lengths_and_names();
    test_status_layout();
    test_flight_layout_including_negatives();
    test_apogee_layout();
    test_beacon_layout();
    test_position_layout();
    test_no_fix_sentinel_survives_encoding();
    test_reserved_flag_bit_is_cleared();
    test_t_ms_is_zero_in_pad_idle();
    test_reserved_state_passes_through();
    test_short_buffer_writes_nothing();
    test_null_arguments();
    test_pad_pressure_encoding();
    test_builds_are_independent_of_the_buffer();

    printf("test_oa_packet: all checks passed\n");
    return 0;
}

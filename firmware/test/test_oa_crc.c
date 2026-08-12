/*
 * Conformance tests for oa_crc.
 *
 * Every test below names the claim in docs/spec/telemetry-packet.md that it
 * checks. The spec's CRC section and its reference decoder are the normative
 * text; this file exists so that a change to the implementation that no longer
 * agrees with them fails here rather than on the air.
 *
 * The reference implementation used for cross-checking is transcribed line by
 * line from the Python in the spec. It is here so that the two can disagree:
 * if somebody rewrites oa_crc16 as a table lookup for speed, this test is what
 * says whether the table was built correctly.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware, and no
 * packet has ever been transmitted.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_crc.h"

/* Transcribed from the reference decoder in docs/spec/telemetry-packet.md:
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

    for (i = 0; i < len; i++) {
        crc ^= (unsigned)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (((crc << 1) ^ 0x1021u) & 0xFFFFu) : ((crc << 1) & 0xFFFFu);
        }
    }

    return (uint16_t)crc;
}

/* A deterministic byte pattern, so a failure is reproducible. Not random data:
 * a test that fails only sometimes is a test people learn to re-run. */
static void fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = (uint8_t)((seed + (uint8_t)(i * 37u)) ^ (uint8_t)(i >> 3));
    }
}

/* CLAIM: "Parameters: polynomial 0x1021, initial value 0xFFFF, no input
 * reflection, no output reflection, no final XOR."
 *
 * 0x29B1 over the ASCII string "123456789" is the published check value for
 * CRC-16/CCITT-FALSE in the standard CRC catalogue. It is the one value that
 * distinguishes this parameter set from the reflected CRC-16s that share the
 * same polynomial, each of which would produce a different and equally
 * plausible looking checksum. It is not a number measured here. */
static void test_catalogue_check_value(void)
{
    const uint8_t msg[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    assert(oa_crc16(msg, sizeof msg) == 0x29B1u);
}

/* CLAIM: the parameters include an initial value of 0xFFFF, so a message of no
 * bytes leaves the register at the seed. This is the boundary a packet builder
 * would hit if it ever computed a CRC over a zero length span, and it must not
 * be 0x0000, which is what an implementation that initialised to zero would
 * return. */
static void test_empty_input_is_the_seed(void)
{
    uint8_t byte = 0x00u;

    assert(oa_crc16(&byte, 0) == OA_CRC16_INIT);
    assert(oa_crc16_update(OA_CRC16_INIT, &byte, 0) == OA_CRC16_INIT);
}

/* CLAIM, from oa_crc.h: "oa_crc16(d, n) is required to equal
 * oa_crc16_update(OA_CRC16_INIT, d, n)", checked against a split at every
 * offset. An incremental CRC that is subtly not the same function as the whole
 * buffer one is a bug that only appears on the packets that happen to straddle
 * a boundary. */
static void test_incremental_matches_whole_buffer(void)
{
    uint8_t buf[64];
    size_t  split;

    fill_pattern(buf, sizeof buf, 0x5Au);

    for (split = 0; split <= sizeof buf; split++) {
        uint16_t whole = oa_crc16(buf, sizeof buf);
        uint16_t piece = oa_crc16_update(OA_CRC16_INIT, buf, split);

        piece = oa_crc16_update(piece, buf + split, sizeof buf - split);

        assert(whole == piece);
    }
}

/* CLAIM: this implementation computes the same function as the reference
 * decoder published in the spec, which is what a receiver will run. Checked
 * over every length from 0 to the longest packet in version 1, and over
 * several byte patterns including all zeros and all ones, because a CRC that
 * ignores leading zero bytes is a classic wrong initial value. */
static void test_agrees_with_the_spec_reference(void)
{
    uint8_t buf[OA_CRC16_BYTES + 32];
    size_t  len;

    for (len = 0; len <= sizeof buf; len++) {
        memset(buf, 0x00, sizeof buf);
        assert(oa_crc16(buf, len) == ref_crc16(buf, len));

        memset(buf, 0xFF, sizeof buf);
        assert(oa_crc16(buf, len) == ref_crc16(buf, len));

        fill_pattern(buf, sizeof buf, 0xA3u);
        assert(oa_crc16(buf, len) == ref_crc16(buf, len));
    }
}

/* CLAIM: "A receiver must discard any packet whose CRC does not match."
 *
 * That is only worth requiring if the CRC actually changes when the packet
 * does. Every single bit flip in a 22 byte buffer, the length of the longest
 * packet in version 1, must change the checksum. This is the property the two
 * bytes are spent on. */
static void test_every_single_bit_flip_changes_the_crc(void)
{
    uint8_t  buf[22];
    uint16_t original;
    size_t   i;

    fill_pattern(buf, sizeof buf, 0x11u);
    original = oa_crc16(buf, sizeof buf);

    for (i = 0; i < sizeof buf * 8u; i++) {
        uint8_t mask = (uint8_t)(1u << (i % 8u));

        buf[i / 8u] = (uint8_t)(buf[i / 8u] ^ mask);
        assert(oa_crc16(buf, sizeof buf) != original);
        buf[i / 8u] = (uint8_t)(buf[i / 8u] ^ mask);
    }

    assert(oa_crc16(buf, sizeof buf) == original);
}

/* CLAIM: with no output reflection and no final XOR, running the CRC on over
 * the message followed by its own checksum, most significant byte first,
 * leaves the register at zero. That residue is a property of the parameter set
 * rather than of this code, so it is an independent check on the arithmetic
 * that does not go through the reference implementation at all.
 *
 * Note that the packet puts the CRC on the wire little-endian, which is a
 * separate decision about byte order and is checked in test_oa_packet.c. The
 * residue property is about the register, not about the wire. */
static void test_residue_of_message_plus_crc_is_zero(void)
{
    uint8_t  buf[24];
    uint16_t crc;

    fill_pattern(buf, 22, 0x7Eu);
    crc = oa_crc16(buf, 22);

    buf[22] = (uint8_t)(crc >> 8);
    buf[23] = (uint8_t)(crc & 0xFFu);

    assert(oa_crc16(buf, 24) == 0x0000u);
}

/* CLAIM, from oa_crc.h: a NULL buffer is a programming error this function has
 * no way to report, since every 16 bit value it could return is a legal CRC.
 * It contributes nothing and returns the seed. This test records the behaviour
 * so that it is a decision rather than an accident, and so that a later change
 * to it is visible. */
static void test_null_buffer_returns_the_seed(void)
{
    assert(oa_crc16(NULL, 0) == OA_CRC16_INIT);
    assert(oa_crc16(NULL, 8) == OA_CRC16_INIT);
    assert(oa_crc16_update(0x1234u, NULL, 8) == 0x1234u);
}

int main(void)
{
    test_catalogue_check_value();
    test_empty_input_is_the_seed();
    test_incremental_matches_whole_buffer();
    test_agrees_with_the_spec_reference();
    test_every_single_bit_flip_changes_the_crc();
    test_residue_of_message_plus_crc_is_zero();
    test_null_buffer_returns_the_seed();

    printf("test_oa_crc: all checks passed\n");
    return 0;
}

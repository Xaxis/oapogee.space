/*
 * oApogee downlink packet builders.
 *
 * One build function per packet type, writing a complete packet into a buffer
 * the caller owns. There is no allocation, no packet object, and no partial
 * state between calls: a packet is a pure function of a header and a body.
 *
 * The enumeration, the length constants, the body structs and the build
 * function declarations below are all expanded from
 * oapogee/oa_packet_fields.def, which is expanded again by the encoder and again
 * by the conformance test that dumps the layout. A field that moves moves in one
 * place, and everything that disagreed with it fails to build or fails a test.
 *
 * Source of truth: docs/spec/telemetry-packet.md, spec_version 1.
 *
 * THERE IS NO DECODER HERE, AND NO PARSER. The payload builds packets and
 * transmits them. It never interprets a packet it did not build, because there
 * is nothing on the air it should obey. See firmware/SAFETY.md.
 *
 * Nothing in this file has run on hardware, and no byte of this format has been
 * observed on a radio.
 */

#ifndef OAPOGEE_OA_PACKET_H
#define OAPOGEE_OA_PACKET_H

#include "oa_states.h"
#include "oapogee/oa_crc.h"
#include "oapogee/oa_flags.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Format constants.
 *
 * These are the format, not tuning. They belong in code.
 * ------------------------------------------------------------------------ */

/* High nibble of byte 0. A receiver must reject a version it does not know
 * rather than attempting to decode it, because field offsets are not guaranteed
 * stable across versions. */
#define OA_PROTOCOL_VERSION (1u)

#define OA_PACKET_HEADER_BYTES (8u)
#define OA_PACKET_CRC_BYTES    (OA_CRC16_BYTES)

/* The longest packet in version 1 is BEACON at 22 bytes. A caller that sizes a
 * buffer with this can build any packet type into it. The _Static_asserts below
 * check it against the table rather than trusting this line. */
#define OA_PACKET_MAX_BYTES (22u)

/* pad_pressure_pa_off in STATUS is offset so the useful range fills 16 bits:
 * the transmitted value is pressure_pa - 50000, covering 50000 to 115535 Pa in
 * 1 Pa steps. Without the offset a u16 of whole pascals would stop at 65535 Pa,
 * below the pressure at any launch site anyone flies from. */
#define OA_PAD_PRESSURE_OFFSET_PA (50000)
#define OA_PAD_PRESSURE_MIN_PA    (50000)
#define OA_PAD_PRESSURE_MAX_PA    (115535)

/* Latitude and longitude carry this when there is no fix, in both BEACON and
 * POSITION. Zero is a real coordinate, which is why it cannot be the sentinel.
 * GNSS_FIX in the header is authoritative and a receiver must check it. */
#define OA_LATLON_NO_FIX (INT32_MIN)

/* ---------------------------------------------------------------------------
 * Packet types, and their lengths, from the table.
 * ------------------------------------------------------------------------ */

/* 0x0 and 0x6 to 0xF are reserved. There are no enumerators for them: a reserved
 * type is not something the payload can be asked to send. */
typedef enum {
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) OA_PKT_##SYM = (code),
#include "oapogee/oa_packet_fields.def"
    OA_PKT_TYPE_COUNT = 5
} oa_packet_type_t;

/* Per-type lengths as enumerators, because the preprocessor cannot emit a
 * #define from a macro expansion and a hand-written parallel list of lengths is
 * exactly the drift this table exists to prevent. */
enum {
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) \
    OA_PKT_##SYM##_BODY_BYTES = (body_len),                    \
    OA_PKT_##SYM##_TOTAL_BYTES = (total_len),
#include "oapogee/oa_packet_fields.def"
    OA_PKT_LENGTHS_END = 0
};

/* header + body + CRC must equal the stated total for every type, and no type
 * may exceed the buffer size callers are told to allocate. The spec states body
 * and total separately, so the arithmetic between them is checkable, and this is
 * where it gets checked: at compile time, on every build, for free. */
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len)                      \
    _Static_assert((body_len) + (int)OA_PACKET_HEADER_BYTES + (int)OA_PACKET_CRC_BYTES \
                       == (total_len),                                              \
                   "packet " #SYM ": header + body + CRC does not equal the stated total"); \
    _Static_assert((total_len) <= (int)OA_PACKET_MAX_BYTES,                          \
                   "packet " #SYM " is longer than OA_PACKET_MAX_BYTES");
#include "oapogee/oa_packet_fields.def"

/* ---------------------------------------------------------------------------
 * The header a caller supplies.
 *
 * Byte 0 is not in this struct. It is the version nibble and the type nibble,
 * and the builder derives it from the function that was called. A caller that
 * could set it could transmit a FLIGHT body under a STATUS type byte, and every
 * receiver would decode the result as a valid packet.
 *
 * t_ms is milliseconds since the transition into ARMED. The builders transmit 0
 * when `state` is OA_STATE_PAD_IDLE regardless of what this field holds, because
 * the spec requires it and because the payload's uptime counter is a firmware
 * implementation detail that is never on the air.
 * ------------------------------------------------------------------------ */

typedef struct {
    uint8_t  flags; /* OA_FLAG_* bits. Bit 7 is cleared by the builder. */
    uint8_t  seq;   /* Increments per transmitted packet, wraps at 255. */
    uint8_t  state; /* oa_state_t value, or a reserved value passed through. */
    uint32_t t_ms;  /* Milliseconds since arming. Sent as 0 in PAD_IDLE. */
} oa_packet_header_t;

/* ---------------------------------------------------------------------------
 * Body structs, one per packet type, generated from the table.
 *
 * These are host-order decoded values, not overlays. The packet is written field
 * by field in little-endian order, never by copying one of these structs, because
 * a struct overlay depends on packing attributes and on the host's endianness and
 * this format is meant to be implementable anywhere.
 * ------------------------------------------------------------------------ */

#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) typedef struct {
#define OA_PKT_BODY_FIELD(name, wire, offset) oa_##wire##_t name;
#define OA_PKT_TYPE_END(SYM, sym) \
    }                             \
    oa_##sym##_body_t;
#include "oapogee/oa_packet_fields.def"

/* ---------------------------------------------------------------------------
 * Builders, one per packet type, generated from the table.
 *
 * Each writes exactly OA_PKT_<SYM>_TOTAL_BYTES bytes into `out` and stores that
 * length in `*out_len`. The contract every builder satisfies:
 *
 *   1. byte 0 is (OA_PROTOCOL_VERSION << 4) | <this type's code>
 *   2. flags are masked with OA_FLAG_TRANSMIT_MASK, so bit 7 goes out as 0
 *   3. t_ms is written as 0 when hdr->state is OA_STATE_PAD_IDLE
 *   4. every multi-byte field is little-endian, with no padding and no alignment
 *   5. the last two bytes are oa_crc16 over all preceding bytes, little-endian
 *   6. nothing is written to `out` at all if the buffer is too small
 *   7. out_len may be NULL; every other pointer argument may not
 *
 * Returns OA_OK, OA_ERR_NULL, or OA_ERR_BUFFER. A builder never fails on the
 * content of a body: an out of range value is clamped by the field's encoder and
 * flagged, because a packet that is not sent carries no information at all.
 *
 * Rule 6 is not tidiness. A caller that reuses one buffer for every packet type
 * would otherwise be handed a half-overwritten previous packet with a valid CRC
 * from the previous build still on the end of it.
 * ------------------------------------------------------------------------ */

#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len)   \
    oa_result_t oa_packet_build_##sym(const oa_packet_header_t *hdr, \
                                      const oa_##sym##_body_t *body, \
                                      uint8_t *out,                  \
                                      size_t out_cap,                \
                                      size_t *out_len);
#include "oapogee/oa_packet_fields.def"

/* ---------------------------------------------------------------------------
 * Field encoders that are shared, and length queries.
 * ------------------------------------------------------------------------ */

/* Byte 0. Exposed so the conformance test can check it against the builders
 * rather than restating the nibble arithmetic. */
uint8_t oa_packet_hdr_byte(uint8_t version, oa_packet_type_t type);

/* pressure_pa to the STATUS field encoding. A reading below
 * OA_PAD_PRESSURE_MIN_PA encodes as 0 and one above OA_PAD_PRESSURE_MAX_PA
 * encodes as 65535, and *out_baro_fault is set true in either case so the caller
 * can raise OA_FLAG_BARO_FAULT. out_baro_fault may not be NULL: a caller that did
 * not want to know would be transmitting a clamped reference pressure as if it
 * were a measurement, and the reference is the zero the whole altitude column is
 * measured against. */
uint16_t oa_packet_encode_pad_pressure(int32_t pressure_pa, bool *out_baro_fault);

/* Total and body length for a type, or 0 if the type is not one this version
 * defines. Zero rather than an error code because every caller of these is
 * sizing a buffer, and 0 fails that safely. */
size_t oa_packet_total_bytes(oa_packet_type_t type);
size_t oa_packet_body_bytes(oa_packet_type_t type);

/* The name from the spec table, "STATUS" and so on, for the serial console and
 * for test failure messages. NULL for an undefined type. */
const char *oa_packet_type_name(oa_packet_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_PACKET_H */

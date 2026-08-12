/*
 * oApogee core: the downlink packet builders.
 *
 * Five builders, one per packet type, all generated from
 * oapogee/oa_packet_fields.def. The table carries each field's wire type and
 * its absolute byte offset, exactly as docs/spec/telemetry-packet.md states
 * them, and this file pastes those two columns into a little-endian store. A
 * field that moves in the table moves in the encoder, in the body struct, and
 * in the length assertions, in one edit.
 *
 * THERE IS NO DECODER HERE. The payload builds packets and transmits them, and
 * never interprets a packet it did not build, because there is nothing on the
 * air it should obey. See firmware/SAFETY.md.
 *
 * Nothing in this file has run on hardware, and no byte of this format has been
 * observed on a radio.
 */

#include "oapogee/oa_packet.h"

/* ---------------------------------------------------------------------------
 * Little-endian stores.
 *
 * Field by field, byte by byte, never by copying a struct over the buffer. A
 * struct overlay would depend on the compiler's packing attributes and on the
 * host being little-endian, and this format is meant to be implementable
 * anywhere by anyone reading the spec. These functions are the only place in
 * the encoder that knows what "little-endian" means.
 *
 * The names are oa_put_<wire> so the field table can paste them: a line in the
 * table says i32 and this file expands it to oa_put_i32 without a second
 * column that could disagree.
 * ------------------------------------------------------------------------ */

static void oa_put_u8(uint8_t *out, size_t off, oa_u8_t v)
{
    out[off] = v;
}

static void oa_put_u16(uint8_t *out, size_t off, oa_u16_t v)
{
    out[off + 0u] = (uint8_t)(v & 0xFFu);
    out[off + 1u] = (uint8_t)((v >> 8) & 0xFFu);
}

static void oa_put_u32(uint8_t *out, size_t off, oa_u32_t v)
{
    out[off + 0u] = (uint8_t)(v & 0xFFu);
    out[off + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    out[off + 2u] = (uint8_t)((v >> 16) & 0xFFu);
    out[off + 3u] = (uint8_t)((v >> 24) & 0xFFu);
}

/* The signed stores convert to unsigned and reuse the store above. The spec
 * says signed fields are two's complement, and conversion to an unsigned type
 * of the same width is defined to produce exactly that bit pattern, so this is
 * the portable spelling of "write the bytes". It is also why INT32_MIN, the no
 * fix sentinel, needs no special case: it converts to 0x80000000 and goes out
 * as 00 00 00 80, which is what the reference decoder's "<i" reads back. */
static void oa_put_i16(uint8_t *out, size_t off, oa_i16_t v)
{
    oa_put_u16(out, off, (oa_u16_t)v);
}

static void oa_put_i32(uint8_t *out, size_t off, oa_i32_t v)
{
    oa_put_u32(out, off, (oa_u32_t)v);
}

/* ---------------------------------------------------------------------------
 * The 8 byte header, identical in every packet type.
 * ------------------------------------------------------------------------ */

/* Byte 0 is not in oa_packet_header_t: it is derived from the builder that was
 * called, so a caller cannot transmit a FLIGHT body under a STATUS type byte.
 * This struct is the header as it goes on the wire, including that byte, built
 * from the same table that states the offsets. */
#define OA_PKT_HDR_FIELD(name, wire, offset) oa_##wire##_t name;
typedef struct {
#include "oapogee/oa_packet_fields.def"
} oa_header_wire_t;

uint8_t oa_packet_hdr_byte(uint8_t version, oa_packet_type_t type)
{
    /* Both nibbles are masked. Version and type are each small enough that a
     * whole byte for each would be a byte per packet spent on nothing, and the
     * mask is what stops an out of range value in either one silently
     * corrupting the other. */
    return (uint8_t)(((unsigned)version & 0x0Fu) << 4 | ((unsigned)type & 0x0Fu));
}

static void oa_write_header(uint8_t *out, oa_packet_type_t type, const oa_packet_header_t *src)
{
    oa_header_wire_t w;

    w.hdr   = oa_packet_hdr_byte((uint8_t)OA_PROTOCOL_VERSION, type);
    w.flags = oa_flags_sanitise(src->flags);
    w.seq   = src->seq;

    /* The state byte is passed through unchanged, including a value this
     * firmware does not define. Values 7 to 255 are reserved and a receiver
     * meeting one displays the number rather than guessing, so an encoder that
     * rejected or rewrote one here would be deciding something the format
     * deliberately left to the receiver. */
    w.state = src->state;

    /* Before arming there is no elapsed time to report, so t_ms is transmitted
     * as 0 in every packet whose state is PAD_IDLE, whatever the caller passed.
     * It is not the power-on uptime: the payload's uptime counter is a firmware
     * implementation detail and is never on the air. Forcing it here rather
     * than trusting call sites is the point, because a run of pad packets
     * carrying an uptime would let a receiver compute an interval across the
     * transition out of PAD_IDLE, which the spec forbids. */
    w.t_ms = (src->state == (uint8_t)OA_STATE_PAD_IDLE) ? 0u : src->t_ms;

#define OA_PKT_HDR_FIELD(name, wire, offset) oa_put_##wire(out, (size_t)(offset), w.name);
#include "oapogee/oa_packet_fields.def"
}

static void oa_append_crc(uint8_t *out, size_t total_bytes)
{
    /* Over all preceding bytes of the packet, header and body, stored
     * little-endian like every other multi-byte field. */
    const size_t covered = total_bytes - (size_t)OA_PACKET_CRC_BYTES;

    oa_put_u16(out, covered, oa_crc16(out, covered));
}

/* ---------------------------------------------------------------------------
 * Field encoders that are shared.
 * ------------------------------------------------------------------------ */

uint16_t oa_packet_encode_pad_pressure(int32_t pressure_pa, bool *out_baro_fault)
{
    bool     fault = false;
    uint16_t encoded;

    /* The transmitted value is pressure_pa - 50000, and the 65536 representable
     * values cover 50000 to 115535 Pa in 1 Pa steps. The clamp is applied to
     * the pressure, before the offset is removed, so that the two endpoints are
     * the pressures the spec names rather than whatever the subtraction would
     * have wrapped to. */
    if (pressure_pa < OA_PAD_PRESSURE_MIN_PA) {
        fault   = true;
        encoded = 0u;
    } else if (pressure_pa > OA_PAD_PRESSURE_MAX_PA) {
        fault   = true;
        encoded = 65535u;
    } else {
        encoded = (uint16_t)(pressure_pa - OA_PAD_PRESSURE_OFFSET_PA);
    }

    /* Written whether or not a clamp fired, rather than only being raised. This
     * function reports its verdict on one reading and does not accumulate:
     * a caller combining several fault sources ORs them itself, and a caller
     * that only ever saw this flag set would have no way to see it clear again
     * when the barometer recovered. */
    if (out_baro_fault != NULL) {
        *out_baro_fault = fault;
    }

    return encoded;
}

/* ---------------------------------------------------------------------------
 * Length queries and names.
 * ------------------------------------------------------------------------ */

size_t oa_packet_total_bytes(oa_packet_type_t type)
{
    switch (type) {
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) \
    case OA_PKT_##SYM:                                         \
        return (size_t)(total_len);
#include "oapogee/oa_packet_fields.def"
    default:
        break;
    }

    /* Zero rather than an error code, because every caller of this is sizing a
     * buffer and 0 fails that safely. */
    return 0u;
}

size_t oa_packet_body_bytes(oa_packet_type_t type)
{
    switch (type) {
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) \
    case OA_PKT_##SYM:                                         \
        return (size_t)(body_len);
#include "oapogee/oa_packet_fields.def"
    default:
        break;
    }

    return 0u;
}

const char *oa_packet_type_name(oa_packet_type_t type)
{
    switch (type) {
#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len) \
    case OA_PKT_##SYM:                                         \
        return #SYM;
#include "oapogee/oa_packet_fields.def"
    default:
        break;
    }

    /* NULL rather than a placeholder string, so a caller cannot print something
     * that reads like a packet name for a type this version does not define. */
    return NULL;
}

/* ---------------------------------------------------------------------------
 * The builders.
 *
 * One function per type, all five generated from the table, so that adding a
 * field to a packet is an edit to the table and nothing else. The shape is the
 * same every time: check the pointers, check the buffer, write the header,
 * write each body field at its stated offset, append the CRC.
 *
 * The buffer check comes before the first store. That is not tidiness. A caller
 * reusing one buffer for every packet type would otherwise be handed a half
 * overwritten previous packet with the previous build's valid CRC still on the
 * end of it, which is a packet that decodes cleanly and is not true.
 * ------------------------------------------------------------------------ */

#define OA_PKT_TYPE_BEGIN(SYM, sym, code, body_len, total_len)                    \
    oa_result_t oa_packet_build_##sym(const oa_packet_header_t *hdr,              \
                                      const oa_##sym##_body_t  *body,             \
                                      uint8_t                  *out,              \
                                      size_t                    out_cap,          \
                                      size_t                   *out_len)          \
    {                                                                             \
        if (hdr == NULL || body == NULL || out == NULL) {                         \
            return OA_ERR_NULL;                                                   \
        }                                                                         \
        if (out_cap < (size_t)OA_PKT_##SYM##_TOTAL_BYTES) {                       \
            return OA_ERR_BUFFER;                                                 \
        }                                                                         \
                                                                                  \
        oa_write_header(out, OA_PKT_##SYM, hdr);

#define OA_PKT_BODY_FIELD(name, wire, offset) oa_put_##wire(out, (size_t)(offset), body->name);

#define OA_PKT_TYPE_END(SYM, sym)                                                 \
        oa_append_crc(out, (size_t)OA_PKT_##SYM##_TOTAL_BYTES);                   \
                                                                                  \
        if (out_len != NULL) {                                                    \
            *out_len = (size_t)OA_PKT_##SYM##_TOTAL_BYTES;                        \
        }                                                                         \
                                                                                  \
        return OA_OK;                                                             \
    }

#include "oapogee/oa_packet_fields.def"

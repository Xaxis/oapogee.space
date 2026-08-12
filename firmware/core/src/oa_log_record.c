/*
 * oApogee onboard log: fixed-width record packing.
 *
 * The 36 byte flight.bin record and the 16 byte gnss.bin record, packed from
 * oapogee/oa_log_fields.def. Every offset, width and order in the output comes
 * from that table and from nothing else, so the only way to move a field is to
 * move the table line, which moves the manifest with it.
 *
 * Source of truth: docs/spec/log-format.md, spec_version 1. That document is
 * normative. Where it and this file disagree, this file is wrong.
 *
 * The packers write field by field rather than copying a struct over the buffer.
 * A struct overlay would depend on the host's packing rules and the host's
 * endianness, and this format is meant to be written by a microcontroller and
 * read by numpy on somebody's laptop, so the byte order is stated in the code
 * instead of inherited from the machine.
 *
 * NOTHING HERE HAS RUN ON HARDWARE. No board has been fabricated and no flight
 * has been logged, so no byte produced by this file has ever been read back off
 * a flash part.
 */

#include "oapogee/oa_log.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Little-endian stores.
 *
 * Named oa_put_<wire> so that a table line carrying the bare token `i16` can be
 * pasted into the call: the table's type column and the store used for it cannot
 * drift apart, because they are the same token.
 *
 * Signed values are converted to the unsigned type of the same width before
 * being taken apart. That conversion is defined in C and produces the two's
 * complement pattern the spec asks for, whereas shifting a negative value right
 * is implementation defined.
 * ------------------------------------------------------------------------ */

static void oa_put_u8(uint8_t *p, oa_u8_t v)
{
    p[0] = v;
}

static void oa_put_u16(uint8_t *p, oa_u16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void oa_put_u32(uint8_t *p, oa_u32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void oa_put_i16(uint8_t *p, oa_i16_t v)
{
    oa_put_u16(p, (oa_u16_t)v);
}

static void oa_put_i32(uint8_t *p, oa_i32_t v)
{
    oa_put_u32(p, (oa_u32_t)v);
}

/* ---------------------------------------------------------------------------
 * The named fields cover the record exactly.
 *
 * The spec states each field's offset and the record's total width separately,
 * which makes the arithmetic between them checkable. This is where it gets
 * checked: a table line whose width does not fit the stated record length fails
 * the build rather than producing a stream whose records are all shifted by two
 * bytes from the manifest that describes them.
 *
 * This catches a wrong width. It does not catch two fields given the same
 * offset, because a swap of that kind can still sum correctly; the conformance
 * test catches that by checking every field lands where the specification's
 * table says it does.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes) _Static_assert((size_t)0
#define OA_LOG_FIELD(name, wire, offset, scale, unit) +sizeof(oa_##wire##_t)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit) +((size_t)(count) * sizeof(oa_##wire##_t))
#define OA_LOG_STREAM_END(SYM, sym)                       \
    == (size_t)OA_LOG_##SYM##_RECORD_BYTES,               \
    "log stream " #sym ": the fields in oa_log_fields.def do not add up to the record length the spec states");
#include "oapogee/oa_log_fields.def"

/* ---------------------------------------------------------------------------
 * The packers.
 *
 * Contract, from oa_log.h:
 *   1. every field lands at the offset in the table
 *   2. every multi-byte field is little-endian
 *   3. nothing is written at all if the buffer is too small
 *   4. every byte of the record is written
 *
 * Rule 4 is why the buffer is cleared before the fields go in. Both records are
 * fully covered by named fields today, so the clear writes nothing that a field
 * does not immediately overwrite. It is here for the first record that gains a
 * reserved gap, where the byte a caller sees would otherwise be whatever the
 * previous sample left in the buffer, which decodes as data.
 *
 * A record is never validated. The log stores what happened, including a
 * saturated gyro axis, an implausible pressure and a state byte outside the
 * defined range. Deciding a reading is wrong is the fault flags' job, and the
 * flags are recorded beside the reading rather than replacing it.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes)                          \
    oa_result_t oa_log_pack_##sym(const oa_log_##sym##_t *rec, uint8_t *out, size_t out_cap) \
    {                                                                                  \
        if (rec == NULL || out == NULL) {                                              \
            return OA_ERR_NULL;                                                        \
        }                                                                              \
        if (out_cap < (size_t)OA_LOG_##SYM##_RECORD_BYTES) {                           \
            return OA_ERR_BUFFER;                                                      \
        }                                                                              \
        memset(out, 0, (size_t)OA_LOG_##SYM##_RECORD_BYTES);
#define OA_LOG_FIELD(name, wire, offset, scale, unit) oa_put_##wire(out + (offset), rec->name);
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)                             \
    for (size_t oa_idx = 0; oa_idx < (size_t)(count); oa_idx++) {                            \
        oa_put_##wire(out + (offset) + (oa_idx * sizeof(oa_##wire##_t)), rec->name[oa_idx]); \
    }
#define OA_LOG_STREAM_END(SYM, sym) \
    return OA_OK;                   \
    }
#include "oapogee/oa_log_fields.def"

/* ---------------------------------------------------------------------------
 * Pack and push in one call.
 *
 * The space check before the write is the reason oa_sink_t has
 * space_remaining(). Truncation mid-record is legal in this format and readers
 * are required to handle it, but it is meant to describe a payload that lost
 * power on impact. A file that stops mid-record because the flash filled up
 * would be indistinguishable from that, so a record that does not fit is not
 * started, and the caller gets OA_ERR_SINK_FULL to raise OA_FLAG_LOG_FULL with.
 * A flight does not end because the flash filled up.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes)                       \
    oa_result_t oa_log_write_##sym(const oa_log_##sym##_t *rec, oa_sink_t *sink)    \
    {                                                                               \
        uint8_t     buf[OA_LOG_##SYM##_RECORD_BYTES];                               \
        oa_result_t res;                                                            \
        if (rec == NULL || sink == NULL) {                                          \
            return OA_ERR_NULL;                                                     \
        }                                                                           \
        if (oa_sink_space_remaining(sink) < sizeof buf) {                           \
            return OA_ERR_SINK_FULL;                                                \
        }                                                                           \
        res = oa_log_pack_##sym(rec, buf, sizeof buf);                              \
        if (res != OA_OK) {                                                         \
            return res;                                                             \
        }                                                                           \
        return oa_sink_write(sink, buf, sizeof buf);                                \
    }
#define OA_LOG_FIELD(name, wire, offset, scale, unit)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)
#include "oapogee/oa_log_fields.def"

/* ---------------------------------------------------------------------------
 * Stream metadata.
 *
 * Switched on the enumerator rather than indexed into an array, so the mapping
 * is by name: OA_LOG_STREAM_FLIGHT reaches the FLIGHT line of the table because
 * the two spellings are pasted together, not because they happen to be in the
 * same position in two lists.
 *
 * The filename here is the one the manifest prints, because both come from the
 * same table line. A manifest that named a file the port layer did not open
 * would send a reader looking for a stream that is not in the directory.
 * ------------------------------------------------------------------------ */

const char *oa_log_stream_filename(oa_log_stream_t stream)
{
    switch (stream) {
#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes) \
    case OA_LOG_STREAM_##SYM:                                 \
        return filename;
#define OA_LOG_FIELD(name, wire, offset, scale, unit)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)
#include "oapogee/oa_log_fields.def"
        default:
            break;
    }

    /* NULL rather than a placeholder: a caller that printed "unknown.bin" would
     * have written a manifest naming a file nobody will ever create. */
    return NULL;
}

size_t oa_log_stream_record_bytes(oa_log_stream_t stream)
{
    switch (stream) {
#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes) \
    case OA_LOG_STREAM_##SYM:                                 \
        return (size_t)OA_LOG_##SYM##_RECORD_BYTES;
#define OA_LOG_FIELD(name, wire, offset, scale, unit)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)
#include "oapogee/oa_log_fields.def"
        default:
            break;
    }

    /* Zero, because every caller of this is sizing a buffer or dividing a file
     * length by it, and zero fails both of those loudly. */
    return 0;
}

/* TODO(confirm): the log format says a GNSS altitude outside the range of an
 * i16 of metres is clamped to the endpoint rather than allowed to wrap, because
 * a wrapped value decodes as a plausible altitude with the wrong sign. Nothing
 * in core can enforce that: oa_log_gnss_t.alt_m is already an i16, so the
 * narrowing has happened before the record reaches this file. The clamp belongs
 * wherever the receiver's own wider altitude is converted, which is the port
 * layer, and there is no port layer yet. Decide whether core should own that
 * conversion so the clamp has a tested home, and record the decision in
 * docs/open-questions.md. */

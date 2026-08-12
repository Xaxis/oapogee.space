/*
 * oApogee pre-launch ring buffer.
 *
 * A fixed-capacity ring of packed, fixed-width records that runs continuously
 * from ARMED and is drained into the log when launch is detected.
 *
 * WHY IT EXISTS
 *
 * Launch detection is inherently late. It cannot fire until enough acceleration
 * has accumulated to be distinguishable from someone knocking the launch rail,
 * and by then the interesting first fraction of a second is already past.
 * Without this buffer, every log would start shortly after ignition and the
 * moment of ignition would be missing from all of them. The buffer is what
 * recovers the part that would otherwise be lost.
 *
 * It holds packed records rather than decoded structs, so draining it is a
 * straight copy into the sink with no repacking. Packing at push time also means
 * the cost of packing is paid on the pad, at the pad sample rate, rather than
 * all at once at the busiest moment of the flight.
 *
 * NO ALLOCATION. The caller supplies the storage. How many records to hold
 * follows from prelaunch_ring_ms and the sample rate, both of which are
 * unmeasured and live in oa_config_t as unset, so this buffer has no opinion
 * about its own size: it is told.
 *
 * Nothing in this file has run on hardware.
 */

#ifndef OAPOGEE_OA_RINGBUF_H
#define OAPOGEE_OA_RINGBUF_H

#include "oapogee/oa_sink.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *storage;          /* Caller-owned. Never freed, never reallocated. */
    size_t   record_bytes;
    size_t   capacity_records;
    size_t   head;             /* Index of the next slot to write. */
    size_t   count;            /* Records currently held, up to capacity. */
    uint64_t pushed_total;     /* Every push ever, including overwritten ones. */
} oa_ringbuf_t;

/* Bind a buffer to caller-owned storage.
 *
 * capacity_records is storage_bytes / record_bytes, rounded down. Returns
 * OA_ERR_RANGE if that is zero: a ring that cannot hold one record is a
 * configuration mistake, and silently accepting it would mean the pre-launch
 * window was empty for a reason nobody could see in the log. */
oa_result_t oa_ringbuf_init(oa_ringbuf_t *rb, void *storage, size_t storage_bytes, size_t record_bytes);

/* Append one packed record of exactly record_bytes.
 *
 * When the ring is full this overwrites the oldest record and returns OA_OK.
 * Overwriting is the correct behaviour and not a degraded one: the buffer's
 * purpose is to hold the most recent window, and a full ring on the pad is the
 * normal case for every flight that waits more than a moment before launching. */
oa_result_t oa_ringbuf_push(oa_ringbuf_t *rb, const void *packed_record);

/* Borrow a record without removing it. index 0 is the oldest held record.
 * Returns OA_ERR_EMPTY when index is past the end. The pointer is valid until
 * the next push. */
oa_result_t oa_ringbuf_peek(const oa_ringbuf_t *rb, size_t index, const uint8_t **out_record);

size_t oa_ringbuf_count(const oa_ringbuf_t *rb);
size_t oa_ringbuf_capacity(const oa_ringbuf_t *rb);
bool   oa_ringbuf_is_full(const oa_ringbuf_t *rb);

/* Records pushed since init, including ones that were overwritten. Recorded so
 * the log can say how much of the pre-launch window was actually retained, which
 * is the difference between a buffer that was long enough and one that was not. */
uint64_t oa_ringbuf_pushed_total(const oa_ringbuf_t *rb);

/* Write every held record into the sink, oldest first, then reset to empty.
 *
 * On a sink failure this stops at the record that failed and returns the sink's
 * result, leaving the records it has not yet written still in the buffer. It
 * does not reset. A partial drain that also emptied the buffer would silently
 * discard the part of the pre-launch window that did not fit, which is precisely
 * the data this buffer exists to preserve. */
oa_result_t oa_ringbuf_drain(oa_ringbuf_t *rb, oa_sink_t *sink);

/* Discard everything. Does not touch the storage. */
void oa_ringbuf_reset(oa_ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_RINGBUF_H */

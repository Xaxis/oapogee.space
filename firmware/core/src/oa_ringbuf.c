/*
 * oApogee core: the pre-launch ring buffer.
 *
 * Implements oapogee/oa_ringbuf.h. Read that header for why this exists: launch
 * detection is late by construction, and this buffer is what puts the moment of
 * ignition back into the log.
 *
 * THERE ARE NO NUMBERS IN THIS FILE. Not a capacity, not a record width, not a
 * window length. The caller supplies the storage and the record size, and how
 * many records that works out to is arithmetic rather than a decision. The
 * window this buffer covers follows from prelaunch_ring_ms and the sample rate,
 * both of which are unmeasured and live in oa_config_t as unset.
 *
 * NO DIVISION OUTSIDE oa_ringbuf_init. The wrap is done with a conditional
 * subtraction rather than a modulo, because push runs at the sample rate in an
 * interrupt-adjacent context, and a 32-bit divide is a compiler runtime helper
 * on any part that lacks a divider. core/allowed-undefined.txt is four entries
 * long on purpose, and a helper appearing there would be a fact about this
 * function worth reading rather than a list worth extending.
 *
 * Nothing in this file has run on hardware.
 */

#include "oapogee/oa_ringbuf.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Internals.
 * ------------------------------------------------------------------------ */

/* Index of the oldest held record.
 *
 * Derived from head and count rather than stored alongside them. Two stored
 * indices can disagree after a wrap, and the failure mode of that disagreement
 * is a drained buffer whose records are in the wrong order, which reads as a
 * plausible flight with the first fraction of a second scrambled. One derived
 * value cannot disagree with itself.
 *
 * Written as a branch rather than (head + capacity - count) % capacity because
 * that form can overflow size_t for a pathologically large capacity, and because
 * it is a division. Both branches stay inside [0, capacity). */
static size_t oa_ringbuf_tail(const oa_ringbuf_t *rb)
{
    if (rb->head >= rb->count) {
        return rb->head - rb->count;
    }
    return rb->capacity_records - (rb->count - rb->head);
}

/* Storage offset of the record `index` places after `tail`, wrapping once.
 *
 * `index` is always less than count, and count is never more than capacity, so
 * exactly one wrap is possible and a conditional subtraction is sufficient. The
 * subtraction is expressed against the distance to the end of the storage so
 * that no intermediate sum can overflow. */
static size_t oa_ringbuf_slot(const oa_ringbuf_t *rb, size_t tail, size_t index)
{
    size_t to_end = rb->capacity_records - tail;

    if (index < to_end) {
        return tail + index;
    }
    return index - to_end;
}

static uint8_t *oa_ringbuf_record_at(const oa_ringbuf_t *rb, size_t slot)
{
    return rb->storage + (slot * rb->record_bytes);
}

/* ---------------------------------------------------------------------------
 * Interface.
 * ------------------------------------------------------------------------ */

oa_result_t oa_ringbuf_init(oa_ringbuf_t *rb, void *storage, size_t storage_bytes, size_t record_bytes)
{
    size_t capacity;

    if (rb == NULL || storage == NULL) {
        return OA_ERR_NULL;
    }
    if (record_bytes == 0u) {
        return OA_ERR_RANGE;
    }

    capacity = storage_bytes / record_bytes;
    if (capacity == 0u) {
        /* A ring that cannot hold one record is a configuration mistake, and
         * accepting it silently would mean the pre-launch window was empty for a
         * reason nobody could see in the log. */
        return OA_ERR_RANGE;
    }

    rb->storage          = (uint8_t *)storage;
    rb->record_bytes     = record_bytes;
    rb->capacity_records = capacity;
    rb->head             = 0u;
    rb->count            = 0u;
    rb->pushed_total     = 0u;

    /* The storage is deliberately not cleared. It is caller-owned, it may be
     * large, and no slot is ever read before it has been written, because peek
     * and drain are both bounded by count. Clearing it would cost a memset of
     * the whole ring at arming time to make no observable difference. */
    return OA_OK;
}

oa_result_t oa_ringbuf_push(oa_ringbuf_t *rb, const void *packed_record)
{
    if (rb == NULL || packed_record == NULL) {
        return OA_ERR_NULL;
    }
    if (rb->storage == NULL || rb->capacity_records == 0u || rb->record_bytes == 0u) {
        /* Not initialised, or initialised into a struct that was later zeroed.
         * Reported rather than dereferenced. */
        return OA_ERR_STATE;
    }

    memcpy(oa_ringbuf_record_at(rb, rb->head), packed_record, rb->record_bytes);

    rb->head++;
    if (rb->head == rb->capacity_records) {
        rb->head = 0u;
    }

    if (rb->count < rb->capacity_records) {
        rb->count++;
    }
    /* When count is already at capacity, head has just moved past the oldest
     * record and that record is the one this push overwrote. count stays put and
     * the tail advances with the head, which is the whole behaviour: the buffer
     * holds the most recent window, and a full ring on the pad is the normal case
     * for every flight that waits more than a moment before launching. */

    rb->pushed_total++;
    return OA_OK;
}

oa_result_t oa_ringbuf_peek(const oa_ringbuf_t *rb, size_t index, const uint8_t **out_record)
{
    if (rb == NULL || out_record == NULL) {
        return OA_ERR_NULL;
    }
    if (rb->storage == NULL || rb->capacity_records == 0u) {
        return OA_ERR_STATE;
    }
    if (index >= rb->count) {
        return OA_ERR_EMPTY;
    }

    *out_record = oa_ringbuf_record_at(rb, oa_ringbuf_slot(rb, oa_ringbuf_tail(rb), index));
    return OA_OK;
}

size_t oa_ringbuf_count(const oa_ringbuf_t *rb)
{
    return (rb == NULL) ? 0u : rb->count;
}

size_t oa_ringbuf_capacity(const oa_ringbuf_t *rb)
{
    return (rb == NULL) ? 0u : rb->capacity_records;
}

bool oa_ringbuf_is_full(const oa_ringbuf_t *rb)
{
    if (rb == NULL || rb->capacity_records == 0u) {
        return false;
    }
    return rb->count == rb->capacity_records;
}

uint64_t oa_ringbuf_pushed_total(const oa_ringbuf_t *rb)
{
    return (rb == NULL) ? 0u : rb->pushed_total;
}

oa_result_t oa_ringbuf_drain(oa_ringbuf_t *rb, oa_sink_t *sink)
{
    size_t tail;
    size_t written = 0u;

    if (rb == NULL || sink == NULL) {
        return OA_ERR_NULL;
    }
    if (rb->storage == NULL || rb->capacity_records == 0u) {
        return OA_ERR_STATE;
    }

    /* count does not change inside the loop, so the tail is stable and is
     * computed once. */
    tail = oa_ringbuf_tail(rb);

    while (written < rb->count) {
        const size_t slot = oa_ringbuf_slot(rb, tail, written);
        oa_result_t  r    = oa_sink_write(sink, oa_ringbuf_record_at(rb, slot), rb->record_bytes);

        if (r != OA_OK) {
            /* Drop exactly what the sink accepted and keep the rest. Emptying the
             * buffer here would discard the part of the pre-launch window that
             * did not fit, which is precisely the data this buffer exists to
             * preserve. The caller may retry, or may raise OA_FLAG_LOG_FULL and
             * keep flying, and either way the records are still here.
             *
             * The failing record is counted as not written: a short write is a
             * failure and not a partial success, so the sink may hold none of it
             * or some of it, and re-sending a whole record is the only recovery
             * that keeps a flat array of fixed-width records flat. */
            rb->count -= written;
            return r;
        }
        written++;
    }

    oa_ringbuf_reset(rb);
    return OA_OK;
}

void oa_ringbuf_reset(oa_ringbuf_t *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->head  = 0u;
    rb->count = 0u;

    /* pushed_total survives a reset. It is documented as records pushed since
     * init, and it is read after the drain to say how much of the pre-launch
     * window was retained rather than overwritten. Clearing it here would erase
     * that number at the exact moment it becomes worth reporting.
     *
     * The storage is not touched, per the header. */
}

/* One record per sink call rather than one call per contiguous run.
 *
 * Two contiguous memcpy-sized writes would be fewer calls, and they would break
 * the guarantee above: a sink that failed part way through a run would leave the
 * buffer unable to say which records the sink took. Writing a record at a time
 * makes "stopped at the record that failed" exactly true. The cost is one call
 * per record on a path that runs once per flight, at launch detection, and that
 * is the cheapest place in the whole firmware to spend it. */

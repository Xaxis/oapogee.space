/*
 * Conformance tests for oa_ringbuf.
 *
 * The claim this buffer exists to make is in data/flight-phases.yaml, under
 * ARMED: "A ring buffer holds the most recent samples continuously, so that when
 * launch is detected the log already contains the moment of ignition rather than
 * starting a fraction of a second after it."
 *
 * Every test below checks one part of that sentence, or one of the guarantees
 * oa_ringbuf.h makes in order to keep it. The two that matter most are ordering,
 * because a drained buffer whose records came out in the wrong order reads as a
 * plausible flight with the first fraction of a second scrambled, and the
 * behaviour of a failed drain, because a partial drain that also emptied the
 * buffer would discard exactly the data this buffer exists to preserve.
 *
 * There are no flight numbers in this file. Capacities and record widths here
 * are chosen to make the arithmetic checkable by hand and are not proposals for
 * anything: how many records a real payload holds follows from prelaunch_ring_ms
 * and the sample rate, both of which are unmeasured and unset.
 *
 * Plain C and assert, no framework. Nothing here has run on hardware.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_ringbuf.h"
#include "oapogee/oa_sink.h"

/* ---------------------------------------------------------------------------
 * A sink that records what it was given, and can be told to fail.
 *
 * The failure case is the interesting one: it is how a flash part that fills up
 * mid-drain is modelled without a flash part.
 * ------------------------------------------------------------------------ */

#define TEST_SINK_CAP 512u

typedef struct {
    uint8_t     bytes[TEST_SINK_CAP];
    size_t      used;
    size_t      writes;
    size_t      fail_after_writes; /* Succeed this many times, then fail. */
    bool        failing;
    oa_result_t fail_with;
} test_sink_ctx_t;

static oa_result_t test_sink_write(void *ctx, const uint8_t *data, size_t len)
{
    test_sink_ctx_t *s = (test_sink_ctx_t *)ctx;

    if (s->failing && s->writes >= s->fail_after_writes) {
        return s->fail_with;
    }
    assert(s->used + len <= TEST_SINK_CAP);
    memcpy(s->bytes + s->used, data, len);
    s->used += len;
    s->writes++;
    return OA_OK;
}

static size_t test_sink_space_remaining(void *ctx)
{
    const test_sink_ctx_t *s = (const test_sink_ctx_t *)ctx;

    return TEST_SINK_CAP - s->used;
}

static oa_result_t test_sink_sync(void *ctx)
{
    (void)ctx;
    return OA_OK;
}

static const oa_sink_vtable_t test_sink_vt = {
    test_sink_write,
    test_sink_space_remaining,
    test_sink_sync
};

static void test_sink_init(test_sink_ctx_t *s, oa_sink_t *sink)
{
    memset(s, 0, sizeof *s);
    s->fail_with = OA_ERR_SINK_FULL;
    sink->vt     = &test_sink_vt;
    sink->ctx    = s;
}

/* ---------------------------------------------------------------------------
 * A record whose contents identify it.
 *
 * Four bytes, all four carrying the same counter, so that a record written at
 * the wrong offset or half copied is visible rather than merely wrong.
 * ------------------------------------------------------------------------ */

#define REC_BYTES 4u

static void make_record(uint8_t rec[REC_BYTES], uint8_t id)
{
    size_t i;

    for (i = 0u; i < REC_BYTES; i++) {
        rec[i] = id;
    }
}

static void push_id(oa_ringbuf_t *rb, uint8_t id)
{
    uint8_t rec[REC_BYTES];

    make_record(rec, id);
    assert(oa_ringbuf_push(rb, rec) == OA_OK);
}

/* The id of the record held at `index`, oldest first. */
static uint8_t peek_id(const oa_ringbuf_t *rb, size_t index)
{
    const uint8_t *rec = NULL;
    size_t         i;

    assert(oa_ringbuf_peek(rb, index, &rec) == OA_OK);
    assert(rec != NULL);
    for (i = 1u; i < REC_BYTES; i++) {
        assert(rec[i] == rec[0]);
    }
    return rec[0];
}

/* ---------------------------------------------------------------------------
 * Tests.
 * ------------------------------------------------------------------------ */

/* CLAIM, from oa_ringbuf.h: "capacity_records is storage_bytes / record_bytes,
 * rounded down. Returns OA_ERR_RANGE if that is zero: a ring that cannot hold
 * one record is a configuration mistake, and silently accepting it would mean
 * the pre-launch window was empty for a reason nobody could see in the log."
 *
 * The rounding case is here because storage sized from an unset window and an
 * unset sample rate is exactly the kind of arithmetic that lands between two
 * record boundaries. */
static void test_init_capacity_and_refusals(void)
{
    uint8_t      storage[32];
    oa_ringbuf_t rb;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    assert(oa_ringbuf_capacity(&rb) == 8u);
    assert(oa_ringbuf_count(&rb) == 0u);
    assert(oa_ringbuf_is_full(&rb) == false);
    assert(oa_ringbuf_pushed_total(&rb) == 0u);

    /* Rounded down, and the remainder is simply unused. */
    assert(oa_ringbuf_init(&rb, storage, 31u, REC_BYTES) == OA_OK);
    assert(oa_ringbuf_capacity(&rb) == 7u);

    /* Not one whole record. */
    assert(oa_ringbuf_init(&rb, storage, 3u, REC_BYTES) == OA_ERR_RANGE);
    assert(oa_ringbuf_init(&rb, storage, 0u, REC_BYTES) == OA_ERR_RANGE);

    /* A record of no bytes is not a record. */
    assert(oa_ringbuf_init(&rb, storage, sizeof storage, 0u) == OA_ERR_RANGE);

    assert(oa_ringbuf_init(NULL, storage, sizeof storage, REC_BYTES) == OA_ERR_NULL);
    assert(oa_ringbuf_init(&rb, NULL, sizeof storage, REC_BYTES) == OA_ERR_NULL);
}

/* CLAIM: "Borrow a record without removing it. index 0 is the oldest held
 * record. Returns OA_ERR_EMPTY when index is past the end." */
static void test_peek_is_oldest_first(void)
{
    uint8_t        storage[16];
    oa_ringbuf_t   rb;
    const uint8_t *rec = NULL;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    assert(oa_ringbuf_peek(&rb, 0u, &rec) == OA_ERR_EMPTY);

    push_id(&rb, 1u);
    push_id(&rb, 2u);
    push_id(&rb, 3u);

    assert(oa_ringbuf_count(&rb) == 3u);
    assert(peek_id(&rb, 0u) == 1u);
    assert(peek_id(&rb, 1u) == 2u);
    assert(peek_id(&rb, 2u) == 3u);
    assert(oa_ringbuf_peek(&rb, 3u, &rec) == OA_ERR_EMPTY);

    assert(oa_ringbuf_peek(&rb, 0u, NULL) == OA_ERR_NULL);
    assert(oa_ringbuf_peek(NULL, 0u, &rec) == OA_ERR_NULL);
}

/* CLAIM: "When the ring is full this overwrites the oldest record and returns
 * OA_OK. Overwriting is the correct behaviour and not a degraded one: the
 * buffer's purpose is to hold the most recent window, and a full ring on the pad
 * is the normal case for every flight that waits more than a moment before
 * launching."
 *
 * Driven well past capacity, and around the wrap more than once, because a wrap
 * that is right the first time and wrong the second is the classic version of
 * this bug. */
static void test_overwrites_oldest_and_stays_in_order(void)
{
    uint8_t      storage[16]; /* Four records. */
    oa_ringbuf_t rb;
    uint8_t      id;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    assert(oa_ringbuf_capacity(&rb) == 4u);

    for (id = 1u; id <= 11u; id++) {
        push_id(&rb, id);

        if (id < 4u) {
            assert(oa_ringbuf_count(&rb) == (size_t)id);
            assert(oa_ringbuf_is_full(&rb) == false);
        } else {
            assert(oa_ringbuf_count(&rb) == 4u);
            assert(oa_ringbuf_is_full(&rb) == true);
        }

        /* The held records are always the last min(id, 4), oldest first. */
        {
            const size_t held  = oa_ringbuf_count(&rb);
            size_t       index = 0u;

            for (index = 0u; index < held; index++) {
                const uint8_t expected = (uint8_t)(id - (uint8_t)(held - 1u - index));

                assert(peek_id(&rb, index) == expected);
            }
        }
    }

    assert(oa_ringbuf_pushed_total(&rb) == 11u);
}

/* CLAIM: "Write every held record into the sink, oldest first, then reset to
 * empty."
 *
 * Oldest first is the whole point. The moment of ignition is the oldest record
 * in the buffer at launch detection, and a drain that wrote newest first would
 * put it after the boost that follows it. */
static void test_drain_writes_in_time_order_then_empties(void)
{
    uint8_t         storage[16];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;
    size_t          i;

    test_sink_init(&ctx, &sink);
    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);

    /* Six pushes into a ring of four, so the drain has to start after the wrap. */
    for (i = 1u; i <= 6u; i++) {
        push_id(&rb, (uint8_t)i);
    }

    assert(oa_ringbuf_drain(&rb, &sink) == OA_OK);

    /* Four records, oldest first: 3, 4, 5, 6. */
    assert(ctx.writes == 4u);
    assert(ctx.used == 4u * REC_BYTES);
    for (i = 0u; i < 4u; i++) {
        const uint8_t expected = (uint8_t)(i + 3u);
        size_t        b;

        for (b = 0u; b < REC_BYTES; b++) {
            assert(ctx.bytes[(i * REC_BYTES) + b] == expected);
        }
    }

    /* Reset to empty, and ready to keep running. */
    assert(oa_ringbuf_count(&rb) == 0u);
    assert(oa_ringbuf_is_full(&rb) == false);

    /* CLAIM: "Records pushed since init, including ones that were overwritten."
     * It survives the drain, because it is read after the drain to say how much
     * of the pre-launch window was retained rather than overwritten, which is
     * the difference between a buffer that was long enough and one that was
     * not. */
    assert(oa_ringbuf_pushed_total(&rb) == 6u);
}

/* CLAIM: "On a sink failure this stops at the record that failed and returns the
 * sink's result, leaving the records it has not yet written still in the buffer.
 * It does not reset."
 *
 * The failing record is counted as not written, because a short write is a
 * failure and not a partial success, so re-sending the whole record is the only
 * recovery that keeps a flat array of fixed-width records flat. */
static void test_failed_drain_keeps_what_the_sink_did_not_take(void)
{
    uint8_t         storage[16];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;
    size_t          i;

    test_sink_init(&ctx, &sink);
    ctx.failing           = true;
    ctx.fail_after_writes = 2u;
    ctx.fail_with         = OA_ERR_SINK_FULL;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    for (i = 1u; i <= 4u; i++) {
        push_id(&rb, (uint8_t)i);
    }

    assert(oa_ringbuf_drain(&rb, &sink) == OA_ERR_SINK_FULL);
    assert(ctx.writes == 2u);

    /* The two the sink took are gone, the two it did not are still here, still
     * oldest first. */
    assert(oa_ringbuf_count(&rb) == 2u);
    assert(peek_id(&rb, 0u) == 3u);
    assert(peek_id(&rb, 1u) == 4u);

    /* Retrying finishes the job rather than duplicating or losing anything. */
    ctx.failing = false;
    assert(oa_ringbuf_drain(&rb, &sink) == OA_OK);
    assert(ctx.writes == 4u);
    assert(oa_ringbuf_count(&rb) == 0u);
    assert(ctx.bytes[2u * REC_BYTES] == 3u);
    assert(ctx.bytes[3u * REC_BYTES] == 4u);
}

/* CLAIM: the same, for a sink that fails on its very first write. Nothing is
 * lost, because nothing was accepted. */
static void test_drain_that_fails_immediately_loses_nothing(void)
{
    uint8_t         storage[16];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;

    test_sink_init(&ctx, &sink);
    ctx.failing           = true;
    ctx.fail_after_writes = 0u;
    ctx.fail_with         = OA_ERR_SINK_IO;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    push_id(&rb, 7u);
    push_id(&rb, 8u);

    assert(oa_ringbuf_drain(&rb, &sink) == OA_ERR_SINK_IO);
    assert(ctx.writes == 0u);
    assert(oa_ringbuf_count(&rb) == 2u);
    assert(peek_id(&rb, 0u) == 7u);
    assert(peek_id(&rb, 1u) == 8u);
}

/* CLAIM: draining an empty buffer is not an error. There is nothing to write,
 * so the sink is not called, and the result is OA_OK.
 *
 * This is the state of the buffer for a payload that armed and launched in less
 * time than one sample, and for the second drain of a flight. Neither is a
 * failure. */
static void test_drain_of_an_empty_buffer(void)
{
    uint8_t         storage[16];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;

    test_sink_init(&ctx, &sink);
    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);

    assert(oa_ringbuf_drain(&rb, &sink) == OA_OK);
    assert(ctx.writes == 0u);
    assert(oa_ringbuf_count(&rb) == 0u);
}

/* CLAIM: "Discard everything. Does not touch the storage."
 *
 * The storage check matters because the buffer is caller-owned and may be shared
 * with nothing at all, and because a reset that cleared it would cost a memset
 * of the whole ring at the busiest moment of the flight. */
static void test_reset_discards_records_but_not_storage_or_history(void)
{
    uint8_t      storage[16];
    oa_ringbuf_t rb;
    uint8_t      before[16];

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    push_id(&rb, 1u);
    push_id(&rb, 2u);
    memcpy(before, storage, sizeof before);

    oa_ringbuf_reset(&rb);

    assert(oa_ringbuf_count(&rb) == 0u);
    assert(oa_ringbuf_pushed_total(&rb) == 2u);
    assert(memcmp(before, storage, sizeof before) == 0);

    /* And it keeps working afterwards, from the start of the storage. */
    push_id(&rb, 9u);
    assert(oa_ringbuf_count(&rb) == 1u);
    assert(peek_id(&rb, 0u) == 9u);
    assert(oa_ringbuf_pushed_total(&rb) == 3u);

    oa_ringbuf_reset(NULL); /* Safe, per the header's NULL discipline. */
}

/* CLAIM: "The pointer is valid until the next push."
 *
 * Checked the only way a test can check it: the bytes behind a borrowed pointer
 * are the record that was borrowed, right up until a push, and the record that
 * push overwrote is the one the ring said was oldest. */
static void test_peek_pointer_tracks_the_storage(void)
{
    uint8_t        storage[8]; /* Two records, so the second push wraps. */
    oa_ringbuf_t   rb;
    const uint8_t *first = NULL;

    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);
    push_id(&rb, 1u);
    assert(oa_ringbuf_peek(&rb, 0u, &first) == OA_OK);
    assert(first[0] == 1u);

    push_id(&rb, 2u);
    assert(first[0] == 1u); /* Slot 0 is untouched: the push went to slot 1. */

    push_id(&rb, 3u);       /* Wraps, overwriting slot 0, which held the oldest. */
    assert(first[0] == 3u);
    assert(peek_id(&rb, 0u) == 2u);
    assert(peek_id(&rb, 1u) == 3u);
}

/* CLAIM: every fallible entry point rejects a NULL rather than dereferencing it,
 * and every accessor answers for a NULL context instead of crashing. A payload
 * that faults while assembling its own log is worse than one that reports
 * nothing. */
static void test_null_discipline(void)
{
    uint8_t         storage[16];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;
    uint8_t         rec[REC_BYTES];

    test_sink_init(&ctx, &sink);
    make_record(rec, 1u);
    assert(oa_ringbuf_init(&rb, storage, sizeof storage, REC_BYTES) == OA_OK);

    assert(oa_ringbuf_push(NULL, rec) == OA_ERR_NULL);
    assert(oa_ringbuf_push(&rb, NULL) == OA_ERR_NULL);
    assert(oa_ringbuf_drain(NULL, &sink) == OA_ERR_NULL);
    assert(oa_ringbuf_drain(&rb, NULL) == OA_ERR_NULL);

    assert(oa_ringbuf_count(NULL) == 0u);
    assert(oa_ringbuf_capacity(NULL) == 0u);
    assert(oa_ringbuf_is_full(NULL) == false);
    assert(oa_ringbuf_pushed_total(NULL) == 0u);
}

/* CLAIM: a buffer that was never initialised, or whose struct was zeroed after
 * it was, reports the state rather than writing through a null storage pointer.
 * Zeroed structs are how this firmware starts every context, so this is a real
 * state and not a hypothetical. */
static void test_uninitialised_buffer_is_reported(void)
{
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;
    uint8_t         rec[REC_BYTES];
    const uint8_t  *out = NULL;

    test_sink_init(&ctx, &sink);
    memset(&rb, 0, sizeof rb);
    make_record(rec, 1u);

    assert(oa_ringbuf_push(&rb, rec) == OA_ERR_STATE);
    assert(oa_ringbuf_peek(&rb, 0u, &out) == OA_ERR_STATE);
    assert(oa_ringbuf_drain(&rb, &sink) == OA_ERR_STATE);
    assert(ctx.writes == 0u);
}

/* CLAIM: the buffer holds packed records rather than decoded structs, so
 * draining is a straight copy into the sink with no repacking. Checked at the
 * width the log format actually uses: docs/spec/log-format.md says a flight.bin
 * record is 36 bytes, and the drain must hand the sink exactly that, exactly
 * once per record, with no header, no padding and no framing of its own.
 *
 * A sink that received 35 or 37 bytes would shift every record after it in a
 * file whose entire readability rests on being a flat array. */
static void test_record_width_is_handed_through_unchanged(void)
{
    uint8_t         storage[36u * 3u];
    uint8_t         record[36];
    oa_ringbuf_t    rb;
    test_sink_ctx_t ctx;
    oa_sink_t       sink;
    size_t          i;

    test_sink_init(&ctx, &sink);
    assert(oa_ringbuf_init(&rb, storage, sizeof storage, 36u) == OA_OK);
    assert(oa_ringbuf_capacity(&rb) == 3u);

    for (i = 0u; i < sizeof record; i++) {
        record[i] = (uint8_t)i;
    }
    assert(oa_ringbuf_push(&rb, record) == OA_OK);
    assert(oa_ringbuf_drain(&rb, &sink) == OA_OK);

    assert(ctx.writes == 1u);
    assert(ctx.used == 36u);
    assert(memcmp(ctx.bytes, record, sizeof record) == 0);
}

int main(void)
{
    test_init_capacity_and_refusals();
    test_peek_is_oldest_first();
    test_overwrites_oldest_and_stays_in_order();
    test_drain_writes_in_time_order_then_empties();
    test_failed_drain_keeps_what_the_sink_did_not_take();
    test_drain_that_fails_immediately_loses_nothing();
    test_drain_of_an_empty_buffer();
    test_reset_discards_records_but_not_storage_or_history();
    test_peek_pointer_tracks_the_storage();
    test_null_discipline();
    test_uninitialised_buffer_is_reported();
    test_record_width_is_handed_through_unchanged();

    printf("test_oa_ringbuf: all checks passed\n");
    return 0;
}

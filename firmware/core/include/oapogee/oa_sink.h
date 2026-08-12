/*
 * oApogee byte sink.
 *
 * The one interface between core and anything that stores or transmits bytes.
 * core writes records and manifests into a sink and knows nothing else: not
 * LittleFS, not a flash part, not a radio, not a file on a laptop.
 *
 * This is the seam that makes core testable with no hardware. The conformance
 * tests point a sink at a fixed buffer and compare the bytes against the spec
 * tables; the firmware points the same code at flash. There is no second
 * implementation of the packing to keep in step, which matters because a log
 * writer that was only exercised through the real filesystem would only be
 * tested on a board that does not exist yet.
 *
 * A vtable rather than a function pointer per call site, and a caller-owned
 * context pointer rather than a global, because core has no global mutable
 * state. Nothing here allocates.
 *
 * Nothing in this file has run on hardware.
 */

#ifndef OAPOGEE_OA_SINK_H
#define OAPOGEE_OA_SINK_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oa_sink oa_sink_t;

typedef struct {
    /* Append len bytes. A short write is a failure, not a partial success:
     * every record this firmware writes is fixed width, and half a record in a
     * flat array of fixed width records shifts every record after it.
     *
     * Returns OA_OK, OA_ERR_SINK_FULL when the store is out of space, or
     * OA_ERR_SINK_IO for anything else. OA_ERR_SINK_FULL is the one the flight
     * code handles: it raises OA_FLAG_LOG_FULL, stops logging, and keeps
     * flying. A flight does not end because the flash filled up. */
    oa_result_t (*write)(void *ctx, const uint8_t *data, size_t len);

    /* Bytes that can still be written, or SIZE_MAX when the sink does not know
     * or does not have a limit. Used to decide whether a whole record fits
     * before starting one, so that the file ends on a record boundary when the
     * store fills up rather than mid-record.
     *
     * Truncation mid-record is legal in the format and readers handle it, but
     * it is meant to describe a payload that lost power on impact. Leaving it to
     * mean that alone is worth one call per record. */
    size_t (*space_remaining)(void *ctx);

    /* Commit what has been written so far as durably as the store can. Called at
     * flight state transitions and after the manifest, not per record, because
     * syncing every record would cost more write bandwidth than the records do.
     *
     * A sink that has nothing to flush returns OA_OK. */
    oa_result_t (*sync)(void *ctx);
} oa_sink_vtable_t;

struct oa_sink {
    const oa_sink_vtable_t *vt;
    void *ctx;
};

/* Forwarding wrappers. They exist so that a NULL sink, a NULL vtable, or a
 * missing method is caught in one place instead of at every call site, and so
 * that call sites read as oa_sink_write(s, ...) rather than s->vt->write(s->ctx,
 * ...), which is easy to get wrong when a sink is optional. */
oa_result_t oa_sink_write(oa_sink_t *sink, const uint8_t *data, size_t len);
size_t      oa_sink_space_remaining(oa_sink_t *sink);
oa_result_t oa_sink_sync(oa_sink_t *sink);

/* Write a NUL-terminated string without its terminator. The manifest writer is
 * the only user: meta.json is text, and building it through the same sink as the
 * binary records keeps core with exactly one output interface. */
oa_result_t oa_sink_write_str(oa_sink_t *sink, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_SINK_H */

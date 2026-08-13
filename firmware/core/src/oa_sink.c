/*
 * oApogee core: byte sink forwarding wrappers.
 *
 * Four functions, and the whole reason they exist is that the NULL checks happen
 * once here instead of at every call site. The log writers and the manifest
 * writer call through these hundreds of times per flight, and a sink is
 * legitimately optional in places (a Solo build writes no gnss.bin), so a call
 * site that forgot to check would be a null dereference on a payload that is
 * already flying.
 *
 * There is no tuning constant in this file, and nothing here allocates or holds
 * state.
 *
 * Nothing here has run on hardware. No byte has been written to a flash part.
 */

#include "oapogee/oa_sink.h"

oa_result_t oa_sink_write(oa_sink_t *sink, const uint8_t *data, size_t len)
{
    if (sink == NULL || sink->vt == NULL || sink->vt->write == NULL) {
        return OA_ERR_NULL;
    }

    /* A zero length write has no bytes to lose, and passing it through would
     * make every sink implementation responsible for handling a case that means
     * nothing. Checked before the data pointer, so that (NULL, 0) is a no-op
     * rather than an error: it is what an empty string reaches this function as. */
    if (len == 0u) {
        return OA_OK;
    }

    if (data == NULL) {
        return OA_ERR_NULL;
    }

    return sink->vt->write(sink->ctx, data, len);
}

size_t oa_sink_space_remaining(oa_sink_t *sink)
{
    /* SIZE_MAX means "does not know or has no limit", per oa_sink.h. A sink whose
     * vtable omits the method is exactly that case, and it must not read as zero:
     * the log writers refuse to start a record that does not fit, so a zero here
     * would silently stop a payload logging anything at all.
     *
     * A NULL sink is the opposite case and is not "does not know". It cannot
     * accept a byte, so the honest answer is none. No caller reaches this with a
     * NULL sink today because every one of them rejects NULL first, and this
     * branch exists so that a future one that forgets fails toward refusing to
     * write rather than toward writing into nothing. */
    if (sink == NULL || sink->vt == NULL) {
        return 0u;
    }

    if (sink->vt->space_remaining == NULL) {
        return SIZE_MAX;
    }

    return sink->vt->space_remaining(sink->ctx);
}

oa_result_t oa_sink_sync(oa_sink_t *sink)
{
    if (sink == NULL || sink->vt == NULL) {
        return OA_ERR_NULL;
    }

    /* A sink with nothing to flush returns OA_OK, and a sink that did not supply
     * the method is one of those. This is not the same as the write case: a
     * missing write means bytes would be dropped, a missing sync means there was
     * nothing buffered to commit. */
    if (sink->vt->sync == NULL) {
        return OA_OK;
    }

    return sink->vt->sync(sink->ctx);
}

oa_result_t oa_sink_write_str(oa_sink_t *sink, const char *str)
{
    size_t len = 0u;

    if (str == NULL) {
        return OA_ERR_NULL;
    }

    /* Counted here rather than with strlen. core links against no libc beyond
     * the four memory functions on core/allowed-undefined.txt, and adding strlen
     * to that list to save three lines would widen the artifact that fences core
     * off from the platform. The strings this writes are manifest fragments of a
     * few dozen bytes. */
    while (str[len] != '\0') {
        len++;
    }

    return oa_sink_write(sink, (const uint8_t *)str, len);
}

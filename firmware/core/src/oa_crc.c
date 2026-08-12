/*
 * oApogee core: CRC-16/CCITT-FALSE.
 *
 * Polynomial 0x1021, initial value 0xFFFF, no input reflection, no output
 * reflection, no final XOR. Those parameters are the wire format, from
 * docs/spec/telemetry-packet.md, not a tuning choice, so they are constants in
 * oa_crc.h and are used from there rather than restated here.
 *
 * This is deliberately the same shape as the reference implementation in the
 * spec, so that the two can be read side by side and checked against each other
 * by eye. That is worth more right now than saving cycles: no packet has been
 * transmitted, no radio timing has been measured, and a payload that computes a
 * CRC nobody can verify against the published receiver is worse than a slow one.
 *
 * Nothing here has run on hardware.
 */

#include "oapogee/oa_crc.h"

uint16_t oa_crc16_update(uint16_t crc, const uint8_t *data, size_t len)
{
    size_t i;

    /* A NULL buffer with a non-zero length is a programming error, and this
     * function has no way to report one: every 16 bit value it could return is
     * a legal CRC. assert() is not an option either, because core may reference
     * nothing outside string.h and the assert machinery is libc, which
     * core/allowed-undefined.txt exists to keep out. So a NULL buffer
     * contributes nothing and the caller gets back the seed it passed in. The
     * packet builders never reach here that way: they check their own pointers
     * and then pass the buffer they have just written. */
    if (data == NULL) {
        return crc;
    }

    for (i = 0; i < len; i++) {
        /* The message byte enters at the top of the register rather than the
         * bottom. That is what makes this CCITT-FALSE rather than one of the
         * reflected CRC-16s that share the same polynomial and would produce a
         * different, equally plausible looking checksum. */
        crc = (uint16_t)(crc ^ (uint16_t)((uint16_t)data[i] << 8));

        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ OA_CRC16_POLY);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    /* No output reflection and no final XOR: the register is the result. */
    return crc;
}

uint16_t oa_crc16(const uint8_t *data, size_t len)
{
    /* The whole-buffer form is the incremental form seeded with the initial
     * value, and is written that way rather than duplicated, because two copies
     * of this loop would eventually differ in one of them and the difference
     * would only show on packets that happened to be computed in pieces. */
    return oa_crc16_update((uint16_t)OA_CRC16_INIT, data, len);
}

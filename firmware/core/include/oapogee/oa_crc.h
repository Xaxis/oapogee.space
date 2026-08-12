/*
 * CRC-16/CCITT-FALSE, as specified for the oApogee downlink.
 *
 * Parameters, from docs/spec/telemetry-packet.md: polynomial 0x1021, initial
 * value 0xFFFF, no input reflection, no output reflection, no final XOR. These
 * are the format, not a tuning choice, so they are constants in code.
 *
 * Why a CRC when LoRa already has one: the radio's CRC covers the air interface
 * and nothing else. It does not cover the serial link between the receiver
 * module and its host, a receiver library handing back a truncated buffer, or a
 * payload firmware bug that assembles a malformed packet and transmits it
 * correctly. A corrupted altitude that decodes cleanly is indistinguishable from
 * a real one.
 *
 * Nothing here has run on hardware.
 */

#ifndef OAPOGEE_OA_CRC_H
#define OAPOGEE_OA_CRC_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OA_CRC16_POLY (0x1021u)
#define OA_CRC16_INIT (0xFFFFu)
#define OA_CRC16_BYTES (2u)

/* Compute the CRC over a whole buffer. Returns the CRC; there is no failure
 * mode, and a NULL buffer with a non-zero length is a programming error the
 * implementation may assert on rather than a runtime condition to report.
 *
 * The value returned is the host-order integer. Storing it little-endian into
 * the last two bytes of the packet is the packet builder's job, not this
 * function's, so that this stays a pure arithmetic routine with an obvious
 * test vector. */
uint16_t oa_crc16(const uint8_t *data, size_t len);

/* Incremental form, for a caller that computes a CRC across pieces it never
 * holds contiguously. Seed with OA_CRC16_INIT. oa_crc16(d, n) is required to
 * equal oa_crc16_update(OA_CRC16_INIT, d, n), and the conformance test checks
 * that against a split at every offset, because an incremental CRC that is
 * subtly not the same function as the whole-buffer one is a bug that only
 * appears on the packets that happen to straddle a boundary. */
uint16_t oa_crc16_update(uint16_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_CRC_H */

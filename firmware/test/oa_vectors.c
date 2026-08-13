/*
 * Emit canonical packet vectors as text, one per line.
 *
 * This exists so that the C encoder and the TypeScript encoder can be compared
 * byte for byte. They were written from the same specification by different
 * hands, and neither was written from the other, so agreement between them is
 * real evidence that the document is unambiguous. Disagreement is a defect in
 * the specification even when both implementations look right, because two
 * careful readers reached different conclusions from the same words.
 *
 * tools/check-crossimpl.mjs runs this, runs the TypeScript encoder over the
 * same inputs, and fails the build on any difference.
 *
 * Output format, one vector per line:
 *
 *     <name> <hex>
 *
 * The vectors are chosen to cover what the two implementations could plausibly
 * disagree about: every packet type, both clamp endpoints of both encoded
 * scalars, the no-fix sentinels, the PAD_IDLE timestamp rule, the sequence
 * wrap, and a negative altitude, which is legitimate and which a careless
 * implementation clamps at zero.
 *
 * NOTHING HERE HAS RUN ON HARDWARE. These are bytes a laptop produced.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "oapogee/oa_flags.h"
#include "oapogee/oa_packet.h"
#include "oapogee/oa_state.h"

static void emit(const char *name, const uint8_t *bytes, size_t len)
{
    printf("%s ", name);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", bytes[i]);
    }
    printf("\n");
}

#define BUILD(fn, name, hdr, body)                                  \
    do {                                                            \
        uint8_t buf[OA_PACKET_MAX_BYTES];                           \
        size_t  len = 0;                                            \
        if (fn(&(hdr), &(body), buf, sizeof buf, &len) != OA_OK) {  \
            fprintf(stderr, "failed to build %s\n", (name));        \
            return 1;                                               \
        }                                                           \
        emit((name), buf, len);                                     \
    } while (0)

int main(void)
{
    /* --- STATUS, including both pressure clamp endpoints ----------------- */
    {
        oa_packet_header_t hdr = {
            .flags = 0, .seq = 12, .state = OA_STATE_PAD_IDLE, .t_ms = 987654};
        /* t_ms above is deliberately non-zero: the builder must transmit 0
         * because the state is PAD_IDLE, and an implementation that forwards it
         * would differ here rather than somewhere subtler. */
        oa_status_body_t body = {.pad_pressure_pa_off = 0, .batt = 0};
        bool             fault = false;

        body.pad_pressure_pa_off = oa_packet_encode_pad_pressure(101325, &fault);
        body.batt                = 158; /* 2.5 + 158/100 = 4.08 V */
        BUILD(oa_packet_build_status, "status_pad", hdr, body);

        body.pad_pressure_pa_off = oa_packet_encode_pad_pressure(50000, &fault);
        body.batt                = 0;
        BUILD(oa_packet_build_status, "status_pressure_low", hdr, body);

        body.pad_pressure_pa_off = oa_packet_encode_pad_pressure(115535, &fault);
        body.batt                = 255;
        BUILD(oa_packet_build_status, "status_pressure_high", hdr, body);

        /* Below and above the band. Both clamp, and both raise BARO_FAULT, so
         * the flags byte differs from the in-range vectors above. */
        fault                    = false;
        body.pad_pressure_pa_off = oa_packet_encode_pad_pressure(49999, &fault);
        hdr.flags                = fault ? OA_FLAG_BARO_FAULT : 0u;
        body.batt                = 100;
        BUILD(oa_packet_build_status, "status_pressure_under", hdr, body);

        fault                    = false;
        body.pad_pressure_pa_off = oa_packet_encode_pad_pressure(115536, &fault);
        hdr.flags                = fault ? OA_FLAG_BARO_FAULT : 0u;
        BUILD(oa_packet_build_status, "status_pressure_over", hdr, body);
    }

    /* --- FLIGHT, including a negative altitude --------------------------- */
    {
        oa_packet_header_t hdr = {.flags = OA_FLAG_HIGH_G,
                                  .seq   = 88,
                                  .state = OA_STATE_BOOST,
                                  .t_ms  = 1430};
        oa_flight_body_t   body = {
              .alt_cm = 8250, .vel_dm_s = 1420, .accel_cg = 2340, .batt = 144};
        BUILD(oa_packet_build_flight, "flight_boost", hdr, body);

        /* Negative altitude is legitimate: the pad reference can sit above where
         * the rocket lands, and the spec forbids clamping it at zero. */
        hdr.state      = OA_STATE_LANDED;
        hdr.t_ms       = 214000;
        body.alt_cm    = -12345;
        body.vel_dm_s  = -400;
        body.accel_cg  = -100;
        BUILD(oa_packet_build_flight, "flight_negative_altitude", hdr, body);

        /* The sequence number wraps at 255 rather than widening. */
        hdr.seq = 255;
        BUILD(oa_packet_build_flight, "flight_seq_max", hdr, body);
        hdr.seq = 0;
        BUILD(oa_packet_build_flight, "flight_seq_wrapped", hdr, body);
    }

    /* --- APOGEE ----------------------------------------------------------- */
    {
        oa_packet_header_t hdr = {
            .flags = 0, .seq = 91, .state = OA_STATE_APOGEE, .t_ms = 9180};
        oa_apogee_body_t body = {.apogee_cm = 40500, .t_apogee_ms = 9040};
        BUILD(oa_packet_build_apogee, "apogee", hdr, body);
    }

    /* --- BEACON, with and without a fix ----------------------------------- */
    {
        oa_packet_header_t hdr = {
            .flags = 0, .seq = 3, .state = OA_STATE_LANDED, .t_ms = 240000};
        oa_beacon_body_t body = {
            .lat_e7 = INT32_MIN, .lon_e7 = INT32_MIN, .apogee_cm = 40500};
        BUILD(oa_packet_build_beacon, "beacon_no_fix", hdr, body);

        hdr.flags   = OA_FLAG_GNSS_FIX;
        body.lat_e7 = 512345678;
        body.lon_e7 = -1234567;
        BUILD(oa_packet_build_beacon, "beacon_fix", hdr, body);
    }

    /* --- POSITION, with and without a fix --------------------------------- */
    {
        oa_packet_header_t hdr = {
            .flags = OA_FLAG_GNSS_FIX, .seq = 40, .state = OA_STATE_COAST, .t_ms = 5000};
        oa_position_body_t body = {
            .lat_e7 = 512345678, .lon_e7 = -1234567, .sats = 9};
        BUILD(oa_packet_build_position, "position_fix", hdr, body);

        hdr.flags   = 0;
        body.lat_e7 = INT32_MIN;
        body.lon_e7 = INT32_MIN;
        body.sats   = 0;
        BUILD(oa_packet_build_position, "position_no_fix", hdr, body);
    }

    return 0;
}

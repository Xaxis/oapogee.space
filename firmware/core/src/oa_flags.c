/*
 * oApogee core: status flag helpers.
 *
 * The three tables below are expanded from OA_FLAG_LIST in oapogee/oa_flags.h,
 * which is itself a transcription of the flags table in
 * docs/spec/telemetry-packet.md. Nothing here restates a bit position or a mask,
 * because the same byte appears at offset 1 of every packet and at offset 35 of
 * every flight.bin record, and a second definition of it would drift.
 *
 * There is no tuning constant in this file. The masks are the wire format.
 *
 * Nothing here has run on hardware.
 */

#include "oapogee/oa_flags.h"

/* The table carries both the bit index and the mask, which is redundant on
 * purpose: the spec publishes both columns, so both are transcribed and the
 * arithmetic between them is checked here rather than assumed. A mistyped mask
 * fails the build instead of producing a packet whose flags mean something else. */
#define OA_FLAG_CHECK(SYM, bit, mask, desc)                    \
    _Static_assert((mask) == (1u << (bit)),                    \
                   "flag " #SYM ": mask does not equal 1u << bit");
OA_FLAG_LIST(OA_FLAG_CHECK)
#undef OA_FLAG_CHECK

/* Bit 7 is the only reserved bit today, so the transmit mask has to be every
 * defined bit except that one. Checked here so that assigning bit 7 in the
 * header without updating the mask cannot go out on the air quietly. */
_Static_assert(OA_FLAG_TRANSMIT_MASK == (0xFFu & ~(unsigned)OA_FLAG_RESERVED7),
               "OA_FLAG_TRANSMIT_MASK must clear exactly the reserved bits");

typedef struct {
    uint8_t     mask;
    const char *name;
    const char *description;
} oa_flag_entry_t;

static const oa_flag_entry_t k_flags[] = {
#define OA_FLAG_ROW(SYM, bit, mask, desc) { (uint8_t)(mask), #SYM, desc },
    OA_FLAG_LIST(OA_FLAG_ROW)
#undef OA_FLAG_ROW
};

_Static_assert(sizeof k_flags / sizeof k_flags[0] == OA_FLAG_COUNT,
               "the flag table and OA_FLAG_COUNT disagree");

uint8_t oa_flags_sanitise(uint8_t flags)
{
    return (uint8_t)(flags & OA_FLAG_TRANSMIT_MASK);
}

/* Both lookups reject anything that is not exactly one defined bit. A caller
 * asking for the name of 0x03 has a bug, and returning "GNSS_FIX" for it would
 * hide the bug behind a plausible string. NULL is the answer the header promises
 * and it is the one a console can print as a number instead. */
static const oa_flag_entry_t *oa_flag_lookup(oa_flag_t flag)
{
    const unsigned value = (unsigned)flag;
    size_t         i;

    for (i = 0; i < sizeof k_flags / sizeof k_flags[0]; i++) {
        if (k_flags[i].mask == value) {
            return &k_flags[i];
        }
    }

    return NULL;
}

const char *oa_flag_name(oa_flag_t flag)
{
    const oa_flag_entry_t *entry = oa_flag_lookup(flag);

    return (entry != NULL) ? entry->name : NULL;
}

const char *oa_flag_description(oa_flag_t flag)
{
    const oa_flag_entry_t *entry = oa_flag_lookup(flag);

    return (entry != NULL) ? entry->description : NULL;
}

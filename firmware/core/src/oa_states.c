/*
 * oApogee core: flight state name tables.
 *
 * Expanded from OA_STATE_LIST in gen/oa_states.h, which is a rendering of
 * data/flight-phases.yaml. Nothing here restates a state, an order value or a
 * name: the wire value in the packet header and the state field in a flight.bin
 * record are the same enumeration as the homepage animation, and a second copy
 * of it would be the drift the generated header exists to prevent.
 *
 * There is no threshold in this file. The transition criteria live in the YAML
 * as prose and their numbers live in oa_config_t as unset.
 *
 * Nothing here has run on hardware.
 */

#include "oa_states.h"

typedef struct {
    uint8_t     order;
    const char *id;
    const char *name;
} oa_state_entry_t;

static const oa_state_entry_t k_states[] = {
#define OA_STATE_ROW(SYM, order, id, name, NEXT) { (uint8_t)(order), id, name },
    OA_STATE_LIST(OA_STATE_ROW)
#undef OA_STATE_ROW
};

_Static_assert(sizeof k_states / sizeof k_states[0] == OA_STATE_COUNT,
               "the state table and OA_STATE_COUNT disagree");

/* Searched rather than indexed. The order values happen to be dense today, but
 * they are wire values that come from a YAML file this code does not control,
 * and an implementation that indexed by order would read past the end of the
 * table the first time somebody reserved a gap in the enumeration. Seven entries
 * makes the search free. */
static const oa_state_entry_t *oa_state_lookup(uint8_t value)
{
    size_t i;

    for (i = 0; i < sizeof k_states / sizeof k_states[0]; i++) {
        if (k_states[i].order == value) {
            return &k_states[i];
        }
    }

    return NULL;
}

const char *oa_state_id(oa_state_t state)
{
    const oa_state_entry_t *entry;

    /* An oa_state_t holding a reserved value is legal: the packet spec requires
     * a receiver to pass 7 to 255 through rather than treating them as an error.
     * The cast is safe because no defined state exceeds 255 and the caller has
     * nothing else it could be holding. */
    if ((unsigned)state > 0xFFu) {
        return NULL;
    }

    entry = oa_state_lookup((uint8_t)state);

    return (entry != NULL) ? entry->id : NULL;
}

const char *oa_state_name(oa_state_t state)
{
    const oa_state_entry_t *entry;

    if ((unsigned)state > 0xFFu) {
        return NULL;
    }

    entry = oa_state_lookup((uint8_t)state);

    return (entry != NULL) ? entry->name : NULL;
}

bool oa_state_is_defined(uint8_t value)
{
    return oa_state_lookup(value) != NULL;
}

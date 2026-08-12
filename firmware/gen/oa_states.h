/*
 * GENERATED FILE. DO NOT HAND-EDIT.
 *
 * Source:    data/flight-phases.yaml
 * Generator: tools/gen-states.mjs
 * Target:    firmware/gen/oa_states.h
 *
 * The flight state enumeration has four consumers: this firmware, the log
 * format, the packet format, and the homepage animation. data/flight-phases.yaml
 * is the one definition, and this header is a rendering of it. Edit the YAML and
 * regenerate; an edit made here is lost the next time the generator runs, and
 * worse, it is lost silently while the two definitions disagree.
 *
 * HONESTY NOTE, to be deleted by the first real generator run:
 * tools/gen-states.mjs does not exist yet. This file was written by hand, to the
 * exact shape the generator will produce, so that the rest of the firmware has
 * something to compile against. Until the generator exists and CI checks that
 * the committed file matches its output, this file is a hand-written copy of the
 * YAML with all the drift risk that implies. It has not run on hardware, because
 * nothing has.
 */

#ifndef OAPOGEE_GEN_OA_STATES_H
#define OAPOGEE_GEN_OA_STATES_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Schema version of data/flight-phases.yaml this file was generated from. */
#define OA_STATES_SCHEMA_VERSION (1)

/* ---------------------------------------------------------------------------
 * The table.
 *
 * X(SYM, order, id_string, name_string, NEXT_SYM)
 *
 * `order` is the wire value: it is what goes in the packet header's state byte
 * and in the flight record's state field, and receivers and readers depend on
 * it. Values 7 to 255 are reserved; a receiver that meets one displays the
 * number rather than guessing.
 *
 * NEXT_SYM is the single successor from `transitions_to`. LANDED is terminal
 * until a power cycle, and names itself, because a table entry that had to be
 * special-cased by every consumer would be special-cased wrongly by one of them.
 * A consumer that cares about terminality tests SYM == NEXT_SYM.
 *
 * The transition criteria are not in this table. They are prose in the YAML
 * because the shape of each rule is a design decision, and every threshold they
 * need is unmeasured and lives in oa_config_t as unset.
 * ------------------------------------------------------------------------ */

#define OA_STATE_LIST(X)                                              \
    X(PAD_IDLE, 0, "PAD_IDLE", "Pad idle", ARMED)                     \
    X(ARMED,    1, "ARMED",    "Armed",    BOOST)                     \
    X(BOOST,    2, "BOOST",    "Boost",    COAST)                     \
    X(COAST,    3, "COAST",    "Coast",    APOGEE)                    \
    X(APOGEE,   4, "APOGEE",   "Apogee",   DESCENT)                   \
    X(DESCENT,  5, "DESCENT",  "Descent",  LANDED)                    \
    X(LANDED,   6, "LANDED",   "Landed",   LANDED)

#define OA_STATE_ENUMERATOR(SYM, order, id, name, NEXT) OA_STATE_##SYM = (order),

typedef enum {
    OA_STATE_LIST(OA_STATE_ENUMERATOR)
    OA_STATE_COUNT = 7
} oa_state_t;

#undef OA_STATE_ENUMERATOR

/* The last defined state value. Values above this are reserved and a decoder
 * must pass them through rather than treating them as an error. */
#define OA_STATE_MAX_DEFINED (OA_STATE_LANDED)

/* The id string exactly as it appears in data/flight-phases.yaml, for the serial
 * console and for test failure messages. Returns NULL for an undefined value,
 * rather than a placeholder, so a caller cannot print something that reads like
 * a state name for a number that is not a state. */
const char *oa_state_id(oa_state_t state);

/* The human name from the YAML, for anything an operator reads. NULL under the
 * same rule. */
const char *oa_state_name(oa_state_t state);

/* True when `value` is one of the defined states. A receiver-side decoder wants
 * this; the state machine itself never produces an undefined value. */
bool oa_state_is_defined(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_GEN_OA_STATES_H */

/*
 * oApogee core: configuration store and the flightworthiness check.
 *
 * There is not one tuning constant in this file, and that is the point of the
 * module. Every threshold, interval, rate and window the firmware needs is
 * declared in OA_CONFIG_FIELDS in oapogee/oa_config.h, every one of them starts
 * OA_UNSET, and oa_config_is_flightworthy refuses while a required one is
 * missing. A payload that will not arm is a payload telling the truth about what
 * it knows.
 *
 * The table below is expanded from that same list, so the field names, units,
 * requirements, closing conditions and struct offsets cannot fall out of step
 * with the members they describe. offsetof is what makes a generic accessor
 * reach a named member without a hand-written parallel list, which is exactly
 * the drift the X-macro exists to prevent.
 *
 * Nothing here has run on hardware, and no threshold in this firmware has been
 * measured.
 */

#include <string.h>

#include "oapogee/oa_config.h"

/* ---------------------------------------------------------------------------
 * The table.
 * ------------------------------------------------------------------------ */

typedef struct {
    const char     *name;
    const char     *unit;
    const char     *why;
    oa_config_req_t requirement;
    size_t          offset; /* of the oa_tunable_t member inside oa_config_t */
} oa_config_entry_t;

/* Indexed by the enumerator rather than laid out in order. The enumerator is used
 * as an array index here and as a bit index into the set bitmap, so the two have
 * to correspond exactly; a designated initialiser makes that correspondence
 * explicit instead of leaving it to depend on both lists being generated in the
 * same order. A row that landed at the wrong index would make every accessor read
 * a different field's value and nothing else would notice. */
static const oa_config_entry_t k_fields[OA_CFG_FIELD_COUNT] = {
#define OA_CONFIG_ROW(name, NAME, req, unit, why) \
    [OA_CFG_##NAME] = { #name, unit, why, req, offsetof(oa_config_t, name) },
    OA_CONFIG_FIELDS(OA_CONFIG_ROW)
#undef OA_CONFIG_ROW
};

_Static_assert(sizeof k_fields / sizeof k_fields[0] == OA_CFG_FIELD_COUNT,
               "the field table and OA_CFG_FIELD_COUNT disagree");

/* ---------------------------------------------------------------------------
 * Helpers.
 * ------------------------------------------------------------------------ */

static bool field_valid(oa_config_field_t field)
{
    /* Cast to unsigned so a negative value taken from a cast of some other
     * integer lands above the count rather than below zero. */
    return (unsigned)field < (unsigned)OA_CFG_FIELD_COUNT;
}

static oa_tunable_t *member_of(oa_config_t *cfg, oa_config_field_t field)
{
    return (oa_tunable_t *)(void *)((unsigned char *)cfg + k_fields[field].offset);
}

static const oa_tunable_t *member_of_const(const oa_config_t *cfg, oa_config_field_t field)
{
    return (const oa_tunable_t *)(const void *)((const unsigned char *)cfg
                                                + k_fields[field].offset);
}

static bool set_bit_get(const uint8_t *set, oa_config_field_t field)
{
    const unsigned index = (unsigned)field;

    return (set[index >> 3] & (uint8_t)(1u << (index & 7u))) != 0u;
}

static void set_bit_raise(uint8_t *set, oa_config_field_t field)
{
    const unsigned index = (unsigned)field;

    set[index >> 3] = (uint8_t)(set[index >> 3] | (uint8_t)(1u << (index & 7u)));
}

static void set_bit_clear(uint8_t *set, oa_config_field_t field)
{
    const unsigned index = (unsigned)field;

    set[index >> 3] = (uint8_t)(set[index >> 3] & (uint8_t)~(uint8_t)(1u << (index & 7u)));
}

/* Compared byte by byte rather than with strcmp. core links against no libc
 * beyond the four memory functions on core/allowed-undefined.txt, and widening
 * that artifact to save four lines would weaken the one check that fences core
 * off from the platform. Field names are short and this runs once per line of a
 * configuration file. */
static bool name_equal(const char *a, const char *b)
{
    size_t i = 0u;

    while (a[i] != '\0' && a[i] == b[i]) {
        i++;
    }

    return a[i] == b[i];
}

/* ---------------------------------------------------------------------------
 * Lifecycle and access.
 * ------------------------------------------------------------------------ */

void oa_config_init(oa_config_t *cfg)
{
    size_t i;

    if (cfg == NULL) {
        return;
    }

    /* Zero first, which establishes the set bitmap, simulated false and an empty
     * callsign, then write the sentinel into every member. Both halves matter:
     * `set` is the authority on whether a field has a value, and the sentinel is
     * the second line of defence so that a value which leaks past the set-bit
     * check is obviously wrong on a plot rather than quietly plausible. */
    memset(cfg, 0, sizeof *cfg);

    for (i = 0u; i < OA_CFG_FIELD_COUNT; i++) {
        *member_of(cfg, (oa_config_field_t)i) = OA_UNSET;
    }
}

oa_result_t oa_config_set(oa_config_t *cfg, oa_config_field_t field, oa_tunable_t value)
{
    if (cfg == NULL) {
        return OA_ERR_NULL;
    }
    if (!field_valid(field)) {
        return OA_ERR_RANGE;
    }

    /* Setting a field to the sentinel would leave the set bit and the member
     * disagreeing, and the disagreement would be invisible: is_set would say yes
     * and every reader would see OA_UNSET. Clearing is a different operation and
     * has its own function. */
    if (!OA_IS_SET(value)) {
        return OA_ERR_RANGE;
    }

    *member_of(cfg, field) = value;
    set_bit_raise(cfg->set, field);

    return OA_OK;
}

oa_result_t oa_config_get(const oa_config_t *cfg, oa_config_field_t field, oa_tunable_t *out)
{
    if (cfg == NULL || out == NULL) {
        return OA_ERR_NULL;
    }
    if (!field_valid(field)) {
        return OA_ERR_RANGE;
    }

    /* An unset field reads back as OA_UNSET rather than as an error, because the
     * caller asked what the value is and "not measured" is the answer. Callers
     * that need to act on the difference ask oa_config_is_set. */
    *out = *member_of_const(cfg, field);

    return OA_OK;
}

oa_result_t oa_config_clear(oa_config_t *cfg, oa_config_field_t field)
{
    if (cfg == NULL) {
        return OA_ERR_NULL;
    }
    if (!field_valid(field)) {
        return OA_ERR_RANGE;
    }

    *member_of(cfg, field) = OA_UNSET;
    set_bit_clear(cfg->set, field);

    return OA_OK;
}

bool oa_config_is_set(const oa_config_t *cfg, oa_config_field_t field)
{
    if (cfg == NULL || !field_valid(field)) {
        return false;
    }

    return set_bit_get(cfg->set, field);
}

/* ---------------------------------------------------------------------------
 * Table metadata.
 * ------------------------------------------------------------------------ */

const char *oa_config_field_name(oa_config_field_t field)
{
    return field_valid(field) ? k_fields[field].name : NULL;
}

const char *oa_config_field_unit(oa_config_field_t field)
{
    return field_valid(field) ? k_fields[field].unit : NULL;
}

const char *oa_config_field_why(oa_config_field_t field)
{
    return field_valid(field) ? k_fields[field].why : NULL;
}

oa_config_req_t oa_config_field_requirement(oa_config_field_t field)
{
    /* OA_REQ_OPTIONAL for a value that is not a field. It is the only answer that
     * cannot cause a caller to demand a number for something that does not
     * exist, and every caller of this reaches it through a valid enumerator. */
    return field_valid(field) ? k_fields[field].requirement : OA_REQ_OPTIONAL;
}

oa_result_t oa_config_field_by_name(const char *name, oa_config_field_t *out)
{
    size_t i;

    if (name == NULL || out == NULL) {
        return OA_ERR_NULL;
    }

    for (i = 0u; i < OA_CFG_FIELD_COUNT; i++) {
        if (name_equal(k_fields[i].name, name)) {
            *out = (oa_config_field_t)i;
            return OA_OK;
        }
    }

    /* An unknown key is reported, never ignored. A typo in a threshold name that
     * was silently dropped would leave that threshold unset, and the operator
     * would find out at the rail. */
    return OA_ERR_RANGE;
}

/* ---------------------------------------------------------------------------
 * Validation.
 * ------------------------------------------------------------------------ */

bool oa_config_field_is_required(oa_config_field_t field, oa_features_t features)
{
    if (!field_valid(field)) {
        return false;
    }

    switch (k_fields[field].requirement) {
        case OA_REQ_ALWAYS:
            return true;

        /* A field required only when the part is populated. Refusing to arm a
         * Solo because no spreading factor was set would be a validator nobody
         * trusts, and a validator nobody trusts is a validator that gets
         * bypassed. */
        case OA_REQ_RADIO:
            return (features & (oa_features_t)OA_FEATURE_RADIO) != 0u;

        case OA_REQ_GNSS:
            return (features & (oa_features_t)OA_FEATURE_GNSS) != 0u;

        case OA_REQ_OPTIONAL:
        default:
            return false;
    }
}

bool oa_config_is_flightworthy(const oa_config_t *cfg,
                               oa_features_t      features,
                               oa_config_report_t *report)
{
    size_t i;
    bool   ok = true;

    if (report != NULL) {
        memset(report, 0, sizeof *report);
    }

    if (cfg == NULL) {
        return false;
    }

    /* Every field is walked even after the first miss, because the useful thing
     * to show an operator is every number that is missing with what would settle
     * each one beside it, not the first one the loop happened to reach. */
    for (i = 0u; i < OA_CFG_FIELD_COUNT; i++) {
        const oa_config_field_t field = (oa_config_field_t)i;

        if (!oa_config_field_is_required(field, features)) {
            continue;
        }
        if (oa_config_is_set(cfg, field)) {
            continue;
        }

        ok = false;

        if (report != NULL) {
            set_bit_raise(report->missing, field);
            report->missing_count++;
        }
    }

    return ok;
}

/*
 * oApogee barometric altitude, and the pad reference accumulator.
 *
 * See oa_baro.h for why the reference is taken on arming and what that costs.
 * This file is the arithmetic.
 *
 * WHAT IS A CONSTANT HERE AND WHAT IS NOT
 *
 * The four numbers the conversion needs (sea level standard temperature, the
 * tropospheric lapse rate, the specific gas constant for dry air, standard
 * gravity) are the International Standard Atmosphere. They are defined values in
 * a published standard, so they are compiled in and cited. Nothing anyone
 * measures on a launch site changes them.
 *
 * The settling window, the minimum sample count and the plausible pressure band
 * are the opposite. They come from barometer noise nobody has measured, so they
 * live in oa_config_t as unset, and this file refuses to act when they are
 * missing rather than substituting anything.
 *
 * NO FLOATING POINT. The conversion runs at the sample rate in an
 * interrupt-adjacent context, so it is integer throughout, and the whole of it
 * is exact integer arithmetic that produces the same bits on the host test and
 * on a target. There is no libm here: pow() is replaced by a digit-by-digit
 * log2 and exp2 over a table of exact mathematical constants, described at the
 * table.
 *
 * Nothing in this file has run on hardware, and no reference has ever been
 * taken. The conversion has been checked against a double-precision evaluation
 * of the same formula on the host, which is a check of the arithmetic and not
 * of any barometer.
 */

#include "oapogee/oa_baro.h"

/* For OA_STANDARD_GRAVITY_UM_S2. Standard gravity is a defined SI constant that
 * already has a home in oa_fusion.h, and one number defined in two places is one
 * number that can end up with two values. Nothing else is used from that
 * header. */
#include "oapogee/oa_fusion.h"

/* ---------------------------------------------------------------------------
 * The International Standard Atmosphere, troposphere layer.
 *
 * ISO 2533:1975, the same values ICAO Doc 7488 publishes. Each is held in an
 * integer unit fine enough to be exact, so the derivations below are exact
 * integer expressions a reader can check with a calculator rather than decimal
 * literals that have to be trusted.
 *
 * The layer this formula describes runs from sea level to 11000 m, where the
 * standard atmosphere becomes isothermal and the lapse rate stops applying. The
 * pressure spec's band bottoms out at 50000 Pa, which is roughly 5500 m of
 * pressure altitude, so every reference the format can carry is inside the
 * layer. Above 11000 m this conversion over-reads, and it is only reachable
 * here from a pressure no conforming packet can hold.
 * ------------------------------------------------------------------------ */

/* 288.15 K, in microkelvin. */
#define OA_ISA_SEA_LEVEL_TEMPERATURE_UK (288150000)

/* 0.0065 K/m, in microkelvin per metre. */
#define OA_ISA_LAPSE_RATE_UK_PER_M (6500)

/* 287.052874 J/(kg K), the specific gas constant for dry air, in microjoule per
 * kilogram per kelvin. */
#define OA_ISA_SPECIFIC_GAS_CONSTANT_UJ_PER_KG_K (287052874)

/* ---------------------------------------------------------------------------
 * The two derived constants.
 *
 * The hypsometric relation under the ISA, written against a local reference
 * pressure rather than sea level:
 *
 *     h = (T0 / L) * (1 - (p / p_ref)^(L * R / g))
 *
 * T0 / L is a length, 44330.77 m, and it is the altitude at which the standard
 * temperature profile would reach absolute zero. It is the whole scale of the
 * conversion, so it is held in centimetres, the unit the result is in.
 *
 * L * R / g is dimensionless, about 0.19026, and it is the exponent. It is held
 * in Q30 because it multiplies a log2 that is also in Q30.
 *
 * Using T0 for the reference level, rather than the actual air temperature at
 * the pad, is the standard altimeter approximation and it has a stated cost: the
 * altitude scales with the temperature of the air column, so a column at T
 * rather than 288.15 K makes every altitude wrong by a factor of (T - 288.15) /
 * 288.15, which is about 0.35% per kelvin. That is a property of the model, not
 * of this implementation, and it is the reason a barometric altimeter and a
 * surveyed height disagree on a hot day.
 *
 * TODO(verify): state the altitude difference this temperature assumption
 * produces on a launch day at, say, 30 degC, alongside a measured flight, so a
 * reader can see the size of it next to the size of everything else.
 *
 * Both expressions are integer constant expressions built from the constants
 * above, so they fold at compile time and no division of any width happens at
 * run time. The /1000 and *1000 in the exponent are there so the numerator
 * stays inside int64 while keeping the full precision of L * R, which is
 * exactly divisible by 1000.
 * ------------------------------------------------------------------------ */

#define OA_BARO_SCALE_CM                                                        \
    ((int32_t)(((int64_t)OA_ISA_SEA_LEVEL_TEMPERATURE_UK * 100                  \
                + (OA_ISA_LAPSE_RATE_UK_PER_M / 2))                             \
               / OA_ISA_LAPSE_RATE_UK_PER_M))

#define OA_BARO_EXPONENT_SHIFT (30)

#define OA_BARO_EXPONENT_Q30                                                    \
    ((int64_t)((((int64_t)OA_ISA_LAPSE_RATE_UK_PER_M                            \
                 * OA_ISA_SPECIFIC_GAS_CONSTANT_UJ_PER_KG_K / 1000)             \
                * ((int64_t)1 << OA_BARO_EXPONENT_SHIFT))                       \
               / ((int64_t)OA_STANDARD_GRAVITY_UM_S2 * 1000)))

/* 44330.77 m in centimetres, and 0.19026 in Q30. Stated as assertions rather
 * than as a comment a reader has to trust, so that a mistyped constant above
 * fails the build instead of shifting every altitude by a plausible amount. */
_Static_assert(OA_BARO_SCALE_CM == 4433077, "ISA scale height is not 44330.77 m");
_Static_assert(OA_BARO_EXPONENT_Q30 > 204293000 && OA_BARO_EXPONENT_Q30 < 204294000,
               "ISA exponent is not 0.19026");

/* ---------------------------------------------------------------------------
 * log2 and exp2 in fixed point, without libm and without floating point.
 *
 * Both are the classic digit-by-digit method over one table. Entry i is
 *
 *     round(log2(1 + 2^-i) * 2^30)
 *
 * which is a mathematical constant with a closed form, not a fitted
 * coefficient: any line can be checked on its own with a calculator, and the
 * table can be regenerated from that expression alone.
 *
 * log2 drives its argument toward 2 by multiplying by (1 + 2^-i) wherever that
 * fits, which costs a shift and an add, and accumulates the log of each factor
 * it used. exp2 runs the same table the other way, subtracting each log it can
 * afford from the exponent and multiplying the result by the matching factor.
 *
 * 28 terms is where the residual stops mattering: the leftover is under
 * 2^-28 of a bit of log2, which is far below the centimetre the result is
 * rounded to. The measured end-to-end error of the whole conversion is stated
 * at oa_baro_altitude_cm.
 * ------------------------------------------------------------------------ */

#define OA_LOG2_TERMS (28u)

static const uint32_t oa_log2_1p_q30[OA_LOG2_TERMS + 1u] = {
    0u,          /* unused, so that the index is the exponent i */
    628098702u,  /* log2(1 + 2^-1)  */
    345667660u,  /* log2(1 + 2^-2)  */
    182455581u,  /* log2(1 + 2^-3)  */
    93912511u,   /* log2(1 + 2^-4)  */
    47667823u,   /* log2(1 + 2^-5)  */
    24017256u,   /* log2(1 + 2^-6)  */
    12055174u,   /* log2(1 + 2^-7)  */
    6039314u,    /* log2(1 + 2^-8)  */
    3022600u,    /* log2(1 + 2^-9)  */
    1512037u,    /* log2(1 + 2^-10) */
    756203u,     /* log2(1 + 2^-11) */
    378148u,     /* log2(1 + 2^-12) */
    189085u,     /* log2(1 + 2^-13) */
    94546u,      /* log2(1 + 2^-14) */
    47274u,      /* log2(1 + 2^-15) */
    23637u,      /* log2(1 + 2^-16) */
    11819u,      /* log2(1 + 2^-17) */
    5909u,       /* log2(1 + 2^-18) */
    2955u,       /* log2(1 + 2^-19) */
    1477u,       /* log2(1 + 2^-20) */
    739u,        /* log2(1 + 2^-21) */
    369u,        /* log2(1 + 2^-22) */
    185u,        /* log2(1 + 2^-23) */
    92u,         /* log2(1 + 2^-24) */
    46u,         /* log2(1 + 2^-25) */
    23u,         /* log2(1 + 2^-26) */
    12u,         /* log2(1 + 2^-27) */
    6u           /* log2(1 + 2^-28) */
};

/* log2 of a strictly positive value, in Q30. The caller guarantees v > 0; there
 * is no logarithm of zero to return and no sentinel that would not also be a
 * plausible altitude. */
static int64_t oa_log2_q30(int32_t v)
{
    uint32_t mantissa = (uint32_t)v;
    int32_t  shift    = 0;
    uint32_t frac     = 0u;
    unsigned i;

    /* Normalise into [1, 2) held in Q30. v is a positive int32 so its top bit is
     * clear and only this direction can run. */
    while (mantissa < (1u << 30)) {
        mantissa <<= 1;
        shift++;
    }

    for (i = 1u; i <= OA_LOG2_TERMS; i++) {
        uint32_t scaled = mantissa + (mantissa >> i);
        if (scaled < (1u << 31)) {
            mantissa = scaled;
            frac += oa_log2_1p_q30[i];
        }
    }

    /* The loop drove the mantissa to 2, so the logs it used add up to whatever
     * was missing between the original mantissa and 2. */
    return (((int64_t)(31 - shift)) << 30) - (int64_t)frac;
}

/* 2^frac for a Q30 fraction in [0, 1), returned in Q30 and therefore in
 * [1, 2). */
static uint32_t oa_exp2_frac_q30(uint32_t frac)
{
    uint32_t result = 1u << 30;
    unsigned i;

    for (i = 1u; i <= OA_LOG2_TERMS; i++) {
        if (frac >= oa_log2_1p_q30[i]) {
            frac -= oa_log2_1p_q30[i];
            result += result >> i;
        }
    }
    return result;
}

/* Shift right with truncation toward zero on both signs.
 *
 * C11 leaves >> of a negative value implementation-defined. Every compiler this
 * can be built with makes it an arithmetic shift, but the claim this fixed point
 * arithmetic rests on is that the host test and the target produce identical
 * bits, and a claim that rests on implementation-defined behaviour is a weaker
 * claim than it looks. The sign is handled here instead, once. */
static int64_t oa_shr_toward_zero(int64_t value, unsigned shift)
{
    return (value < 0) ? -((-value) >> shift) : (value >> shift);
}

/* ---------------------------------------------------------------------------
 * Pressure to altitude.
 * ------------------------------------------------------------------------ */

/* MEASURED APPROXIMATION ERROR, which is the figure the TODO(verify) in
 * oa_baro.h asks for.
 *
 * test_oa_baro.c evaluates this function against a double-precision evaluation of
 * the same ISA formula over the whole 50000 to 115535 Pa band in both arguments
 * and asserts the largest disagreement stays under 1 cm. The largest seen when
 * this was written was 0.55 cm, and most of that is the final rounding of an
 * exact quantity to a whole centimetre, which no integer implementation can
 * avoid. That number describes this arithmetic only. It says nothing about how
 * well the ISA describes the air over a launch site, which is a much larger
 * error and is not this function's to make. */
int32_t oa_baro_altitude_cm(int32_t pressure_pa, int32_t reference_pa)
{
    int64_t  log_ratio_q30;
    int64_t  exponent_q30;
    int64_t  biased_q30;
    int64_t  power_q30;
    int64_t  one_minus_q30;
    int64_t  magnitude_cm;
    int32_t  whole;
    uint32_t frac_q30;
    uint32_t mantissa_q30;

    /* A programming error rather than a reading: the caller is supposed to have
     * checked oa_baro_ref_locked first. Zero is returned because there is no
     * altitude to report and no error channel on this signature. */
    if (reference_pa <= 0) {
        return 0;
    }

    /* Absolute pressure is strictly positive. A reading of zero or below is not
     * a reading, and the logarithm below has nothing to say about it. Pinning it
     * at 1 Pa makes the result saturate near the 44330 m where the standard
     * temperature profile reaches absolute zero, which is an altitude no reader
     * will mistake for a flight. */
    if (pressure_pa < 1) {
        pressure_pa = 1;
    }

    log_ratio_q30 = oa_log2_q30(pressure_pa) - oa_log2_q30(reference_pa);

    exponent_q30 = oa_shr_toward_zero(log_ratio_q30 * OA_BARO_EXPONENT_Q30,
                                      OA_BARO_EXPONENT_SHIFT);

    /* Both logs are of a positive int32, so the ratio spans less than 31 bits
     * and the exponent cannot leave (-6, 6). The clamp is what makes the shift
     * below provably in range rather than in range by argument. */
    if (exponent_q30 > ((int64_t)8 << 30)) {
        exponent_q30 = (int64_t)8 << 30;
    }
    if (exponent_q30 < -((int64_t)8 << 30)) {
        exponent_q30 = -((int64_t)8 << 30);
    }

    /* Split into a whole power of two and a fraction. Biasing first keeps the
     * split a shift and a mask of a positive value, with no signed division and
     * no rounding direction to argue about. */
    biased_q30   = exponent_q30 + ((int64_t)8 << 30);
    whole        = (int32_t)(biased_q30 >> 30) - 8;
    frac_q30     = (uint32_t)(biased_q30 & (((int64_t)1 << 30) - 1));
    mantissa_q30 = oa_exp2_frac_q30(frac_q30);

    power_q30 = (whole >= 0) ? ((int64_t)mantissa_q30 << whole)
                             : ((int64_t)mantissa_q30 >> (-whole));

    /* Negative altitudes are legitimate and are not clamped. A rocket can land
     * below the pad, and pressure can rise during a flight. */
    one_minus_q30 = ((int64_t)1 << 30) - power_q30;

    magnitude_cm = ((one_minus_q30 < 0 ? -one_minus_q30 : one_minus_q30)
                        * OA_BARO_SCALE_CM
                    + ((int64_t)1 << 29))
                   >> 30;

    return (int32_t)((one_minus_q30 < 0) ? -magnitude_cm : magnitude_cm);
}

/* ---------------------------------------------------------------------------
 * The pad reference accumulator.
 * ------------------------------------------------------------------------ */

oa_result_t oa_baro_ref_start(oa_baro_ref_t *ref, uint32_t now_ms)
{
    if (ref == NULL) {
        return OA_ERR_NULL;
    }

    /* Everything, including a previous lock. Restarting the window means the
     * operator disarmed and armed again, and the old reference was taken
     * somewhere else. */
    ref->sum_pa       = 0;
    ref->samples      = 0u;
    ref->started_ms   = now_ms;
    ref->started      = true;
    ref->reference_pa = 0;
    ref->locked       = false;
    ref->rejected     = 0u;

    return OA_OK;
}

oa_result_t oa_baro_ref_add(oa_baro_ref_t *ref,
                            const oa_config_t *cfg,
                            int32_t            pressure_pa,
                            uint32_t           now_ms)
{
    bool     band_unset = false;
    bool     plausible;
    uint32_t elapsed_ms;

    if (ref == NULL || cfg == NULL) {
        return OA_ERR_NULL;
    }
    if (!ref->started) {
        return OA_ERR_STATE;
    }

    /* A window of negative length and a minimum of zero samples are not values,
     * they are the absence of one arriving through a path that did not go
     * through oa_config_set. A reference averaged from no samples is exactly the
     * mistake this accumulator exists to prevent, so both are reported as
     * missing rather than acted on. */
    if (!OA_IS_SET(cfg->pad_reference_window_ms) || cfg->pad_reference_window_ms < 0) {
        return OA_ERR_UNSET;
    }
    if (!OA_IS_SET(cfg->pad_reference_min_samples) || cfg->pad_reference_min_samples <= 0) {
        return OA_ERR_UNSET;
    }

    /* Frozen means frozen. Every altitude in the flight is measured against this
     * number, so a zero that moves during the flight moves the whole altitude
     * column with it. */
    if (ref->locked) {
        return OA_OK;
    }

    plausible = oa_baro_is_plausible(cfg, pressure_pa, &band_unset);
    if (band_unset) {
        return OA_ERR_UNSET;
    }

    if (plausible) {
        ref->sum_pa += pressure_pa;
        ref->samples++;
    } else {
        /* Counted, not averaged. A dropout or a garbage reading must not move
         * the zero, and the operator is entitled to be told the reference was
         * taken from a struggling sensor. */
        ref->rejected++;
    }

    /* Unsigned subtraction, so a millisecond counter that wraps during a long
     * pad wait still yields the right interval. */
    elapsed_ms = now_ms - ref->started_ms;

    if (elapsed_ms >= (uint32_t)cfg->pad_reference_window_ms
        && ref->samples >= (uint32_t)cfg->pad_reference_min_samples) {
        /* The only division wider than 32 bits in core, and it runs once per
         * flight, when the window closes, rather than in the sample path. On a
         * target with no 64-bit divide instruction it becomes a compiler runtime
         * helper, which check-undefined will report as a new external symbol on
         * the first target build. That report is the intended signal and the
         * answer is to read it, not to widen the list quietly.
         *
         * Rounded to nearest rather than truncated: the reference is the zero
         * the entire altitude column is measured against, and a systematic half
         * pascal of bias in it is a systematic bias in every altitude. */
        int64_t rounded = ref->sum_pa + (int64_t)(ref->samples / 2u);

        ref->reference_pa = (int32_t)(rounded / (int64_t)ref->samples);
        ref->locked       = true;
    }

    /* If the window elapses and the sample count is never reached, this stays
     * unlocked forever and nothing leaves ARMED. That is the correct outcome: a
     * payload that cannot establish a zero has nothing to measure a flight
     * against, and refusing is more useful than a reference from four
     * readings. */
    return OA_OK;
}

bool oa_baro_ref_locked(const oa_baro_ref_t *ref)
{
    return (ref != NULL) && ref->locked;
}

uint32_t oa_baro_ref_samples(const oa_baro_ref_t *ref)
{
    return (ref == NULL) ? 0u : ref->samples;
}

uint32_t oa_baro_ref_rejected(const oa_baro_ref_t *ref)
{
    return (ref == NULL) ? 0u : ref->rejected;
}

oa_result_t oa_baro_ref_pressure_pa(const oa_baro_ref_t *ref, int32_t *out_pressure_pa)
{
    if (ref == NULL || out_pressure_pa == NULL) {
        return OA_ERR_NULL;
    }
    /* The running average is deliberately not reachable. A partial average
     * presented as a reference is the mistake, not the fix. */
    if (!ref->locked) {
        return OA_ERR_STATE;
    }
    *out_pressure_pa = ref->reference_pa;
    return OA_OK;
}

bool oa_baro_is_plausible(const oa_config_t *cfg, int32_t pressure_pa, bool *out_unset)
{
    if (out_unset != NULL) {
        *out_unset = false;
    }

    /* No configuration and no band are the same answer: this cannot say the
     * sensor is healthy, and saying so anyway would let a missing band read as a
     * working barometer. */
    if (cfg == NULL
        || !OA_IS_SET(cfg->baro_plausible_min_pa)
        || !OA_IS_SET(cfg->baro_plausible_max_pa)) {
        if (out_unset != NULL) {
            *out_unset = true;
        }
        return false;
    }

    return (pressure_pa >= cfg->baro_plausible_min_pa)
           && (pressure_pa <= cfg->baro_plausible_max_pa);
}

/*
 * oApogee configuration.
 *
 * THE POINT OF THIS FILE
 *
 * Every flight threshold, interval, rate and window this firmware needs is
 * unmeasured. There is no hardware, nothing has flown, and no sensor noise floor
 * has been characterised. A number chosen from intuition and compiled in as a
 * default would be plausible, specific and wrong, and it would fly.
 *
 * So there are no tuning constants in this firmware. Every one of them lives
 * here, every one of them starts unset, and oa_config_is_flightworthy() refuses
 * to arm while a required one is missing. A payload that will not arm is a
 * payload that is telling the truth about what it knows. That is the correct
 * behaviour for this project at this stage, and it is how the firmware avoids
 * shipping a guess.
 *
 * Constants that come from the specs are not tunables and are not here. The
 * 50000 Pa pressure offset, the 2.5 V battery offset, the CRC polynomial, the
 * field offsets and the record widths are the format, and they belong in code.
 * The test is whether the number would change if someone measured something. If
 * it would, it is a tunable.
 *
 * Configuration is a plain text file on the board's filesystem, edited over USB.
 * Parsing that file is the application's job, not core's: core defines the
 * fields, holds them, and decides whether the set of them is complete enough to
 * fly. That split is what lets the validator be tested on a laptop.
 *
 * Nothing in this file has run on hardware.
 */

#ifndef OAPOGEE_OA_CONFIG_H
#define OAPOGEE_OA_CONFIG_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Build features.
 *
 * Which parts are populated. Solo has neither radio nor GNSS, Link has the
 * radio, Track has both. A required-for-radio field is not required on a build
 * with no radio, and refusing to arm a Solo because no spreading factor was set
 * would be a validator nobody trusts, which is a validator that gets bypassed.
 * ------------------------------------------------------------------------ */

typedef enum {
    OA_FEATURE_NONE   = 0u,
    OA_FEATURE_RADIO  = 1u << 0,
    OA_FEATURE_GNSS   = 1u << 1,
    OA_FEATURE_HIGH_G = 1u << 2
} oa_feature_t;

typedef uint32_t oa_features_t;

/* ---------------------------------------------------------------------------
 * When a field is required.
 * ------------------------------------------------------------------------ */

typedef enum {
    /* Required on every build. Missing means the payload will not arm. */
    OA_REQ_ALWAYS = 0,

    /* Required only when the corresponding part is populated. */
    OA_REQ_RADIO = 1,
    OA_REQ_GNSS  = 2,

    /* Never blocks arming. Either the feature it controls is off when unset, or
     * the open question of whether it is needed at all has not been settled. An
     * optional field that is unset must have a behaviour, and that behaviour is
     * documented on the field. */
    OA_REQ_OPTIONAL = 3
} oa_config_req_t;

/* ---------------------------------------------------------------------------
 * THE FIELD TABLE
 *
 * X(name, NAME, requirement, unit, why)
 *
 * `name` is the struct member and the key in the configuration file. `NAME` is
 * the same word uppercased, because the preprocessor cannot change case and the
 * enumerator wants it. `unit` is the integer unit the value is stored in, which
 * is always one of the units the specs already use, so that no scaling happens
 * between the file, the validator and the code that reads it.
 *
 * `why` is the measurement that would close the field: exactly the closing
 * condition a TODO(verify) marker in the documentation would carry. It is a
 * string in the firmware, not just a comment, because the most useful thing a
 * payload that refuses to arm can do is say which number is missing and what
 * would settle it. That costs flash and it is worth it.
 *
 * Every field below is unset. Not defaulted. Unset.
 * ------------------------------------------------------------------------ */

/* clang-format off */
#define OA_CONFIG_FIELDS(X)                                                                        \
    /* --- sampling ---------------------------------------------------------------------- */     \
    X(sample_rate_hz, SAMPLE_RATE_HZ, OA_REQ_ALWAYS, "Hz",                                         \
      "Measure the rate the barometer, IMU and high-g part can actually be read together over "    \
      "their buses without the loop overrunning, on real hardware, and state the margin.")         \
                                                                                                   \
    /* --- pad reference, taken during ARMED --------------------------------------------- */     \
    X(pad_reference_window_ms, PAD_REFERENCE_WINDOW_MS, OA_REQ_ALWAYS, "ms",                       \
      "Measure barometer noise on a settled pad and choose the averaging window that brings the "  \
      "reference error below the altitude resolution being claimed. State the altitude error an "  \
      "operator incurs by walking away before the window completes.")                              \
    X(pad_reference_min_samples, PAD_REFERENCE_MIN_SAMPLES, OA_REQ_ALWAYS, "samples",              \
      "Follows from the window and the sample rate, but is stored separately so a reference "      \
      "taken during a sensor dropout is rejected rather than averaged from too few readings.")     \
                                                                                                   \
    /* --- ARMED to BOOST, launch detection ---------------------------------------------- */     \
    X(launch_accel_threshold_cg, LAUNCH_ACCEL_THRESHOLD_CG, OA_REQ_ALWAYS, "0.01 g",               \
      "Measure pad noise on real hardware including a deliberately clumsy rail knock, and state "  \
      "the threshold with the motor class it was tuned against.")                                  \
    X(launch_confirm_samples, LAUNCH_CONFIRM_SAMPLES, OA_REQ_ALWAYS, "samples",                    \
      "Choose from the same measured pad noise. Too few triggers on a knock, too many delays "     \
      "detection and lengthens the span the pre-launch ring buffer has to recover.")                \
                                                                                                   \
    /* --- BOOST to COAST, burnout ------------------------------------------------------- */     \
    X(burnout_confirm_samples, BURNOUT_CONFIRM_SAMPLES, OA_REQ_ALWAYS, "samples",                  \
      "Confirm against a logged boost that acceleration falling through zero is not chattering "   \
      "on a rough burn.")                                                                          \
    X(burnout_accel_hysteresis_cg, BURNOUT_ACCEL_HYSTERESIS_CG, OA_REQ_OPTIONAL, "0.01 g",         \
      "Open question: whether burnout detection needs hysteresis at all. Unset means no "          \
      "hysteresis band is applied and detection is on the sign change alone.")                     \
                                                                                                   \
    /* --- COAST to APOGEE --------------------------------------------------------------- */     \
    X(apogee_confirm_samples, APOGEE_CONFIRM_SAMPLES, OA_REQ_ALWAYS, "samples",                    \
      "Set from measured barometer noise at the configured output data rate, and publish the "     \
      "resulting detection lag in milliseconds, because that lag is an error in the recorded "     \
      "apogee time and a reader is entitled to know its size.")                                    \
                                                                                                   \
    /* --- APOGEE to DESCENT ------------------------------------------------------------- */     \
    X(descent_confirm_samples, DESCENT_CONFIRM_SAMPLES, OA_REQ_ALWAYS, "samples",                  \
      "Set from the same measured barometer noise. The deployment jolt happens in this window "    \
      "and a flown log is needed to see what it does to the signal.")                              \
                                                                                                   \
    /* --- DESCENT to LANDED ------------------------------------------------------------- */     \
    X(landing_alt_band_cm, LANDING_ALT_BAND_CM, OA_REQ_ALWAYS, "cm",                               \
      "Set from measured barometer drift over a long pad wait, since the band has to be wider "    \
      "than the zero can drift during a flight.")                                                  \
    X(landing_accel_band_cg, LANDING_ACCEL_BAND_CG, OA_REQ_ALWAYS, "0.01 g",                       \
      "Set from measured accelerometer noise on a stationary payload lying on the ground.")        \
    X(landing_hold_ms, LANDING_HOLD_MS, OA_REQ_ALWAYS, "ms",                                       \
      "Long enough that a rocket swinging under a parachute near the ground is not called down. "  \
      "Determine what this detector does with a rocket hanging in a tree and state it, because "   \
      "the honest answer may be that it cannot tell.")                                             \
                                                                                                   \
    /* --- pre-launch ring buffer -------------------------------------------------------- */     \
    X(prelaunch_ring_ms, PRELAUNCH_RING_MS, OA_REQ_ALWAYS, "ms",                                   \
      "Must exceed the measured launch detection lag, so the log contains the moment of ignition " \
      "rather than starting a fraction of a second after it. Measure the lag first.")              \
                                                                                                   \
    /* --- fusion ------------------------------------------------------------------------ */     \
    X(fusion_tau_ms, FUSION_TAU_MS, OA_REQ_ALWAYS, "ms",                                           \
      "The complementary filter crossover. Set from measured barometer noise and measured "        \
      "accelerometer bias drift: it is the time above which the barometer is trusted and below "   \
      "which the accelerometer is.")                                                                \
    X(fusion_tau_boost_ms, FUSION_TAU_BOOST_MS, OA_REQ_ALWAYS, "ms",                               \
      "The same crossover during BOOST, where airflow over the static ports disturbs the "         \
      "pressure the sensor sees and the estimate has to lean on integrated acceleration. Needs "   \
      "a flown log to set honestly.")                                                              \
                                                                                                   \
    /* --- barometer plausibility, which raises BARO_FAULT -------------------------------- */    \
    X(baro_plausible_min_pa, BARO_PLAUSIBLE_MIN_PA, OA_REQ_ALWAYS, "Pa",                           \
      "The band outside which a reading is treated as a sensor fault rather than as weather. "     \
      "Set from the barometer datasheet range and the launch sites the payload is flown from.")    \
    X(baro_plausible_max_pa, BARO_PLAUSIBLE_MAX_PA, OA_REQ_ALWAYS, "Pa",                           \
      "As above, at the top of the band.")                                                          \
    X(baro_max_rate_pa_s, BARO_MAX_RATE_PA_S, OA_REQ_OPTIONAL, "Pa/s",                             \
      "A jump detector for a barometer that starts returning garbage without leaving its "         \
      "plausible band. Unset means no rate check is applied. Needs flown data to set, because a "  \
      "real transonic pressure excursion must not trip it.")                                       \
                                                                                                    \
    /* --- battery ----------------------------------------------------------------------- */     \
    X(batt_low_mv, BATT_LOW_MV, OA_REQ_ALWAYS, "mV",                                               \
      "The threshold that raises LOW_BATT. Set from the measured cell discharge curve and the "    \
      "measured sag under radio transmit, not from the cell's nominal cutoff.")                    \
                                                                                                    \
    /* --- logging rates per phase ------------------------------------------------------- */     \
    X(log_hz_armed, LOG_HZ_ARMED, OA_REQ_ALWAYS, "Hz",                                             \
      "Set once flash write bandwidth and the resulting file size for a representative flight "    \
      "have been measured, rather than defaulting to a round number.")                             \
    X(log_hz_boost, LOG_HZ_BOOST, OA_REQ_ALWAYS, "Hz", "As above. Boost is the phase that needs "  \
      "the highest rate and lasts the shortest time.")                                             \
    X(log_hz_coast, LOG_HZ_COAST, OA_REQ_ALWAYS, "Hz", "As above.")                                \
    X(log_hz_descent, LOG_HZ_DESCENT, OA_REQ_ALWAYS, "Hz", "As above. Descent lasts far longer "   \
      "than boost and carries far less information per second, and battery spent here is "         \
      "battery the recovery beacon does not have.")                                                \
    X(log_hz_landed, LOG_HZ_LANDED, OA_REQ_ALWAYS, "Hz", "As above.")                              \
    X(gnss_log_hz, GNSS_LOG_HZ, OA_REQ_GNSS, "Hz",                                                 \
      "The rate gnss.bin records are appended at, bounded by the rate the receiver actually "      \
      "produces solutions at, which has to be measured with the airborne dynamic model set.")      \
                                                                                                    \
    /* --- radio ------------------------------------------------------------------------- */     \
    X(radio_freq_hz, RADIO_FREQ_HZ, OA_REQ_RADIO, "Hz",                                            \
      "There is deliberately no default. The legal frequency depends on the region and on "        \
      "whether the operator is flying under an unlicensed allocation or an amateur licence, and "  \
      "a firmware that shipped a frequency would transmit on it in a country where that is "       \
      "illegal.")                                                                                  \
    X(radio_bandwidth_hz, RADIO_BANDWIDTH_HZ, OA_REQ_RADIO, "Hz",                                  \
      "Part of the link budget, which has not been measured. Set together with the spreading "     \
      "factor and coding rate, and publish the achieved range and duty cycle that resulted.")      \
    X(radio_spreading_factor, RADIO_SPREADING_FACTOR, OA_REQ_RADIO, "SF",                          \
      "As above. It trades data rate against range, which sets the achievable packet rate, which " \
      "is why the packet spec states no absolute rates.")                                          \
    X(radio_coding_rate, RADIO_CODING_RATE, OA_REQ_RADIO, "4/N denominator",                       \
      "As above.")                                                                                 \
    X(radio_tx_power_dbm, RADIO_TX_POWER_DBM, OA_REQ_RADIO, "dBm",                                 \
      "Regionally limited and licence dependent, and it drives the current draw that decides "     \
      "beacon endurance. No default for the same reason as the frequency.")                        \
    X(radio_preamble_symbols, RADIO_PREAMBLE_SYMBOLS, OA_REQ_RADIO, "symbols",                     \
      "Set from the receiver sensitivity measured on a real link, not from a module datasheet "    \
      "example.")                                                                                  \
                                                                                                    \
    /* --- transmit scheduling ----------------------------------------------------------- */     \
    X(tx_interval_pad_ms, TX_INTERVAL_PAD_MS, OA_REQ_RADIO, "ms",                                  \
      "The STATUS interval during PAD_IDLE and ARMED. Set from the measured airtime at the "       \
      "shipped radio configuration and the regional duty cycle limit.")                            \
    X(tx_interval_flight_ms, TX_INTERVAL_FLIGHT_MS, OA_REQ_RADIO, "ms",                            \
      "The FLIGHT interval from BOOST through DESCENT. Must not be shorter than the measured "     \
      "airtime of a 19 byte packet at the shipped configuration.")                                 \
    X(tx_interval_beacon_ms, TX_INTERVAL_BEACON_MS, OA_REQ_RADIO, "ms",                            \
      "The initial BEACON interval after LANDED.")                                                 \
    X(tx_interval_beacon_max_ms, TX_INTERVAL_BEACON_MAX_MS, OA_REQ_RADIO, "ms",                    \
      "The ceiling the beacon interval lengthens to, trading update rate for endurance during a "  \
      "long search. Set from measured beacon endurance from landing detect to cell cutoff.")       \
    X(tx_beacon_stretch_ms, TX_BEACON_STRETCH_MS, OA_REQ_RADIO, "ms",                              \
      "How much the beacon interval lengthens per transmission. Set alongside the ceiling.")       \
    X(tx_apogee_repeat, TX_APOGEE_REPEAT, OA_REQ_RADIO, "count",                                   \
      "How many times the APOGEE packet repeats. Set from the measured packet loss rate at the "   \
      "distance and attitude apogee actually happens at, which needs a flight.")                   \
    X(tx_apogee_repeat_interval_ms, TX_APOGEE_REPEAT_INTERVAL_MS, OA_REQ_RADIO, "ms",              \
      "Spacing between those repeats. Long enough to survive a burst of interference, short "      \
      "enough to finish before the deployment event that is the most likely moment for the "       \
      "payload to be damaged.")                                                                    \
    X(tx_position_interleave, TX_POSITION_INTERLEAVE, OA_REQ_GNSS, "FLIGHT packets per POSITION",  \
      "How often a POSITION packet displaces a FLIGHT packet on Track. Position changes slowly "   \
      "compared to altitude, and the right ratio needs a flown log to choose.")                    \
                                                                                                    \
    /* --- buzzer and LED ---------------------------------------------------------------- */     \
    X(buzzer_freq_hz, BUZZER_FREQ_HZ, OA_REQ_ALWAYS, "Hz",                                         \
      "The drive frequency. LS1 is a Murata PKLCS1212E4001-R1 and its datasheet resonance is "     \
      "4 kHz, so the starting value is known. It stays unset because loudest is not the same as "  \
      "easiest to walk toward: measure which frequency is actually findable in the open, over "    \
      "wind, and set it from that rather than from the peak on a bench.")                          \
    X(buzzer_armed_period_ms, BUZZER_ARMED_PERIOD_MS, OA_REQ_ALWAYS, "ms",                         \
      "The armed indication has to be recognisable from several metres away while the operator "  \
      "walks back. Choose it, then confirm it is audible over a launch site.")                     \
    X(buzzer_armed_on_ms, BUZZER_ARMED_ON_MS, OA_REQ_ALWAYS, "ms",                                 \
      "Duty of the armed pattern. Also a current draw, which comes out of pad wait endurance.")    \
    X(buzzer_beacon_period_ms, BUZZER_BEACON_PERIOD_MS, OA_REQ_ALWAYS, "ms",                       \
      "The recovery pattern after LANDED. On a Solo build this is the only recovery aid there "    \
      "is, so measure how far away it is actually audible in grass and in wind.")                  \
    X(buzzer_beacon_on_ms, BUZZER_BEACON_ON_MS, OA_REQ_ALWAYS, "ms",                               \
      "Duty of the recovery pattern, and the dominant current draw during a search.")              \
    X(led_brightness_pct, LED_BRIGHTNESS_PCT, OA_REQ_OPTIONAL, "%",                                \
      "Unset means full brightness. It is optional because it trades visibility against a "        \
      "current draw that has not been measured, and neither side of that trade is known yet.")     \
                                                                                                   \
    /* --- sensor health, applied by oa_health.c ----------------------------------------- */     \
    /* These lived in a second table, OA_HEALTH_LIMITS in oa_health.h, which the config     */     \
    /* parser had never heard of. That made them unsettable: every health check was         */     \
    /* permanently unset, so BARO_FAULT and IMU_FAULT could never be raised, and the         */     \
    /* baro_valid gate in the state machine could never fire from a real fault. Three of      */     \
    /* the baro thresholds were already here, which is what made the split arbitrary rather   */     \
    /* than principled. OPTIONAL because an unset limit means the check is not performed,     */     \
    /* which is a state this firmware is comfortable with and reports in checks_unset.       */     \
    X(baro_stuck_samples, BARO_STUCK_SAMPLES, OA_REQ_OPTIONAL, "samples",                         \
      "How many byte-identical consecutive pressure readings mean the sensor has stopped rather "  \
      "than the air being still. Measure the barometer's noise on a settled pad: the count has to "\
      "be longer than the longest genuine repeat at the configured resolution and output rate.")   \
    X(baro_stale_ms, BARO_STALE_MS, OA_REQ_OPTIONAL, "ms",                                        \
      "How long without a successful read before the barometer is declared dead. Measure the "     \
      "worst case bus retry time on real hardware, since a value shorter than that faults a "      \
      "healthy sensor during a transient the driver was about to recover from.")                   \
    X(imu_stuck_samples, IMU_STUCK_SAMPLES, OA_REQ_OPTIONAL, "samples",                           \
      "As above, for all six IMU axes at once. An exact repeat across six noisy axes is far less " \
      "likely than across one, so this count can be shorter than the barometer's, but by how "     \
      "much needs the IMU's measured noise floor to say.")                                         \
    X(imu_stale_ms, IMU_STALE_MS, OA_REQ_OPTIONAL, "ms",                                          \
      "As above, for the IMU.")                                                                    \
    X(imu_accel_max_mg, IMU_ACCEL_MAX_MG, OA_REQ_OPTIONAL, "mg",                                  \
      "The magnitude beyond which an accelerometer axis is not a reading. It follows from the "    \
      "full-scale range oApogee configures on the 6-axis part, which is itself an open question "  \
      "recorded against the imu entry in data/bom.yaml. Note that a reading AT full scale is "     \
      "saturation and not a fault: the log format says a saturated axis reads as a flat plateau, " \
      "and this limit exists to catch a part reporting past its own range.")                       \
    X(imu_gyro_max_cdps, IMU_GYRO_MAX_CDPS, OA_REQ_OPTIONAL, "0.01 deg/s",                        \
      "As above, for the gyroscope, and bounded also by the 327.67 deg/s the log format's i16 "    \
      "can hold. The peak roll rate of a real flight has never been recorded, and until it is "    \
      "there is no honest way to separate a fast roll from a broken part.")
/* clang-format on */

/* ---------------------------------------------------------------------------
 * Field identifiers.
 * ------------------------------------------------------------------------ */

#define OA_CONFIG_ENUMERATOR(name, NAME, req, unit, why) OA_CFG_##NAME,

typedef enum {
    OA_CONFIG_FIELDS(OA_CONFIG_ENUMERATOR)
    OA_CFG_FIELD_COUNT
} oa_config_field_t;

#undef OA_CONFIG_ENUMERATOR

#define OA_CONFIG_SET_BYTES ((OA_CFG_FIELD_COUNT + 7) / 8)

/* ---------------------------------------------------------------------------
 * The configuration.
 *
 * Named members so call sites read as cfg->apogee_confirm_samples rather than
 * cfg->v[OA_CFG_APOGEE_CONFIRM_SAMPLES], which is greppable and survives a
 * reordering of the table.
 *
 * `set` is the authority on whether a field has a value. The sentinel in the
 * member is the second line of defence, not the first: a field is unset because
 * nobody set it, not because it happens to hold a magic number. oa_config_init
 * establishes both, and an implementation of oa_config_set that updates one
 * without the other is a defect the tests look for.
 *
 * The generic accessors need to reach a named member from a field index. The
 * implementation builds that mapping by expanding OA_CONFIG_FIELDS into a table
 * of offsetof(), from this same list, so the two cannot fall out of step.
 * ------------------------------------------------------------------------ */

#define OA_CONFIG_MEMBER(name, NAME, req, unit, why) oa_tunable_t name;

typedef struct {
    OA_CONFIG_FIELDS(OA_CONFIG_MEMBER)

    /* One bit per field, indexed by oa_config_field_t. */
    uint8_t set[OA_CONFIG_SET_BYTES];

    /* Not tunables, and deliberately outside the table above.
     *
     * `simulated` has a meaningful default: false, a real flight. It sets the
     * SIM flag on every packet and session.simulated in every manifest, and it
     * is the one bit that stops a bench run being published as a flight.
     *
     * `callsign` is empty unless the operator is transmitting under an amateur
     * licence, where identification is a legal requirement the firmware cannot
     * infer. Empty is a valid state, so it has no set bit and never blocks
     * arming. Whether the firmware should refuse to transmit without one is a
     * question about which legal regime the operator chose, and the firmware
     * does not know that.
     *
     * Decided: an explicit part15 / part97 mode belongs here, so that a missing
     * callsign is an error in the mode where it is one rather than a silent
     * omission. The safety page says station identification "has to be built
     * into the firmware, not remembered on the day", and that is only true if
     * the firmware knows which regime it is operating under.
     * TODO(confirm-on-hardware): add the mode field and make arming fail
     * without a callsign in part97. */
    bool simulated;
    char callsign[16];
} oa_config_t;

#undef OA_CONFIG_MEMBER

_Static_assert(OA_CFG_FIELD_COUNT > 0, "the configuration table is empty");
_Static_assert(OA_CONFIG_SET_BYTES * 8 >= OA_CFG_FIELD_COUNT,
               "the set bitmap is too small for the field table");

/* ---------------------------------------------------------------------------
 * Validation.
 * ------------------------------------------------------------------------ */

/* What a failed flightworthiness check reports. A count and a bitmap rather than
 * just a yes or no, because the useful thing to show an operator is every number
 * that is missing, with what would settle each one, not the first one the
 * validator happened to reach. */
typedef struct {
    size_t  missing_count;
    uint8_t missing[OA_CONFIG_SET_BYTES];
} oa_config_report_t;

/* Every tunable OA_UNSET, every set bit clear, simulated false, callsign empty.
 * This is the only correct starting state, and it is the state a payload with no
 * configuration file is in. */
void oa_config_init(oa_config_t *cfg);

/* Set and read a field by index. oa_config_set writes the member and raises the
 * set bit together. Setting a field to OA_UNSET is an error rather than a way to
 * clear it, because it would leave the set bit and the sentinel disagreeing;
 * use oa_config_clear. */
oa_result_t oa_config_set(oa_config_t *cfg, oa_config_field_t field, oa_tunable_t value);
oa_result_t oa_config_get(const oa_config_t *cfg, oa_config_field_t field, oa_tunable_t *out);
oa_result_t oa_config_clear(oa_config_t *cfg, oa_config_field_t field);
bool        oa_config_is_set(const oa_config_t *cfg, oa_config_field_t field);

/* Table metadata, for the configuration file parser, the serial console, and the
 * message a payload prints when it refuses to arm. */
const char     *oa_config_field_name(oa_config_field_t field);
const char     *oa_config_field_unit(oa_config_field_t field);
const char     *oa_config_field_why(oa_config_field_t field);
oa_config_req_t oa_config_field_requirement(oa_config_field_t field);

/* Look a field up by its name in the configuration file. Returns OA_ERR_RANGE
 * for a key this firmware does not define, so an unknown key is reported to the
 * operator rather than silently ignored. A typo in a threshold name that was
 * ignored would leave that threshold unset, and the operator would find out at
 * the rail. */
oa_result_t oa_config_field_by_name(const char *name, oa_config_field_t *out);

/* Whether this configuration is complete enough to arm, for a build with these
 * features.
 *
 * True only when every OA_REQ_ALWAYS field is set, every OA_REQ_RADIO field is
 * set if OA_FEATURE_RADIO is present, and every OA_REQ_GNSS field is set if
 * OA_FEATURE_GNSS is present. OA_REQ_OPTIONAL fields never block.
 *
 * `report` may be NULL. When it is not, it is filled in whether or not the
 * check passes, so a caller can list every missing field with
 * oa_config_field_why() beside it.
 *
 * This function is the whole reason the configuration is shaped this way. It is
 * what turns "no threshold has been measured yet" from a comment in a spec into
 * a payload that will not arm. */
bool oa_config_is_flightworthy(const oa_config_t *cfg,
                               oa_features_t features,
                               oa_config_report_t *report);

/* True when this specific field is required for a build with these features. */
bool oa_config_field_is_required(oa_config_field_t field, oa_features_t features);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_CONFIG_H */

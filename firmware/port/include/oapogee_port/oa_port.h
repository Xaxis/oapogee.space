/*
 * oApogee port layer: the entire hardware surface, as declarations.
 *
 * Everything this firmware needs from a microcontroller, a sensor, a flash part
 * or a radio is declared in this one file. core/ includes none of it. That split
 * is the reason core can be built and tested on a laptop today, with no SDK
 * installed and no board in existence, and it is the reason a second target
 * would be a second implementation of this header rather than a fork.
 *
 * There are no definitions here. Not one function in this file has been
 * implemented, and no board has been fabricated, so nothing declared here has
 * ever run.
 *
 * THE OUTPUTS ARE NOT IN THIS FILE. Every physical output the firmware may drive
 * is in oa_out.h and in port/outputs.allowlist, and there are exactly two of
 * them. Anything that acts on the world belongs there so that a reviewer has one
 * short list to check rather than this long one.
 *
 * THERE IS NO RECEIVE FUNCTION FOR THE RADIO, ANYWHERE IN THIS HEADER. That
 * absence is deliberate and it is the downlink-only property in code: there is
 * no path by which anything transmitted at this payload can reach any of its
 * logic. Adding one would be a change to the passive payload boundary.
 *
 * Conventions:
 *   Every fallible call returns oa_result_t. Nothing returns a sentinel that is
 *   also a legal reading.
 *   Nothing here allocates.
 *   A sensor read fills a caller-owned struct and never blocks longer than the
 *   bus transaction, because it is called from the sample loop.
 */

#ifndef OAPOGEE_PORT_OA_PORT_H
#define OAPOGEE_PORT_OA_PORT_H

#include "oapogee/oa_log.h"
#include "oapogee/oa_sink.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Lifecycle and identity.
 * ------------------------------------------------------------------------ */

/* Bring up clocks, buses, storage and every populated peripheral. Called once.
 * A peripheral that fails to initialise does not fail this call: the payload
 * still flies, with the corresponding fault flag raised, because a flight with a
 * dead barometer is worth more than no flight. Query oa_port_features to find
 * out what actually came up. */
oa_result_t oa_port_init(void);

/* Which parts are populated and healthy, as OA_FEATURE_* bits from oa_config.h.
 * Determined by probing, not by a build-time tier switch: one firmware image
 * runs on Solo, Link and Track, and the upgrade promise is that populating
 * footprints is all it takes. */
oa_features_t oa_port_features(void);

/* Stable device identifier derived from the microcontroller's unique id, in the
 * "oapogee-000000000000" form that meta.json expects. Writes at most `cap` bytes
 * including the terminator. Stable across reflashes, which is what lets a flight
 * archive group flights by airframe. */
oa_result_t oa_port_device_id(char *out, size_t cap);

/* Firmware version and the git commit it was built from, for meta.json. A flight
 * log has to be able to say exactly what was flying. */
const char *oa_port_fw_version(void);
const char *oa_port_fw_git(void);

/* Hardware revision, or NULL when the board carries none. NULL today: no board
 * has been fabricated. */
const char *oa_port_hw_rev(void);

/* ---------------------------------------------------------------------------
 * Time.
 *
 * Milliseconds and microseconds since power-on, both monotonic. Neither is ever
 * transmitted: t_ms on the air and in the log is milliseconds since arming, and
 * the payload's uptime counter is a firmware implementation detail. There is no
 * real-time clock on this board, deliberately, because a coin cell and a crystal
 * cost mass and money to supply a timestamp that a laptop supplies for free at
 * offload.
 * ------------------------------------------------------------------------ */

uint32_t oa_port_millis(void);
uint64_t oa_port_micros(void);
void     oa_port_delay_ms(uint32_t ms);

/* ---------------------------------------------------------------------------
 * Operator input.
 *
 * The arming switch is the only thing outside the payload that can reach the
 * flight state machine. It is on a GPIO rather than in the power path: oApogee
 * is passive, so cutting power arms nothing and protects nobody. What the
 * operator needs is to power the payload up, let the GNSS get a fix and the
 * barometer settle, then arm it once the rocket is on the rail.
 * ------------------------------------------------------------------------ */

/* Debounced arming input. True means armed. The input is pulled up and the
 * switch pulls down; a floating input reads as noise, and an input that reads as
 * noise arms a rocket at random. */
bool oa_port_arm_switch(void);

/* ---------------------------------------------------------------------------
 * Sensors.
 *
 * Each read fills a caller-owned struct with raw values in the units the log
 * format stores, so that nothing between the sensor and the log rescales
 * anything. A read that fails leaves the struct untouched and returns non-OK;
 * the caller raises the fault flag and carries the last value forward, because
 * the packet format says a faulted sensor's fields carry the last value read and
 * the flag is what tells you not to trust them. Transmitting a sentinel instead
 * would produce a plot with a spike in it rather than a gap, which is worse.
 * ------------------------------------------------------------------------ */

typedef struct {
    int32_t pressure_pa; /* Raw, whole pascals. */
    int16_t temp_dc;     /* Die temperature, 0.1 degC. */
} oa_port_baro_sample_t;

typedef struct {
    int16_t accel_mg[3];  /* X Y Z, milli-g. */
    int16_t gyro_cdps[3]; /* X Y Z, 0.01 deg/s. */
} oa_port_imu_sample_t;

typedef struct {
    int16_t accel_dg[3]; /* X Y Z, 0.1 g. */
} oa_port_highg_sample_t;

typedef struct {
    int32_t lat_e7;  /* Degrees x 10^7, the u-blox native scaling. */
    int32_t lon_e7;
    int16_t alt_m;   /* Ellipsoid referenced, whole metres, clamped not wrapped. */
    uint8_t sats;
    uint8_t fix;     /* u-blox convention: 0 none, 2 two-dimensional, 3 three-dimensional. */
} oa_port_gnss_fix_t;

oa_result_t oa_port_baro_read(oa_port_baro_sample_t *out);
oa_result_t oa_port_imu_read(oa_port_imu_sample_t *out);

/* Returns OA_ERR_UNSUPPORTED when no high-g part is populated, which is the
 * normal case on Solo. The caller writes zeros into hg_accel_dg and leaves
 * OA_FLAG_HIGH_G clear; the field keeps its six bytes in the record regardless,
 * because a record whose width changed with the populated footprints would not
 * be a fixed-width record. */
oa_result_t oa_port_highg_read(oa_port_highg_sample_t *out);

/* Non-blocking. Returns OA_ERR_EMPTY when no new solution has arrived, which is
 * most calls, and OA_ERR_UNSUPPORTED on a build with no receiver. A record is
 * only appended to gnss.bin when this returns a fix, so gaps in the log are real
 * and mean the receiver had no solution. */
oa_result_t oa_port_gnss_poll(oa_port_gnss_fix_t *out);

/* Configure the receiver's dynamic platform model for flight.
 *
 * u-blox receivers ship assuming ground vehicle behaviour and reject their own
 * solutions under rocket acceleration, at exactly the moment tracking is worth
 * having. This is the single most common reason a GNSS-equipped model rocket
 * payload comes back with no track, and it is a configuration problem rather
 * than a hardware one. Called at every boot.
 *
 * TODO(verify): confirm the specific configuration messages required, and
 * confirm by flight that lock is retained through boost once they are applied. */
oa_result_t oa_port_gnss_set_airborne_model(void);

/* Battery voltage in millivolts, through the divider. The divider ratio, the
 * reference voltage and the ADC's nonlinearity all live under this call; nothing
 * above it knows the board has a divider at all.
 *
 * TODO(confirm-on-hardware): no divider has been built and no ADC has been
 * characterised, so the conversion in the implementation of this function is
 * currently the least trustworthy number in the firmware. */
oa_result_t oa_port_battery_mv(int32_t *out_millivolts);

/* ---------------------------------------------------------------------------
 * Storage.
 *
 * LittleFS on the soldered-down QSPI flash, chosen because a payload can lose
 * power at any instant: an impact can break a battery connection and a hard
 * landing can reset the microcontroller. A filesystem that corrupts on power
 * loss loses the whole flight, including the part that was already safely
 * written.
 *
 * TODO(verify): confirm that LittleFS on the chosen flash part actually survives
 * power loss mid-write in practice, by writing continuously and cutting power
 * repeatedly, then checking that every completed record is intact. This is the
 * assumption the whole storage design rests on and it should be demonstrated
 * rather than trusted.
 * ------------------------------------------------------------------------ */

oa_result_t oa_port_storage_mount(void);

/* Create the next flight directory and return its number. Directory names are
 * zero-padded decimal, monotonically increasing, and never reused. Called at the
 * transition into ARMED, before any record is written, so a flight that ends in
 * a crash and a truncated file still has a directory and a manifest. */
oa_result_t oa_port_log_begin_flight(uint32_t *out_flight_number);

/* Open one of the flight's files and bind a sink to it. The sink is valid until
 * oa_port_log_end_flight. core writes through the sink and knows nothing about
 * the filesystem underneath it.
 *
 * meta.json is opened with oa_port_log_open_manifest so that it can be rewritten
 * at LANDED with the summary filled in, which is the one file in the directory
 * that is written twice. */
oa_result_t oa_port_log_open_stream(oa_log_stream_t stream, oa_sink_t *out_sink);
oa_result_t oa_port_log_open_manifest(oa_sink_t *out_sink);

/* Close everything and flush. Safe to call more than once. */
oa_result_t oa_port_log_end_flight(void);

/* Free space in bytes. The flight loop uses it to raise OA_FLAG_LOG_FULL and
 * stop logging while continuing to fly and continuing to transmit. A flight does
 * not end because the flash filled up. */
size_t oa_port_storage_free_bytes(void);

/* ---------------------------------------------------------------------------
 * Radio. Transmit only.
 *
 * There is no receive function here and there will not be one. See the note at
 * the top of this file, oa_out.h, and firmware/SAFETY.md.
 * ------------------------------------------------------------------------ */

/* Apply the radio configuration. Returns OA_ERR_UNSET when any required radio
 * field is missing from the configuration, and does not transmit.
 *
 * There is deliberately no default frequency, bandwidth, spreading factor,
 * coding rate or transmit power anywhere in this firmware. The legal values
 * depend on the region and on whether the operator is flying under an unlicensed
 * allocation or an amateur licence, and a firmware that shipped a frequency
 * would transmit on it in a country where doing so is illegal. */
oa_result_t oa_port_radio_configure(const oa_config_t *cfg);

/* Queue one packet. Non-blocking: returns OA_OK once the packet is handed to the
 * radio, and the caller polls oa_port_radio_busy. A transmit that fails is
 * reported so that the scheduler does not advance its sequence number, because
 * a seq gap is supposed to mean a receiver lost a packet and nothing else. */
oa_result_t oa_port_radio_send(const uint8_t *packet, size_t len);

/* True while the last packet is still going out. */
bool oa_port_radio_busy(void);

/* ---------------------------------------------------------------------------
 * Host console.
 *
 * USB serial. Used for the boot banner, for reporting exactly which
 * configuration fields are unset when the payload refuses to arm, and for
 * offload. Never load bearing in flight: a payload that behaves differently with
 * a laptop attached is a payload that was tested in a state it never flies in.
 * ------------------------------------------------------------------------ */

oa_result_t oa_port_console_write(const char *text);
bool        oa_port_console_connected(void);

/* ---------------------------------------------------------------------------
 * Watchdog.
 *
 * Decided: the watchdog is enabled in flight, and the peak altitude is
 * committed to flash as it updates so that a reset does not lose it.
 *
 * The two arguments are not symmetric once the mitigation is on the table. A
 * hung payload logs nothing, beacons nothing, and cannot be found; a reset that
 * keeps the beacon alive preserves recovery, and a rocket you recover can have
 * its log read afterwards while a rocket you cannot find yields nothing at all.
 * The cost of the reset, losing the in-memory peak, is the part that is fixable,
 * so it is fixed rather than accepted. The reasoning is in
 * docs/open-questions.md.
 *
 * TODO(confirm-on-hardware): implement the peak commit and confirm the timeout
 * against real loop timing once a board exists.
 * ------------------------------------------------------------------------ */

oa_result_t oa_port_watchdog_enable(uint32_t timeout_ms);
void        oa_port_watchdog_feed(void);

/* Reboot into the bootloader so the board appears as a USB drive. The only way
 * out of the firmware, and it is operator-initiated over the console. */
void oa_port_reboot_to_bootloader(void);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_PORT_OA_PORT_H */

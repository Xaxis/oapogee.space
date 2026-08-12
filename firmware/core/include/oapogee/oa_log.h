/*
 * oApogee onboard log: record packing and manifest emission.
 *
 * The record structs, the length constants, the packers and the manifest writer
 * are all expanded from oapogee/oa_log_fields.def. The manifest is the reason
 * that matters: this format is self-describing, so a meta.json that disagreed
 * with the packer would produce files that decode wrongly and look fine. One
 * table, expanded twice, makes that disagreement a compile error instead of a
 * quiet data loss.
 *
 * Source of truth: docs/spec/log-format.md, spec_version 1.
 *
 * Everything writes through an oa_sink_t. core does not know what a filesystem
 * is. Assigning flight numbers, creating /flights/NNNN/, and opening files are
 * the port layer's job, because they are the parts that need LittleFS.
 *
 * Nothing in this file has run on hardware, and no flight has been logged.
 */

#ifndef OAPOGEE_OA_LOG_H
#define OAPOGEE_OA_LOG_H

#include "oapogee/oa_config.h"
#include "oapogee/oa_sink.h"
#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version of docs/spec/log-format.md this writer implements. Written into
 * meta.json as spec_version. */
#define OA_LOG_SPEC_VERSION (1)

/* ---------------------------------------------------------------------------
 * Record sizes, from the table.
 * ------------------------------------------------------------------------ */

enum {
#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes) \
    OA_LOG_##SYM##_RECORD_BYTES = (record_bytes),
#define OA_LOG_FIELD(name, wire, offset, scale, unit)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)
#include "oapogee/oa_log_fields.def"
    OA_LOG_RECORD_BYTES_END = 0
};

/* ---------------------------------------------------------------------------
 * Record structs, from the table.
 *
 * Host-order decoded values, not overlays. The packers write field by field in
 * little-endian order and never copy one of these structs, because a struct
 * overlay depends on packing attributes and on the host's endianness, and this
 * format is meant to be readable by anyone's tools on anyone's machine.
 *
 * That also means sizeof(oa_log_flight_t) is not 36 and is not supposed to be.
 * The 36 is the packed length, checked by the conformance test against the
 * bytes the packer produces.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes) typedef struct {
#define OA_LOG_FIELD(name, wire, offset, scale, unit) oa_##wire##_t name;
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit) oa_##wire##_t name[count];
#define OA_LOG_STREAM_END(SYM, sym) \
    }                               \
    oa_log_##sym##_t;
#include "oapogee/oa_log_fields.def"

/* ---------------------------------------------------------------------------
 * Packers and writers, from the table.
 *
 * oa_log_pack_<stream> writes exactly OA_LOG_<SYM>_RECORD_BYTES bytes into a
 * caller-owned buffer. oa_log_write_<stream> packs and pushes through a sink in
 * one call, which is what the flight loop uses.
 *
 * The contract every packer satisfies:
 *   1. every field lands at the offset in the table
 *   2. every multi-byte field is little-endian
 *   3. nothing is written at all if the buffer is too small
 *   4. every byte of the record is written, including any the table does not
 *      name, so a record never carries stale bytes from a previous one
 *
 * Rule 4 costs nothing today because both records are fully covered by named
 * fields. It is stated because the first time a reserved gap appears in a record
 * is the first time an unwritten byte becomes a leak of the previous sample.
 * ------------------------------------------------------------------------ */

#define OA_LOG_STREAM_BEGIN(SYM, sym, filename, record_bytes)                     \
    oa_result_t oa_log_pack_##sym(const oa_log_##sym##_t *rec, uint8_t *out, size_t out_cap); \
    oa_result_t oa_log_write_##sym(const oa_log_##sym##_t *rec, oa_sink_t *sink);
#define OA_LOG_FIELD(name, wire, offset, scale, unit)
#define OA_LOG_FIELD_ARR(name, wire, offset, count, scale, unit)
#include "oapogee/oa_log_fields.def"

/* The filename from the table, "flight.bin" and "gnss.bin". The port layer needs
 * it to open the file and the manifest needs it in the stream object, and those
 * two must be the same string or the manifest names a file that is not there. */
typedef enum {
    OA_LOG_STREAM_FLIGHT = 0,
    OA_LOG_STREAM_GNSS = 1,
    OA_LOG_STREAM_COUNT = 2
} oa_log_stream_t;

const char *oa_log_stream_filename(oa_log_stream_t stream);
size_t      oa_log_stream_record_bytes(oa_log_stream_t stream);

/* ---------------------------------------------------------------------------
 * Manifest.
 * ------------------------------------------------------------------------ */

/* Summary written at LANDED. Every numeric member is a tunable-typed value so
 * that OA_UNSET means "not known" and is written as JSON null, which is what the
 * spec requires and what stops a reader treating an unfinished flight as a
 * measured zero.
 *
 * A caller that passes NULL where this is expected gets the creation-time
 * manifest: every summary member null and landed false. That is not a
 * convenience, it is a requirement of the format, and a manifest whose landed is
 * still false is itself information about how the flight ended. */
typedef struct {
    oa_tunable_t apogee_cm;
    oa_tunable_t t_apogee_ms;
    oa_tunable_t max_accel_cg;
    oa_tunable_t max_velocity_dm_s;
    oa_tunable_t flight_duration_ms;
    bool         landed;
} oa_log_summary_t;

/* Everything the manifest needs that core cannot know. Strings are borrowed for
 * the duration of the call and are never stored.
 *
 * A NULL string member is written as JSON null, which is the correct value for
 * hw_rev on a board that has no revision yet and for armed_utc on a build with
 * no GNSS. There is no real-time clock on this board, so every time in the log
 * is relative to arming and absolute time is attached at offload where it is
 * available. */
typedef struct {
    uint32_t    flight;           /* Directory number, assigned at ARMED. */
    const char *device_id;        /* From the microcontroller's unique id. */
    const char *tier;             /* "solo", "link", "track". */
    const char *path;             /* "board" or "modules". */
    const char *hw_rev;           /* NULL until a board has been fabricated. */
    const char *fw_version;
    const char *fw_git;
    const char *armed_utc;        /* NULL unless a GNSS fix supplied UTC. */
    uint32_t    armed_uptime_ms;
    bool        simulated;        /* True for any run that is not a flight. */

    /* Calibration. pad_pressure_pa is the zero the entire altitude column is
     * measured against, and publishing it with its sample count is what makes a
     * flight re-derivable when the reference turns out to have been taken
     * badly. */
    int32_t  pad_pressure_pa;
    uint32_t pad_pressure_samples;
    int16_t  pad_temperature_dc;
    int16_t  accel_bias_mg[3];
    int16_t  gyro_bias_cdps[3];

    /* Which streams this build wrote. The manifest must list exactly the files
     * present in the directory and nothing else, so a Solo or Link log carries
     * no gnss key at all. */
    bool has_gnss_stream;

    /* Configured rates, not achieved ones. OA_UNSET is written as null, which is
     * the honest value today: no log rate has been chosen, because choosing one
     * needs measured sensor noise and measured flash write bandwidth. */
    oa_tunable_t flight_nominal_hz;
    oa_tunable_t gnss_nominal_hz;
} oa_log_manifest_t;

/* Write meta.json through a sink.
 *
 * `summary` may be NULL, which produces the manifest written before the first
 * record: summary present, every member null, landed false.
 *
 * The stream layout in the output is expanded from oa_log_fields.def, so the
 * offsets, types, scales and units in the manifest are the same table the
 * packers use. Scales are emitted as the literal text from that table rather
 * than formatted from a float, because 1e-7 printed through a double formatter
 * on a microcontroller is a way to turn an exact scale factor into an
 * approximate one, in the one file that tells a reader how to interpret every
 * other byte. There is no floating point anywhere in this writer.
 *
 * Returns OA_OK, OA_ERR_NULL, or whatever the sink returned. */
oa_result_t oa_log_write_manifest(const oa_log_manifest_t *manifest,
                                  const oa_log_summary_t *summary,
                                  oa_sink_t *sink);

/* TODO(confirm): the manifest has no way to say that accel_bias_mg and
 * gyro_bias_cdps have never been measured. All zeros currently means both "no
 * calibration has been run" and "calibration ran and found no bias", and a
 * reader cannot tell which. Decide whether to add a calibration flag to
 * spec_version 2 or to accept the ambiguity, and record the decision in
 * docs/open-questions.md. */

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_OA_LOG_H */

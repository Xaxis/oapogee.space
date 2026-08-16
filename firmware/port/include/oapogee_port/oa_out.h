/*
 * THE PASSIVE BOUNDARY, IN CODE.
 *
 * oApogee is a passive instrumentation payload. It does not fire ejection
 * charges, control deployment, ignite motors, or command any pyrotechnic device.
 * It has no mechanism to do any of those things, and this file is where that
 * claim stops being a sentence in a document and becomes something a reviewer
 * can check in thirty seconds.
 *
 * This is the complete list of physical outputs the firmware may drive, and
 * these are the only functions that drive one. There are exactly two outputs: a
 * buzzer and a status LED. Both exist to help a person find a rocket and to tell
 * a person what state the payload is in. Neither can do anything else.
 *
 * HOW THIS IS ENFORCED, MECHANICALLY
 *
 *   1. The enum below is compared, name for name, against
 *      firmware/port/outputs.allowlist by the check-outputs build target. A
 *      third output added here without a line in that plain text file fails the
 *      build, and a line in that file with no enumerator fails it too. The
 *      allowlist is what a reviewer reads to confirm the boundary without
 *      reading any C.
 *
 *   2. The _Static_assert below fails if the count is anything but two.
 *
 *   3. core/ cannot call any of these functions. core is compiled against no
 *      SDK and links against nothing, and every external symbol its objects
 *      reference is checked against firmware/core/allowed-undefined.txt, which
 *      lists memcpy, memset and their close relatives and nothing else. A call
 *      to oa_out_buzzer_set from anywhere in core would appear as an undefined
 *      symbol that is not on that list, and the check-undefined target would
 *      fail. The state machine, the fusion filter, the scheduler and the packet
 *      builders therefore cannot drive anything at all, by construction rather
 *      than by convention.
 *
 * WHAT THIS FILE DOES NOT COVER, STATED PLAINLY
 *
 * Bus pins are not outputs in the sense this file uses the word. The firmware
 * drives I2C, SPI, UART and QSPI lines, and it drives the radio, and those are
 * declared in oa_port.h as buses and peripherals. They move bytes to parts that
 * are soldered to the board. The distinction that matters is that no output
 * declared anywhere in this firmware could be wired to an igniter, and the two
 * in this file are wired, on the schematic, to a piezo buzzer and an LED.
 *
 * The radio is the case worth being explicit about. It transmits, and there is
 * no receive function anywhere in oa_port.h. That absence is the downlink-only
 * property: a ground station cannot arm this payload, cannot trigger anything
 * and cannot deploy anything, because there is no code path from the air into
 * this firmware at all.
 *
 * NOTHING HERE HAS RUN ON HARDWARE. No board has been fabricated. This file
 * describes an interface that no implementation has yet satisfied.
 */

#ifndef OAPOGEE_PORT_OA_OUT_H
#define OAPOGEE_PORT_OA_OUT_H

#include "oapogee/oa_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Every physical output the firmware may drive.
 *
 * Keep this list, firmware/port/outputs.allowlist, and the recovery aids on the
 * schematic in agreement. The build target check-outputs enforces the first two;
 * the third is a human reading all three.
 * ------------------------------------------------------------------------ */

typedef enum {
    /* Piezo buzzer, LS1 on the schematic, on the pin labelled BUZZER. Arm
     * indication and the post-landing recovery beacon. On a Solo build it is the
     * only recovery aid there is, which is why it earns its mass. */
    OA_OUT_BUZZER = 0,

    /* RGB LED, D1 on the schematic, common cathode, on the three pins labelled
     * LED_R, LED_G and LED_B through series resistors R10, R11 and R12. Says
     * what state the payload is in from arm's length.
     *
     * One enumerator, three pins. The boundary this file protects is about what
     * the firmware is allowed to drive, not how many wires it takes to do it,
     * and an LED that needed three entries here would make the count of
     * outputs a fact about packaging rather than about capability. */
    OA_OUT_STATUS_LED = 1
} oa_out_t;

#define OA_OUT_COUNT (2)

_Static_assert(OA_OUT_COUNT == 2,
               "oApogee has exactly two outputs: the buzzer and the status LED. "
               "Adding a third is a change to the passive payload boundary and is "
               "not a firmware decision.");

_Static_assert((int)OA_OUT_STATUS_LED == OA_OUT_COUNT - 1,
               "the output enumerators must be dense and start at zero, so that "
               "the allowlist check can enumerate them");

/* ---------------------------------------------------------------------------
 * The only functions permitted to drive an output.
 *
 * There is deliberately no generic oa_out_write(oa_out_t, value). A generic
 * driver would make every output interchangeable at a call site, and the whole
 * point of this file is that these two are not a class of thing that can be
 * extended.
 * ------------------------------------------------------------------------ */

/* Drive the buzzer at `freq_hz`, or silence it when freq_hz is 0.
 *
 * The frequency is a parameter rather than a constant for two reasons. LS1 is an
 * externally driven piezo transducer: it makes no sound on DC, so something has
 * to generate a square wave and the only question is where. And a piezo element
 * is loudest at its resonance, which for the chosen part is 4 kHz, but the
 * number that matters is the one a person can actually walk toward in a field
 * rather than the peak on a bench. It comes from buzzer_freq_hz in the
 * configuration, which is unset until that is measured. */
oa_result_t oa_out_buzzer_set(uint16_t freq_hz);

/* Set the status LED colour. Eight bits per channel, 0,0,0 is off.
 *
 * D1 is a plain common cathode RGB LED, not an addressable one: three dice in a
 * package, each with its own anode on its own microcontroller pin through its
 * own series resistor, sharing a cathode to ground. So a pin high is that colour
 * lit and the three combine to eight colours.
 *
 * The signature is still eight bits per channel rather than three booleans,
 * because the interface should not force a decision the port is entitled to
 * make. A port that drives the three pins on and off treats any non-zero
 * channel as on and loses nothing this project currently asks for. A port that
 * puts them on PWM slices gets intermediate colours and dimming for free. Both
 * satisfy this function; neither is visible above it.
 *
 * TODO(confirm-on-hardware): whether dimming is worth having at all. An
 * indicator that is bright enough to read in direct sun is uncomfortable at
 * night, and which of those two a launch actually is has not been tested. */
oa_result_t oa_out_status_led_set(uint8_t r, uint8_t g, uint8_t b);

/* Silence the buzzer and blank the LED, in one call, and return the first
 * failure if either fails.
 *
 * This is the safe state, and it is what a fault handler and a watchdog reset
 * path call. It is a convenience over the two functions above and drives nothing
 * they do not. */
oa_result_t oa_out_all_off(void);

/* The name of an output, exactly as it appears in outputs.allowlist. Used by the
 * check that compares this enum against that file, and by the serial console.
 * Returns NULL for a value that is not an output, rather than a placeholder,
 * because a placeholder would let the allowlist check pass on a value that is
 * not really there. */
const char *oa_out_name(oa_out_t out);

#ifdef __cplusplus
}
#endif

#endif /* OAPOGEE_PORT_OA_OUT_H */

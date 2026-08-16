/**
 * oApogee payload, functional schematic.
 *
 * READ THIS BEFORE USING ANY PART OF IT
 *
 * This file is now a layout as well as a netlist. It routes, and `make fab`
 * exports a fabrication package from it, so the old instruction here not to
 * send it to a fabricator is gone along with the placeholder pin numbers that
 * justified it.
 *
 * What is verified: the microcontroller's sixty pins are the RP2350 datasheet's
 * own numbering, and each signal sits on a GPIO its peripheral can actually
 * reach.
 *
 * What is NOT verified, and is the reason to read a datasheet before spending
 * money: every other package uses the pin numbering its footprint library
 * assigns, and nothing in this repository compares that against the
 * manufacturer's land pattern. A wrong pin number is invisible in a schematic,
 * invisible in a render, and shows up as a board that does not work.
 *
 * One board, three tiers. Everything is on the same schematic because that is
 * the claim the project makes: Solo, Link and Track are the same PCB with
 * different footprints populated, and a schematic that showed three different
 * boards would quietly contradict it. Tier membership is in the comment above
 * each block and in data/system.yaml.
 *
 * Cross-checked against data/bom.yaml by tools/check-data.mjs, which reads the
 * generated netlist: every reference designator on this sheet is claimed by
 * exactly one part in the bill of materials and vice versa, asserted part
 * numbers must match, and the board outline below must agree with
 * data/mechanical.yaml, because the printed enclosure is built from it.
 */


/**
 * A quad-flat land pattern with corners that actually clear.
 *
 * tscircuit's generic qfn/lga footprints place the corner pad of one side so it
 * OVERLAPS the corner pad of the perpendicular side. Not touches: overlaps, by
 * about 0.14 mm, which is a short between two pins of the part. It affects
 * every quad package in the library and no combination of the pad-length and
 * pad-width parameters changes it, so the footprints are built here instead.
 *
 * Pins are numbered counter-clockwise from the top of the left side, which is
 * how the RP2350 datasheet numbers the QFN-60 and how quad packages are
 * numbered generally. The pads sit mostly outboard of the body edge, which is
 * what a real land pattern does and what leaves the corners clear.
 */
function quadPads(opts: {
  pins: number
  body: number
  pitch: number
  padLen: number
  padWid: number
  overhang?: number
}) {
  const { pins, body, pitch, padLen, padWid, overhang = 0.25 } = opts
  const perSide = pins / 4
  const half = body / 2
  const r = half - padLen / 2 + overhang
  const span = (perSide - 1) * pitch
  const out: React.ReactNode[] = []
  let n = 1
  for (const side of ['left', 'bottom', 'right', 'top'] as const) {
    for (let i = 0; i < perSide; i++) {
      const t = -span / 2 + i * pitch
      const horizontal = side === 'left' || side === 'right'
      const x = side === 'left' ? -r : side === 'right' ? r : side === 'bottom' ? t : -t
      const y = side === 'left' ? -t : side === 'right' ? t : side === 'bottom' ? -r : r
      out.push(
        <smtpad
          key={n}
          portHints={[String(n)]}
          pcbX={x}
          pcbY={y}
          width={`${horizontal ? padLen : padWid}mm`}
          height={`${horizontal ? padWid : padLen}mm`}
          shape="rect"
        />
      )
      n++
    }
  }
  return out
}


/**
 * A two-row land pattern, for the LGA parts whose pin counts are not multiples
 * of four and whose real pad layouts are not published in any form this
 * repository has read yet. Two rows have no corners, so they cannot reproduce
 * the overlap the generic footprints have. They are placeholders and
 * tools/check-pcb.mjs says so on every run: the package outline is right, the
 * pad positions are not the manufacturer's land pattern.
 */
function dualPads(opts: { pins: number; body: number; pitch: number; padLen: number; padWid: number }) {
  const { pins, body, pitch, padLen, padWid } = opts
  const perSide = Math.ceil(pins / 2)
  const span = (perSide - 1) * pitch
  const r = body / 2 - padLen / 2 + 0.2
  const out: React.ReactNode[] = []
  let n = 1
  for (const side of [-1, 1]) {
    for (let i = 0; i < perSide && n <= pins; i++) {
      const t = side < 0 ? -span / 2 + i * pitch : span / 2 - i * pitch
      out.push(
        <smtpad
          key={n}
          portHints={[String(n)]}
          pcbX={side * r}
          pcbY={t}
          width={`${padLen}mm`}
          height={`${padWid}mm`}
          shape="rect"
        />
      )
      n++
    }
  }
  return out
}

export default () => (
  <board width="28mm" height="78mm" minTraceWidth="0.127mm">
    {/* ---------------------------------------------------------------------
        Power. USB-C in, charger, cell, 3V3 rail.

        A single lithium cell runs from about 4.2 V down to about 3.0 V, which
        crosses 3.3 V partway through the discharge. A plain buck browns out at
        the bottom and a plain LDO wastes headroom at the top, so the rail is a
        buck-boost. Tiers: all.
        --------------------------------------------------------------------- */}

    <chip
      name="J1"
      footprint="usbcmidmount"
      pcbX={-0.4}
      pcbY={32.5}
      manufacturerPartNumber="USB4085-GF-A"
      pinLabels={{ pin1: 'VBUS', pin2: 'GND', pin3: 'DP', pin4: 'DM', pin5: 'CC1', pin6: 'CC2' }}
      schX={-20}
      schY={-8}
    />

    {/* USB-C sinks advertise their current draw with two 5.1k pulldowns, one
        per CC line. Without them a compliant source supplies nothing and the
        board looks dead on a good cable, which is indistinguishable from the
        much more common charge-only-cable fault. */}
    <resistor name="R1" resistance="5.1k" schX={-20} schY={-12} footprint="0402" pcbX={3.75} pcbY={25.0} />
    <resistor name="R2" resistance="5.1k" schX={-20} schY={-13.5} footprint="0402" pcbX={7.5} pcbY={25.0} />

    <chip
      name="U1"
      footprint="sot23_5"
      pcbX={-8.75}
      pcbY={23.75}
      manufacturerPartNumber="MCP73831"
      pinLabels={{ pin1: 'STAT', pin2: 'VSS', pin3: 'VBAT', pin4: 'VDD', pin5: 'PROG' }}
      schX={-15}
      schY={-8}
    />

    {/* Sets the charge current. The value is not chosen yet: it follows from
        the cell capacity, which follows from the endurance requirement, which
        follows from measured current draw. All three are open. */}
    <resistor name="R3" resistance="10k" schX={-15} schY={-11} footprint="0402" pcbX={-3.12} pcbY={24.38} />

    <chip
      name="J2"
      footprint="jst_ph_2"
      pcbX={-7.5}
      pcbY={16.25}
      manufacturerPartNumber="S2B-PH-K-S"
      pinLabels={{ pin1: 'VBAT', pin2: 'GND' }}
      schX={-15}
      schY={-14}
    />

    <chip
      name="U2"
      /* VSON-10, 3 by 3 mm, with the exposed pad tied to PGND as the datasheet
         requires. Pin numbering is the datasheet's own (SLVS520C, Pin
         Functions): 1 VOUT, 2 L2, 3 PGND, 4 L1, 5 VIN, 6 EN, 7 PS/SYNC,
         8 VINA, 9 GND, 10 FB. */
      footprint={
        <footprint>
          {dualPads({ pins: 10, body: 3, pitch: 0.5, padLen: 0.7, padWid: 0.28 })}
          <smtpad portHints={['11']} pcbX={0} pcbY={0} width="1.6mm" height="2.4mm" shape="rect" />
        </footprint>
      }
      pcbX={5.0}
      pcbY={18.75}
      manufacturerPartNumber="TPS63001"
      pinLabels={{
        pin1: 'VOUT',
        pin2: 'L2',
        pin3: 'PGND',
        pin4: 'L1',
        pin5: 'VIN',
        pin6: 'EN',
        pin7: 'PSSYNC',
        pin8: 'VINA',
        pin9: 'GND',
        pin10: 'FB',
        pin11: 'EPAD',
      }}
      schX={-10}
      schY={-8}
      schWidth={4}
      schHeight={6}
    />

    {/* The buck-boost's switching inductor. A buck-boost stores energy in this
        on every cycle, so without it the rail simply does not come up. The
        datasheet's typical application uses 2.2 uH. */}
    <inductor name="L2" inductance="2.2uH" footprint="0805" pcbX={10.5} pcbY={15.5} schX={-7} schY={-6} />

    {/* C1 is the input capacitor, C2 the output. They are different values
        because the datasheet asks for different values: SLVS520C section
        8.2.2.3 wants at least 4.7 uF in and a nominal 15 uF out. C2 is 22 uF
        rather than 15 uF because a small ceramic loses a large fraction of its
        marked capacitance under DC bias, so 15 uF marked is not 15 uF on the
        rail, and 22 uF is the next standard value with room for that. */}
    <capacitor name="C1" capacitance="10uF" schX={-12} schY={-11} footprint="0805" pcbX={-8.75} pcbY={11.25} />
    <capacitor name="C2" capacitance="22uF" schX={-8} schY={-11} footprint="0805" pcbX={-4.38} pcbY={11.25} />

    {/* ---------------------------------------------------------------------
        Compute. RP2350 and its QSPI flash.

        Drag-and-drop UF2 flashing is why this family was chosen: a first-time
        builder installs no toolchain to get a working payload, and that removes
        the most common place people give up. Tiers: all.
        --------------------------------------------------------------------- */}

    <chip
      name="U3"
      footprint={
        <footprint>
          {quadPads({ pins: 60, body: 7, pitch: 0.4, padLen: 0.75, padWid: 0.2 })}
          <smtpad portHints={['61']} pcbX={0} pcbY={0} width="3.2mm" height="3.2mm" shape="rect" />
        </footprint>
      }
      pcbX={0.0}
      pcbY={3.75}
      manufacturerPartNumber="RP2350A"
      pinLabels={{
        pin1: 'VDD',
        pin2: 'GPIO0',
        pin3: 'GPIO1',
        pin4: 'GPIO2',
        pin5: 'GPIO3',
        pin6: 'DVDD1',
        pin7: 'SDA',
        pin8: 'SCL',
        pin9: 'GPIO6',
        pin10: 'GPIO7',
        pin11: 'IOVDD2',
        pin12: 'GPIO8',
        pin13: 'LED_B',
        pin14: 'GPIO10',
        pin15: 'GPIO11',
        pin16: 'LED_G',
        pin17: 'ARM',
        pin18: 'BUZZER',
        pin19: 'LED_R',
        pin20: 'IOVDD3',
        pin21: 'XIN',
        pin22: 'XOUT',
        pin23: 'DVDD2',
        pin24: 'SWCLK',
        pin25: 'SWDIO',
        pin26: 'RUN',
        pin27: 'SPI_MISO',
        pin28: 'CS_IMU',
        pin29: 'SPI_SCK',
        pin30: 'IOVDD4',
        pin31: 'SPI_MOSI',
        pin32: 'CS_HIGHG',
        pin33: 'CS_RADIO',
        pin34: 'RADIO_BUSY',
        pin35: 'RADIO_DIO1',
        pin36: 'GNSS_TX',
        pin37: 'GNSS_RX',
        pin38: 'IOVDD5',
        pin39: 'DVDD3',
        pin40: 'VBAT_SENSE',
        pin41: 'GPIO27',
        pin42: 'GPIO28',
        pin43: 'GPIO29',
        pin44: 'ADC_AVDD',
        pin45: 'IOVDD6',
        pin46: 'VREG_AVDD',
        pin47: 'VREG_PGND',
        pin48: 'VREG_LX',
        pin49: 'VREG_VIN',
        pin50: 'VREG_FB',
        pin51: 'USB_DM',
        pin52: 'USB_DP',
        pin53: 'USB_OTP_VDD',
        pin54: 'QSPI_IOVDD',
        pin55: 'QSPI_D3',
        pin56: 'QSPI_SCK',
        pin57: 'QSPI_D0',
        pin58: 'QSPI_D2',
        pin59: 'QSPI_D1',
        pin60: 'QSPI_CS',
        pin61: 'GND',
      }}
      schX={0}
      schY={0}
      schWidth={4.4}
      schHeight={16}
      /* Pins grouped by what they do and placed on the side they leave towards,
         so the sheet reads the same way the block diagram does: power and the
         host connection on the left, sensors and outputs on the right. Left to
         the default the pins land in declaration order on two sides and every
         net crosses the part. */
      schPinArrangement={{
        leftSide: {
          direction: 'top-to-bottom',
          pins: [
            'VDD',
            'GND',
            'USB_DP',
            'USB_DM',
            'VBAT_SENSE',
            'RUN',
            'QSPI_SCK',
            'QSPI_CS',
            'QSPI_D0',
            'QSPI_D1',
            'QSPI_D2',
            'QSPI_D3',
          ],
        },
        rightSide: {
          direction: 'top-to-bottom',
          pins: [
            'SDA',
            'SCL',
            'SPI_SCK',
            'SPI_MOSI',
            'SPI_MISO',
            'CS_IMU',
            'CS_HIGHG',
            'CS_RADIO',
            'RADIO_BUSY',
            'RADIO_DIO1',
            'GNSS_TX',
            'GNSS_RX',
            'BUZZER',
            'LED_R',
            'LED_G',
            'LED_B',
            'ARM',
          ],
        },
      }}
    />

    <capacitor name="C3" capacitance="100nF" schX={-6} schY={-3} footprint="0402" pcbX={5.62} pcbY={14.38} />

    {/* Soldered down, deliberately. A microSD card is held in by a friction
        detent and boost acceleration is enough to unseat one, with the worst
        failure mode available: the flight proceeds normally and the data is
        gone. Tiers: all. */}
    <chip
      name="U4"
      footprint="soic8"
      pcbX={-6.88}
      pcbY={-6.25}
      manufacturerPartNumber="W25Q128JVSIQ"
      pinLabels={{
        pin1: 'CS',
        pin2: 'DO',
        pin3: 'WP',
        pin4: 'GND',
        pin5: 'DI',
        pin6: 'CLK',
        pin7: 'HOLD',
        pin8: 'VCC',
      }}
      schX={-7}
      schY={4}
    />

    {/* ---------------------------------------------------------------------
        Sensing.

        The barometer sits on I2C and the high rate parts sit on SPI, so the
        sample rate is not limited by the bus. Tiers: baro and IMU on all,
        high-g on Link and Track.
        --------------------------------------------------------------------- */}

    <chip
      name="U5"
      footprint={
        <footprint>
          {dualPads({ pins: 10, body: 2, pitch: 0.5, padLen: 0.45, padWid: 0.3 })}
        </footprint>
      }
      pcbX={-8.12}
      pcbY={-11.88}
      manufacturerPartNumber="BMP390"
      pinLabels={{ pin1: 'VDD', pin2: 'GND', pin3: 'SDA', pin4: 'SCL', pin5: 'SDO' }}
      schX={8}
      schY={7}
    />

    {/* One set of pull-ups on the bus, on the board. On the Modules path each
        breakout brings its own and several in parallel load the bus enough to
        stop it working, which presents as intermittent dropouts. */}
    <resistor name="R4" resistance="4.7k" schX={12} schY={9} footprint="0402" pcbX={-1.88} pcbY={-11.88} />
    <resistor name="R5" resistance="4.7k" schX={12} schY={10.5} footprint="0402" pcbX={1.25} pcbY={-11.88} />

    <chip
      name="U6"
      footprint={
        <footprint>
          {dualPads({ pins: 14, body: 2.5, pitch: 0.4, padLen: 0.5, padWid: 0.25 })}
        </footprint>
      }
      pcbX={6.88}
      pcbY={-11.88}
      manufacturerPartNumber="ICM-42688-P"
      pinLabels={{
        pin1: 'VDD',
        pin2: 'GND',
        pin3: 'SCLK',
        pin4: 'SDI',
        pin5: 'SDO',
        pin6: 'CS',
        pin7: 'INT1',
      }}
      schX={8}
      schY={3}
    />

    {/* A general purpose IMU tops out around 16 g and reports its maximum
        rather than an error, so boost above a C motor comes back as a flat
        plateau that looks like data. This part is why the boost phase means
        anything. Tiers: Link, Track. */}
    <chip
      name="U7"
      footprint={
        <footprint>
          {dualPads({ pins: 14, body: 3, pitch: 0.5, padLen: 0.6, padWid: 0.3 })}
        </footprint>
      }
      pcbX={-8.12}
      pcbY={-17.5}
      manufacturerPartNumber="ADXL375"
      pinLabels={{
        pin1: 'VDD',
        pin2: 'GND',
        pin3: 'SCLK',
        pin4: 'SDI',
        pin5: 'SDO',
        pin6: 'CS',
      }}
      schX={8}
      schY={-1}
    />

    {/* ---------------------------------------------------------------------
        Radio. Tiers: Link, Track.
        --------------------------------------------------------------------- */}

    <chip
      name="U8"
      footprint={
        <footprint>
          {quadPads({ pins: 24, body: 4, pitch: 0.5, padLen: 0.8, padWid: 0.25 })}
          <smtpad portHints={['25']} pcbX={0} pcbY={0} width="2.2mm" height="2.2mm" shape="rect" />
        </footprint>
      }
      pcbX={-4.5}
      pcbY={-23.0}
      manufacturerPartNumber="SX1262"
      pinLabels={{
        pin1: 'VDD',
        pin2: 'GND',
        pin3: 'SCK',
        pin4: 'MOSI',
        pin5: 'MISO',
        pin6: 'NSS',
        pin7: 'BUSY',
        pin8: 'DIO1',
        pin9: 'ANT',
      }}
      schX={8}
      schY={-5}
    />

    <chip
      name="J3"
      footprint="sma"
      pcbX={-1.25}
      pcbY={-35.25}
      manufacturerPartNumber="U.FL-R-SMT-1(10)"
      pinLabels={{ pin1: 'ANT', pin2: 'GND' }}
      schX={13}
      schY={-5}
    />

    {/* ---------------------------------------------------------------------
        Position. Tiers: Track.

        The receiver defaults to a dynamic platform model that assumes ground
        vehicle behaviour and rejects its own solutions under rocket
        acceleration. The airborne model has to be configured in firmware; no
        amount of correct wiring fixes it.
        --------------------------------------------------------------------- */}

    <chip
      name="U9"
      footprint={
        <footprint>
          {dualPads({ pins: 18, body: 4.5, pitch: 0.5, padLen: 0.8, padWid: 0.3 })}
        </footprint>
      }
      pcbX={-6.25}
      pcbY={-30.0}
      manufacturerPartNumber="MAX-M10S"
      pinLabels={{ pin1: 'VCC', pin2: 'GND', pin3: 'TXD', pin4: 'RXD', pin5: 'RF_IN' }}
      schX={8}
      schY={-9}
    />

    {/* ---------------------------------------------------------------------
        Recovery aids. Tiers: all.

        On a Solo build the buzzer is the only recovery aid there is, so it
        earns its mass.
        --------------------------------------------------------------------- */}

    <chip
      name="LS1"
      footprint="pinrow2"
      pcbX={8.12}
      pcbY={-18.75}
      manufacturerPartNumber="PKLCS1212E4001-R1"
      pinLabels={{ pin1: 'IN', pin2: 'GND' }}
      schX={8}
      schY={-13}
    />

    {/* Common cathode: three ordinary dice in one package, each with its own
        anode, sharing a cathode to ground. The microcontroller sources through
        a resistor per colour, so a pin high is that colour lit. */}
    <chip
      name="D1"
      footprint="led5050"
      pcbX={7.5}
      pcbY={-28.12}
      manufacturerPartNumber="RGB-LED-CC"
      pinLabels={{ pin1: 'A_R', pin2: 'A_G', pin3: 'A_B', pin4: 'K' }}
      schX={8}
      schY={-16}
    />


    {/* ---------------------------------------------------------------------
        Arming.

        The flight state machine's first transition is operator-driven, and the
        board previously had no way for an operator to drive it. Arming is what
        starts the pressure reference settling and the pre-arm ring buffer, so
        without an input there is no flight.

        The switch is on a GPIO rather than in the power path. oApogee is a
        passive payload, so cutting power arms nothing and protects nobody; what
        the operator needs is to power the payload up, let the GNSS get a fix and
        the barometer settle, and then arm it once the rocket is on the rail.
        Tiers: all.
        --------------------------------------------------------------------- */}

    <chip
      name="SW1"
      footprint="smdslideswitch"
      pcbX={8.5}
      pcbY={-5.0}
      manufacturerPartNumber="JS102011SCQN"
      pinLabels={{ pin1: 'A', pin2: 'B' }}
      schX={13}
      schY={-16}
    />

    {/* Pulled up, switch pulls down. A floating input reads as noise, and an
        input that reads as noise arms a rocket at random. */}
    <resistor name="R6" resistance="100k" schX={11} schY={-18} footprint="0402" pcbX={1.25} pcbY={-5.0} />

    {/* ---------------------------------------------------------------------
        Getting code onto the part, and getting it back out.

        This board had none of this. RUN, SWCLK and SWDIO were all unconnected
        and QSPI_CS went only to the flash, which means a fabricated board could
        be flashed exactly once, while its flash was still blank and the bootrom
        fell through to USB boot, and then never again: no way into the
        bootloader, no way to reset it, and no way to attach a debugger. For a
        payload that is meant to be reflashed between flights that is fatal, and
        it was invisible in the schematic because the pins were simply absent.

        BOOTSEL is not a dedicated pin on this part. Holding the flash chip
        select low while the chip comes out of reset is what selects USB boot,
        so the boot button pulls QSPI_CS down and the reset button pulls RUN
        down. Press and hold boot, tap reset, release boot.
        --------------------------------------------------------------------- */}

    <chip
      name="SW2"
      footprint="smdpushbutton"
      pcbX={-9.38}
      pcbY={0.62}
      manufacturerPartNumber="B3U-1000P"
      pinLabels={{ pin1: 'A1', pin2: 'A2', pin3: 'B1', pin4: 'B2' }}
      schX={-13}
      schY={-16}
    />
    <chip
      name="SW3"
      footprint="smdpushbutton"
      pcbX={9.38}
      pcbY={0.25}
      manufacturerPartNumber="B3U-1000P"
      pinLabels={{ pin1: 'A1', pin2: 'A2', pin3: 'B1', pin4: 'B2' }}
      schX={-13}
      schY={-19}
    />

    {/* RUN has an internal pull-up on this part, and an external one is the
        convention because it makes the reset behaviour independent of a
        datasheet detail somebody would otherwise have to look up. */}
    {/* One resistor per colour, because the three dice do not share a forward
        voltage. Red drops around two volts and the green and blue dice drop
        appreciably more, so a single shared resistor would give three different
        brightnesses with no way to correct one without changing the others. */}
    <resistor name="R10" resistance="330" schX={10} schY={-14} footprint="0402" pcbX={12.6} pcbY={-24.0} />
    <resistor name="R11" resistance="220" schX={10} schY={-15.5} footprint="0402" pcbX={12.6} pcbY={-26.5} />
    <resistor name="R12" resistance="220" schX={10} schY={-17} footprint="0402" pcbX={12.6} pcbY={-29.0} />

    <resistor name="R9" resistance="10k" schX={-15} schY={-17} footprint="0402" pcbX={11.88} pcbY={-12.0} />

    {/* Debug pads. Two pads and a ground are the difference between a board
        that can be single-stepped and one that can only be power-cycled. */}
    <chip
      name="J4"
      /* Three test pads at 1.27 mm rather than a 2.54 mm header. A header is
         8 mm long and there is nowhere on this board to put 8 mm; pads are what
         a debugger clips to anyway, and they cost no height under the pod. */
      footprint={
        <footprint>
          <smtpad portHints={['1']} pcbX={0} pcbY={1.27} width="0.7mm" height="0.7mm" shape="rect" />
          <smtpad portHints={['2']} pcbX={0} pcbY={0} width="0.7mm" height="0.7mm" shape="rect" />
          <smtpad portHints={['3']} pcbX={0} pcbY={-1.27} width="0.7mm" height="0.7mm" shape="rect" />
        </footprint>
      }
      pcbX={12.0}
      pcbY={-15.38}
      manufacturerPartNumber="SWD-PADS"
      pinLabels={{ pin1: 'SWCLK', pin2: 'SWDIO', pin3: 'GND' }}
      schX={13}
      schY={-20}
    />

    {/* A tactile switch has four pads in two internally joined pairs, and which
        pair is which is a property of the part rather than something this design
        should assume. Both pads of each side are tied. */}
    <trace from=".SW2 .A1" to=".U3 .QSPI_CS" />
    <trace from=".SW2 .A2" to=".U3 .QSPI_CS" />
    <trace from=".SW2 .B1" to="net.GND" />
    <trace from=".SW2 .B2" to="net.GND" />
    <trace from=".SW3 .A1" to=".U3 .RUN" />
    <trace from=".SW3 .A2" to=".U3 .RUN" />
    <trace from=".SW3 .B1" to="net.GND" />
    <trace from=".SW3 .B2" to="net.GND" />
    <trace from=".R9 .pin1" to="net.V3V3" />
    <trace from=".R9 .pin2" to=".U3 .RUN" />
    <trace from=".J4 .SWCLK" to=".U3 .SWCLK" />
    <trace from=".J4 .SWDIO" to=".U3 .SWDIO" />
    <trace from=".J4 .GND" to="net.GND" />

    {/* ---------------------------------------------------------------------
        Battery sense.

        The telemetry format transmits a battery voltage in every FLIGHT and
        STATUS packet, and the recovery beacon's endurance is the number that
        decides whether a payload is findable the next morning. Neither is
        measurable without a divider: a single cell reaches 4.2 V, which is above
        the 3V3 rail and above what the microcontroller's ADC will accept.
        Tiers: all.
        --------------------------------------------------------------------- */}

    <resistor name="R7" resistance="100k" schX={-14} schY={-17} footprint="0402" pcbX={-1.25} pcbY={-17.5} />
    <resistor name="R8" resistance="100k" schX={-14} schY={-19} footprint="0402" pcbX={1.88} pcbY={-17.5} />

    {/* ---------------------------------------------------------------------
        Nets.
        --------------------------------------------------------------------- */}

    {/* Power path: USB in, charge, cell, rail. */}
    <trace from=".J1 .VBUS" to=".U1 .VDD" />
    <trace from=".C2 .pin1" to="net.V3V3" />
    <trace from=".J1 .CC1" to=".R1 .pin1" />
    <trace from=".J1 .CC2" to=".R2 .pin1" />
    <trace from=".U1 .PROG" to=".R3 .pin1" />

    {/* USB data straight to the microcontroller. One connector does power,
        charging, configuration and offload. */}
    <trace from=".J1 .DP" to=".U3 .USB_DP" />
    <trace from=".J1 .DM" to=".U3 .USB_DM" />

    {/* QSPI flash. */}
    <trace from=".U3 .QSPI_SCK" to=".U4 .CLK" />
    <trace from=".U3 .QSPI_CS" to=".U4 .CS" />
    <trace from=".U3 .QSPI_D0" to=".U4 .DI" />
    <trace from=".U3 .QSPI_D1" to=".U4 .DO" />
    <trace from=".U3 .QSPI_D2" to=".U4 .WP" />
    <trace from=".U3 .QSPI_D3" to=".U4 .HOLD" />

    {/* I2C, barometer. */}
    <trace from=".U3 .SDA" to=".U5 .SDA" />
    <trace from=".U3 .SCL" to=".U5 .SCL" />
    <trace from=".U5 .SDA" to=".R4 .pin1" />
    <trace from=".U5 .SCL" to=".R5 .pin1" />

    {/* SPI bus, shared by the IMU, the high-g part and the radio, with a chip
        select each. */}
    <trace from=".U3 .SPI_SCK" to=".U6 .SCLK" />
    <trace from=".U3 .SPI_MOSI" to=".U6 .SDI" />
    <trace from=".U3 .SPI_MISO" to=".U6 .SDO" />
    <trace from=".U3 .CS_IMU" to=".U6 .CS" />

    <trace from=".U3 .SPI_SCK" to=".U7 .SCLK" />
    <trace from=".U3 .SPI_MOSI" to=".U7 .SDI" />
    <trace from=".U3 .SPI_MISO" to=".U7 .SDO" />
    <trace from=".U3 .CS_HIGHG" to=".U7 .CS" />

    <trace from=".U3 .SPI_SCK" to=".U8 .SCK" />
    <trace from=".U3 .SPI_MOSI" to=".U8 .MOSI" />
    <trace from=".U3 .SPI_MISO" to=".U8 .MISO" />
    <trace from=".U3 .CS_RADIO" to=".U8 .NSS" />
    <trace from=".U3 .RADIO_BUSY" to=".U8 .BUSY" />
    <trace from=".U3 .RADIO_DIO1" to=".U8 .DIO1" />
    <trace from=".U8 .ANT" to=".J3 .ANT" />

    {/* GNSS on a UART. Crossed, because TX on one end is RX on the other, and
        this is the single most common wiring mistake on a serial link. */}
    <trace from=".U3 .GNSS_TX" to=".U9 .RXD" />
    <trace from=".U3 .GNSS_RX" to=".U9 .TXD" />

    {/* Recovery aids. */}
    <trace from=".U3 .BUZZER" to=".LS1 .IN" />
    <trace from=".U3 .LED_R" to=".R10 .pin1" />
    <trace from=".R10 .pin2" to=".D1 .A_R" />
    <trace from=".U3 .LED_G" to=".R11 .pin1" />
    <trace from=".R11 .pin2" to=".D1 .A_G" />
    <trace from=".U3 .LED_B" to=".R12 .pin1" />
    <trace from=".R12 .pin2" to=".D1 .A_B" />

    {/* --- power and ground -------------------------------------------------
        Every part's supply and return. These were missing entirely, which meant
        the pull-ups had nothing to pull towards and the decoupling had nothing
        to decouple against. A netlist without them describes a board that cannot
        work.
        --------------------------------------------------------------------- */}

    <trace from=".J1 .GND" to="net.GND" />
    <trace from=".R1 .pin2" to="net.GND" />
    <trace from=".R2 .pin2" to="net.GND" />
    <trace from=".U1 .VSS" to="net.GND" />
    <trace from=".R3 .pin2" to="net.GND" />
    <trace from=".J2 .GND" to="net.GND" />
    <trace from=".U2 .GND" to="net.GND" />
    <trace from=".C1 .pin2" to="net.GND" />
    <trace from=".C2 .pin2" to="net.GND" />
    <trace from=".U3 .GND" to="net.GND" />
    <trace from=".C3 .pin2" to="net.GND" />
    <trace from=".U4 .GND" to="net.GND" />
    <trace from=".U5 .GND" to="net.GND" />
    <trace from=".U6 .GND" to="net.GND" />
    <trace from=".U7 .GND" to="net.GND" />
    <trace from=".U8 .GND" to="net.GND" />
    <trace from=".J3 .GND" to="net.GND" />
    <trace from=".U9 .GND" to="net.GND" />
    <trace from=".LS1 .GND" to="net.GND" />
    <trace from=".D1 .K" to="net.GND" />
    <trace from=".SW1 .B" to="net.GND" />
    <trace from=".R8 .pin2" to="net.GND" />

    {/* The barometer's address select pin is tied low rather than left to
        float, so the part answers at a known address instead of an arbitrary
        one. */}
    <trace from=".U5 .SDO" to="net.GND" />

    <trace from=".U2 .VOUT" to="net.V3V3" />
    <trace from=".U3 .VDD" to="net.V3V3" />
    <trace from=".C3 .pin1" to="net.V3V3" />
    <trace from=".U4 .VCC" to="net.V3V3" />
    <trace from=".U5 .VDD" to="net.V3V3" />
    <trace from=".R4 .pin2" to="net.V3V3" />
    <trace from=".R5 .pin2" to="net.V3V3" />
    <trace from=".U6 .VDD" to="net.V3V3" />
    <trace from=".U7 .VDD" to="net.V3V3" />
    <trace from=".U8 .VDD" to="net.V3V3" />
    <trace from=".U9 .VCC" to="net.V3V3" />
    <trace from=".R6 .pin2" to="net.V3V3" />

    {/* The regulator runs whenever the cell is connected. There is no soft power
        control, because a payload that can switch itself off is a payload that
        can stop beaconing while you are still looking for it. */}
    <trace from=".U2 .EN" to="net.VBAT" />
    <trace from=".U2 .VINA" to="net.VBAT" />
    <trace from=".U2 .L1" to=".L2 .pin1" />
    <trace from=".U2 .L2" to=".L2 .pin2" />
    {/* Fixed-output parts tie FB to VOUT: the datasheet says "must be connected
        to VOUT on fixed output voltage versions". */}
    <trace from=".U2 .FB" to="net.V3V3" />
    {/* Power-save mode enabled, which is the 0 case. A payload that sits armed
        on a pad spends most of its life at almost no load, and that is exactly
        where a converter held in fixed-frequency PWM wastes the cell. */}
    <trace from=".U2 .PSSYNC" to="net.GND" />
    <trace from=".U2 .PGND" to="net.GND" />
    <trace from=".U2 .EPAD" to="net.GND" />
    <trace from=".U1 .VBAT" to="net.VBAT" />
    <trace from=".J2 .VBAT" to="net.VBAT" />
    <trace from=".U2 .VIN" to="net.VBAT" />
    <trace from=".C1 .pin1" to="net.VBAT" />
    <trace from=".R7 .pin1" to="net.VBAT" />

    {/* Arming and battery sense into the microcontroller. */}
    <trace from=".SW1 .A" to=".U3 .ARM" />
    <trace from=".R6 .pin1" to=".U3 .ARM" />
    <trace from=".R7 .pin2" to=".U3 .VBAT_SENSE" />
    <trace from=".R8 .pin1" to=".U3 .VBAT_SENSE" />
    {/* The RP2350's core supply is an internal buck, and a buck needs an
        external inductor. VREG_LX switches into L1, the far side of L1 is the
        1.1 V core rail feeding all three DVDD pins, and VREG_FB senses it.
        This part did not exist while U3's pinout was a placeholder, which is
        exactly the kind of gap a placeholder hides: the schematic looked
        complete because the pins it needed were not on it. */}
    <inductor name="L1" inductance="3.3uH" footprint="0805" pcbX={7.5} pcbY={9.38} schX={4} schY={-6} />

    {/* Core rail. */}
    <trace from=".U3 .VREG_LX" to=".L1 .pin1" />
    <trace from=".L1 .pin2" to="net.VCORE" />
    <trace from=".U3 .VREG_FB" to="net.VCORE" />
    <trace from=".U3 .DVDD1" to="net.VCORE" />
    <trace from=".U3 .DVDD2" to="net.VCORE" />
    <trace from=".U3 .DVDD3" to="net.VCORE" />

    {/* Every supply pin the package actually has, not just the one a
        placeholder named. */}
    <trace from=".U3 .IOVDD2" to="net.V3V3" />
    <trace from=".U3 .IOVDD3" to="net.V3V3" />
    <trace from=".U3 .IOVDD4" to="net.V3V3" />
    <trace from=".U3 .IOVDD5" to="net.V3V3" />
    <trace from=".U3 .IOVDD6" to="net.V3V3" />
    <trace from=".U3 .ADC_AVDD" to="net.V3V3" />
    <trace from=".U3 .QSPI_IOVDD" to="net.V3V3" />
    <trace from=".U3 .USB_OTP_VDD" to="net.V3V3" />
    <trace from=".U3 .VREG_AVDD" to="net.V3V3" />
    <trace from=".U3 .VREG_VIN" to="net.V3V3" />
    <trace from=".U3 .VREG_PGND" to="net.GND" />

  </board>

)

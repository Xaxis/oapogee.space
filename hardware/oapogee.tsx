/**
 * oApogee payload, functional schematic.
 *
 * READ THIS BEFORE USING ANY PART OF IT
 *
 * This is a netlist, not a pinout. It says what connects to what, which is the
 * design decision worth capturing and reviewing. It does NOT say which physical
 * pin of any package a signal lands on, and the pin numbers below are
 * structural placeholders assigned in the order the labels are written, not
 * read from a datasheet.
 *
 * That distinction is the whole reason this file exists in this form. This
 * project's rule is that it never publishes a number it has not measured or
 * sourced, and a datasheet pin number recalled from memory is exactly the kind
 * of confident, plausible, wrong figure the rule exists to prevent. Mapping
 * these functional pins onto real packages is a separate step, done against
 * datasheets, and it is tracked as such.
 *
 * So: the connectivity here is reviewable and is meant to be reviewed. The pin
 * numbers are not. Do not lay a board out from this file and do not send it to
 * a fabricator.
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
  <board width="22mm" height="60mm" minTraceWidth="0.127mm">
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
      pcbX={0}
      pcbY={26}
      manufacturerPartNumber="USB-C-16P"
      pinLabels={{ pin1: 'VBUS', pin2: 'GND', pin3: 'DP', pin4: 'DM', pin5: 'CC1', pin6: 'CC2' }}
      schX={-20}
      schY={-8}
    />

    {/* USB-C sinks advertise their current draw with two 5.1k pulldowns, one
        per CC line. Without them a compliant source supplies nothing and the
        board looks dead on a good cable, which is indistinguishable from the
        much more common charge-only-cable fault. */}
    <resistor name="R1" resistance="5.1k" schX={-20} schY={-12} footprint="0402" pcbX={3} pcbY={20} />
    <resistor name="R2" resistance="5.1k" schX={-20} schY={-13.5} footprint="0402" pcbX={6} pcbY={20} />

    <chip
      name="U1"
      footprint="sot23_5"
      pcbX={-7}
      pcbY={19}
      manufacturerPartNumber="MCP73831"
      pinLabels={{ pin1: 'STAT', pin2: 'VSS', pin3: 'VBAT', pin4: 'VDD', pin5: 'PROG' }}
      schX={-15}
      schY={-8}
    />

    {/* Sets the charge current. The value is not chosen yet: it follows from
        the cell capacity, which follows from the endurance requirement, which
        follows from measured current draw. All three are open. */}
    <resistor name="R3" resistance="10k" schX={-15} schY={-11} footprint="0402" pcbX={-2.5} pcbY={19.5} />

    <chip
      name="J2"
      footprint="jst_ph_2"
      pcbX={-6}
      pcbY={13}
      manufacturerPartNumber="S2B-PH-K-S"
      pinLabels={{ pin1: 'VBAT', pin2: 'GND' }}
      schX={-15}
      schY={-14}
    />

    <chip
      name="U2"
      footprint="sot23_5"
      pcbX={4}
      pcbY={15}
      manufacturerPartNumber="TODO-BUCK-BOOST"
      pinLabels={{ pin1: 'VIN', pin2: 'GND', pin3: 'EN', pin4: 'VOUT' }}
      schX={-10}
      schY={-8}
    />

    <capacitor name="C1" capacitance="10uF" schX={-12} schY={-11} footprint="0805" pcbX={-7} pcbY={9} />
    <capacitor name="C2" capacitance="10uF" schX={-8} schY={-11} footprint="0805" pcbX={-3.5} pcbY={9} />

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
      pcbX={0}
      pcbY={3}
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
        pin13: 'GPIO9',
        pin14: 'GPIO10',
        pin15: 'GPIO11',
        pin16: 'GPIO12',
        pin17: 'ARM',
        pin18: 'BUZZER',
        pin19: 'LED',
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
            'LED',
            'ARM',
          ],
        },
      }}
    />

    <capacitor name="C3" capacitance="100nF" schX={-6} schY={-3} footprint="0402" pcbX={4.5} pcbY={11.5} />

    {/* Soldered down, deliberately. A microSD card is held in by a friction
        detent and boost acceleration is enough to unseat one, with the worst
        failure mode available: the flight proceeds normally and the data is
        gone. Tiers: all. */}
    <chip
      name="U4"
      footprint="soic8"
      pcbX={-5.5}
      pcbY={-5}
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
      pcbX={-6.5}
      pcbY={-9.5}
      manufacturerPartNumber="BMP390"
      pinLabels={{ pin1: 'VDD', pin2: 'GND', pin3: 'SDA', pin4: 'SCL', pin5: 'SDO' }}
      schX={8}
      schY={7}
    />

    {/* One set of pull-ups on the bus, on the board. On the Modules path each
        breakout brings its own and several in parallel load the bus enough to
        stop it working, which presents as intermittent dropouts. */}
    <resistor name="R4" resistance="4.7k" schX={12} schY={9} footprint="0402" pcbX={-1.5} pcbY={-9.5} />
    <resistor name="R5" resistance="4.7k" schX={12} schY={10.5} footprint="0402" pcbX={1} pcbY={-9.5} />

    <chip
      name="U6"
      footprint={
        <footprint>
          {dualPads({ pins: 14, body: 2.5, pitch: 0.4, padLen: 0.5, padWid: 0.25 })}
        </footprint>
      }
      pcbX={5.5}
      pcbY={-9.5}
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
      pcbX={-6.5}
      pcbY={-14}
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
      pcbX={-3.6}
      pcbY={-18.4}
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
      pcbX={-1}
      pcbY={-28.2}
      manufacturerPartNumber="ANT-902-928"
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
      pcbX={-5}
      pcbY={-24.0}
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
      pcbX={6.5}
      pcbY={-15.0}
      manufacturerPartNumber="PIEZO-BUZZER"
      pinLabels={{ pin1: 'IN', pin2: 'GND' }}
      schX={8}
      schY={-13}
    />

    <chip
      name="D1"
      footprint="led5050"
      pcbX={6}
      pcbY={-22.5}
      manufacturerPartNumber="RGB-LED"
      pinLabels={{ pin1: 'DIN', pin2: 'VDD', pin3: 'GND', pin4: 'DOUT' }}
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
      pcbX={6.8}
      pcbY={-4}
      manufacturerPartNumber="ARM-SWITCH"
      pinLabels={{ pin1: 'A', pin2: 'B' }}
      schX={13}
      schY={-16}
    />

    {/* Pulled up, switch pulls down. A floating input reads as noise, and an
        input that reads as noise arms a rocket at random. */}
    <resistor name="R6" resistance="100k" schX={11} schY={-18} footprint="0402" pcbX={1.0} pcbY={-4} />

    {/* ---------------------------------------------------------------------
        Battery sense.

        The telemetry format transmits a battery voltage in every FLIGHT and
        STATUS packet, and the recovery beacon's endurance is the number that
        decides whether a payload is findable the next morning. Neither is
        measurable without a divider: a single cell reaches 4.2 V, which is above
        the 3V3 rail and above what the microcontroller's ADC will accept.
        Tiers: all.
        --------------------------------------------------------------------- */}

    <resistor name="R7" resistance="100k" schX={-14} schY={-17} footprint="0402" pcbX={-1} pcbY={-14} />
    <resistor name="R8" resistance="100k" schX={-14} schY={-19} footprint="0402" pcbX={1.5} pcbY={-14} />

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
    <trace from=".U3 .LED" to=".D1 .DIN" />

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
    <trace from=".D1 .GND" to="net.GND" />
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
    <trace from=".D1 .VDD" to="net.V3V3" />
    <trace from=".R6 .pin2" to="net.V3V3" />

    {/* The regulator runs whenever the cell is connected. There is no soft power
        control, because a payload that can switch itself off is a payload that
        can stop beaconing while you are still looking for it. */}
    <trace from=".U2 .EN" to="net.VBAT" />
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
    <inductor name="L1" inductance="3.3uH" footprint="0805" pcbX={6} pcbY={7.5} schX={4} schY={-6} />

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

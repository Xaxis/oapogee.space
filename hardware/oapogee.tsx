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

/**
 * A u.FL land pattern, for both coaxial sockets.
 *
 * It is here rather than named as a string because tscircuit has no "ufl"
 * footprint, and asking for one is not an error that stops anything: the export
 * logs an invalid-property message, exits zero, and produces a component with
 * no pads at all. That is worse than a wrong footprint. A part with no pads
 * cannot be routed, so its net silently has no copper, and the only evidence is
 * one line in a JSON file nobody reads. tools/check-pcb.mjs now treats an
 * invalid property as a blocker for exactly this reason.
 *
 * Signal pad forward, two ground pads behind it, which is the shape every u.FL
 * land pattern has. TODO(confirm-on-hardware): the dimensions are approximated,
 * as everywhere else on this board. Take them from the Hirose drawing before
 * ordering.
 */
function uflPads() {
  return [
    <smtpad key="1" portHints={['1']} pcbX={0} pcbY={-1.15} width="0.7mm" height="0.8mm" shape="rect" />,
    <smtpad key="2" portHints={['2']} pcbX={-1.15} pcbY={0.45} width="1.0mm" height="1.6mm" shape="rect" />,
    <smtpad key="3" portHints={['3']} pcbX={1.15} pcbY={0.45} width="1.0mm" height="1.6mm" shape="rect" />,
  ]
}

export default () => (
  <board width="28mm" height="92mm" minTraceWidth="0.127mm">
    {/* ---------------------------------------------------------------------
        Power. USB-C in, charger, cell, 3V3 rail.

        A single lithium cell runs from about 4.2 V down to about 3.0 V, which
        crosses 3.3 V partway through the discharge. A plain buck browns out at
        the bottom and a plain LDO wastes headroom at the top, so the rail is a
        buck-boost. Tiers: all.
        --------------------------------------------------------------------- */}

    <chip
      name="J1"
      /* GCT USB4085 product drawing, revision B, sheet 1/3: the pin table and
         the recommended PCB layout. Sixteen round plated holes in two rows of
         eight on 0.85 mm pitch, rows 1.35 mm apart, plus four oblong slots for
         the shell stakes. There is no hole for the eight nominal Type-C
         positions this part omits, which are the SuperSpeed pairs.

         THE THING THAT WOULD HAVE RUINED THIS BOARD is the row ordering. The
         two rows run in OPPOSITE directions, so the pins do not pair up
         vertically. Left to right, row A is A1 A4 A5 A6 A7 A8 A9 A12 and row B
         is B12 B9 B8 B7 B6 B5 B4 B1. Column 4 is therefore Dp1 sitting directly
         above Dn2, and column 5 is Dn1 above Dp2.

         So bridging straight down a column, which is the obvious move and
         exactly what the ground and VBUS columns invite, shorts USB data plus to
         data minus. The correct connections are the two diagonals and they
         cross. Columns 3 and 6 have the same trap with a configuration channel
         above a sideband pin. Only columns 1, 2, 7 and 8 are same-signal pairs.

         TODO(confirm-on-hardware): the four shell stakes are slots rather than
         round holes, dimensioned 0.60 and 0.90 across with different lengths
         top and bottom, and their exact positions are approximated here. Take
         them from the GCT drawing before ordering. The sixteen signal holes are
         the drawing's own geometry. */
      footprint={
        <footprint>
          {(() => {
            const PITCH = 0.85
            const ROW = 1.35
            const A = ['A1', 'A4', 'A5', 'A6', 'A7', 'A8', 'A9', 'A12']
            const B = ['B12', 'B9', 'B8', 'B7', 'B6', 'B5', 'B4', 'B1']
            const out: React.ReactNode[] = []
            for (const [row, names, y] of [
              ['A', A, ROW / 2],
              ['B', B, -ROW / 2],
            ] as const) {
              names.forEach((name, i) => {
                out.push(
                  <platedhole
                    key={name}
                    portHints={[name]}
                    pcbX={(i - (names.length - 1) / 2) * PITCH}
                    pcbY={y}
                    holeDiameter="0.35mm"
                    outerDiameter="0.65mm"
                    shape="circle"
                  />
                )
              })
            }
            for (const [i, x] of [-3.7, 3.7].entries()) {
              for (const [j, y] of [ROW / 2, -ROW / 2].entries()) {
                out.push(
                  <platedhole
                    key={`SHELL${i}${j}`}
                    portHints={[`SHELL${i}${j}`]}
                    pcbX={x}
                    pcbY={y}
                    holeDiameter="0.6mm"
                    outerDiameter="1.0mm"
                    shape="circle"
                  />
                )
              }
            }
            return out
          })()}
        </footprint>
      }
      pcbX={-0.4}
      pcbY={32.5}
      manufacturerPartNumber="USB4085-GF-A"

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
        pin41: 'RADIO_NRESET',
        pin42: 'RADIO_DIO2',
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

    {/* The crystal, and the reason it is not optional.
        ------------------------------------------------------------------
        This board previously declared XIN and XOUT and connected neither, on
        the reasoning that the RP2350 has an internal oscillator and will run
        without one. It does, and it will, and the board would still have been
        unflashable: "Hardware design with RP2350" chapter 4 says the internal
        oscillator's frequency is not well defined or controlled, varying by
        chip, supply and temperature, and that applications relying on exact
        frequencies are not possible without an external source, USB being
        their named example. USB is the only connector on this board and UF2
        drag-and-drop is the whole reason this microcontroller family was
        chosen, so a missing crystal is a dead board rather than a degraded
        one.

        Every value here is Raspberry Pi's, not this project's. The crystal is
        the ABM8-272-T3 they specify and the Pico 2 uses, 12 MHz, 30 ppm, 10 pF
        load, 50 ohm maximum ESR. C4 and C5 are 15 pF: in series they present
        7.5 pF, and the guide adds an assumed 3 pF of track and pin parasitic
        capacitance to reach 10.5 pF against the crystal's 10 pF target. R13 is
        the 1k series resistor that keeps a 50 ohm ESR crystal from being
        overdriven at an IOVDD of 3.3 V.

        Two consequences for layout, both from the same chapter: the parasitic
        capacitance of the tracks is part of the sum above, so XIN and XOUT
        must be kept short, and the whole circuit is tuned for 3.3 V IOVDD. It
        is the one part of this board where copying the reference design
        exactly is the correct engineering. */}
    <chip
      name="Y1"
      /* ABM8, 3.2 by 2.5 mm. Pads 1 and 3 are the crystal terminals, 2 and 4
         are the case, which is grounded. TODO(confirm-on-hardware): the pad
         geometry is approximated, as everywhere else on this board. Take it
         from the Abracon drawing before ordering. */
      footprint={
        <footprint>
          <smtpad portHints={['1']} pcbX={-1.1} pcbY={-0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['2']} pcbX={1.1} pcbY={-0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['3']} pcbX={1.1} pcbY={0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['4']} pcbX={-1.1} pcbY={0.85} width="1.2mm" height="1.0mm" shape="rect" />
        </footprint>
      }
      pcbX={-1.0}
      pcbY={-2.5}
      manufacturerPartNumber="ABM8-272-T3"
      pinLabels={{ pin1: 'XA', pin2: 'CASE1', pin3: 'XB', pin4: 'CASE2' }}
      schX={-4}
      schY={4}
    />

    <capacitor name="C4" capacitance="15pF" schX={-6} schY={3} footprint="0402" pcbX={-4.5} pcbY={-2.5} />
    {/* The barometer's two supply decoupling capacitors. The BMP390 datasheet
        gives one value for both, under Figure 24 and Figure 25: "the
        recommended value for C1, C2 is 100 nF". They are separate parts because
        the part has separate supplies, an analog VDD and a digital VDDIO, and
        one capacitor shared between them decouples neither properly. */}
    {/* The IMU's three bypass capacitors, all from the datasheet's own bill of
        materials in Table 11. Two on VDD, 0.1 uF and 2.2 uF, listed as both and
        not either: the small one handles the fast edges and the large one the
        bulk. One on VDDIO, and it is 10 nF, not the 100 nF that every other
        digital supply on this board gets, which is the sort of difference that
        gets quietly normalised by somebody tidying a schematic. */}
    {/* The high-g accelerometer's two decoupling capacitors, from the
        ADXL375 Power Supply Decoupling section: "A 1 uF tantalum capacitor (CS)
        at VS and a 0.1 uF ceramic capacitor (CI/O) at VDD I/O placed close to
        the ADXL375 supply pins are recommended".

        TODO(confirm-on-hardware): the datasheet recommends 1 uF on VS but its
        specification table was characterised with 10 uF. Those are different
        claims and the difference is not addressed anywhere in the document, so
        a part fitted with 1 uF may not meet the published noise figures.
        Measure the noise floor both ways before deciding which number this
        project publishes. */}
    <capacitor name="C11" capacitance="100nF" footprint="0402" pcbX={-11.6} pcbY={-16.2} schX={6} schY={-16} />
    <capacitor name="C12" capacitance="1uF" footprint="0603" pcbX={-11.6} pcbY={-19.0} schX={6} schY={-17.5} />

    <capacitor name="C8" capacitance="100nF" footprint="0402" pcbX={10.2} pcbY={-9.6} schX={10} schY={-10} />
    <capacitor name="C9" capacitance="2.2uF" footprint="0603" pcbX={10.2} pcbY={-14.2} schX={10} schY={-11.5} />
    <capacitor name="C10" capacitance="10nF" footprint="0402" pcbX={3.6} pcbY={-11.9} schX={10} schY={-13} />

    <capacitor name="C6" capacitance="100nF" footprint="0402" pcbX={-11.5} pcbY={-10.5} schX={6} schY={-10} />
    <capacitor name="C7" capacitance="100nF" footprint="0402" pcbX={-11.5} pcbY={-13.2} schX={6} schY={-11.5} />

    <capacitor name="C5" capacitance="15pF" schX={-2} schY={3} footprint="0402" pcbX={2.5} pcbY={-2.5} />
    <resistor name="R13" resistance="1k" schX={-2} schY={5} footprint="0402" pcbX={-1.0} pcbY={-4.5} />

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
      /* Bosch BMP390 datasheet, Table 52 and Figure 25 (I2C connection).
         Transcribed rather than assumed, and the assumption it replaces was
         fatal: this part previously had SDA on pin 3 and SCL on pin 4, and on
         the real part pin 3 is VSS. The board would have shorted the I2C data
         line to ground and the bus would have been dead with both sensors on
         it, not just this one. */
      pinLabels={{
        pin1: 'VDDIO',
        pin2: 'SCL',
        pin3: 'VSS3',
        pin4: 'SDA',
        pin5: 'SDO',
        pin6: 'CSB',
        pin7: 'INT',
        pin8: 'VSS8',
        pin9: 'VSS9',
        pin10: 'VDD',
      }}
      schX={8}
      schY={7}
    />

    {/* One set of pull-ups on the bus, on the board. On the Modules path each
        breakout brings its own and several in parallel load the bus enough to
        stop it working, which presents as intermittent dropouts. */}
    <resistor name="R4" resistance="4.7k" schX={12} schY={9} footprint="0402" pcbX={-5.0} pcbY={-10.5} />
    <resistor name="R5" resistance="4.7k" schX={12} schY={10.5} footprint="0402" pcbX={-5.0} pcbY={-13.2} />

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
      /* TDK InvenSense ICM-42688-P datasheet, Table 10, and the 4-wire SPI
         application schematic. Transcribed rather than assumed, and again the
         assumption was fatal: this part previously had VDD on pin 1, and pin 1
         is AP_SDO, the part's own data output. The board would have connected a
         3.3 V rail straight to an output driver.

         Four of the fourteen pins are reserved. The datasheet distinguishes
         between them and the distinction is not cosmetic: pins 2, 3, 10 and 11
         say "No Connect or Connect to GND", while pin 7 says "Connect to GND"
         with no alternative. They are grounded here because a defined pin is
         easier to inspect than a floating one, and because it is one of the two
         readings the datasheet permits for the optional four. */
      pinLabels={{
        pin1: 'SDO',
        pin2: 'RESV2',
        pin3: 'RESV3',
        pin4: 'INT1',
        pin5: 'VDDIO',
        pin6: 'GND',
        pin7: 'RESV7',
        pin8: 'VDD',
        pin9: 'INT2',
        pin10: 'RESV10',
        pin11: 'RESV11',
        pin12: 'CS',
        pin13: 'SCLK',
        pin14: 'SDI',
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
      /* Analog Devices ADXL375 datasheet, Table 5. Transcribed rather than
         assumed. The two reserved pins are worth reading twice, because they
         are opposites and a schematic that treats them alike is wrong on one of
         them: pin 3 "must be connected to VS or left open", pin 11 "must be
         connected to ground or left open". Not interchangeable.

         The previous guess had the SPI clock on pin 3, which is one of those
         two reserved pins. */
      pinLabels={{
        pin1: 'VDDIO',
        pin2: 'GND2',
        pin3: 'RESV3',
        pin4: 'GND4',
        pin5: 'GND5',
        pin6: 'VS',
        pin7: 'CS',
        pin8: 'INT1',
        pin9: 'INT2',
        pin10: 'NC',
        pin11: 'RESV11',
        pin12: 'SDO',
        pin13: 'SDI',
        pin14: 'SCLK',
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
      pcbX={-6.0}
      pcbY={-27.0}
      manufacturerPartNumber="SX1262"
      /* Semtech DS.SX1261-2.W.APP Table 2-1, QFN 4x4 24L. Identical in
         revision 1.2 (2019) and revision 2.2 (2024), checked against both and
         against the package top view in Figure 2-1. Note the datasheet
         designates the exposed pad 0 in Table 2-1 and 25 in the schematic
         symbol of Figure 14-2; it is one pad, and it is ground. */
      pinLabels={{
        pin1: 'VDD_IN',
        pin2: 'GND2',
        pin3: 'XTA',
        pin4: 'XTB',
        pin5: 'GND5',
        pin6: 'DIO3',
        pin7: 'VREG',
        pin8: 'GND8',
        pin9: 'DCC_SW',
        pin10: 'VBAT',
        pin11: 'VBAT_IO',
        pin12: 'DIO2',
        pin13: 'DIO1',
        pin14: 'BUSY',
        pin15: 'NRESET',
        pin16: 'MISO',
        pin17: 'MOSI',
        pin18: 'SCK',
        pin19: 'NSS',
        pin20: 'GND20',
        pin21: 'RFI_P',
        pin22: 'RFI_N',
        pin23: 'RFO',
        pin24: 'VR_PA',
        pin25: 'EPAD',
      }}
      schX={8}
      schY={-5}
    />

    {/* The radio's own 32 MHz reference, and the thing about it that is easy to
        get wrong.
        ------------------------------------------------------------------
        Semtech DS.SX1261-2.W.APP section 4.1.3: the SX1262 "does not require
        the user to set external foot capacitors on the XTAL supplying the
        32MHz clock", because it carries programmable capacitors on both XTA
        and XTB, trimmed in 0.47 pF steps and impossible to switch off. The
        state machine writes 19.7 pF into both on entering STDBY_XOSC. So this
        crystal has no load capacitors beside it, and adding the pair that
        every other crystal circuit wants would detune it.

        The crystal itself is specified by the datasheet rather than chosen:
        Table 3-4 asks for 32 MHz at 10 pF nominal load with an ESR of 30 ohm
        typical and 60 ohm maximum, and a drive level of at most 100 microwatt.

        A TCXO is the alternative and this design does not use one. It would go
        on XTA through a 220 ohm resistor and a 10 pF DC block with XTB left
        open, powered from DIO3, and it costs more and draws more. A crystal is
        accurate enough for LoRa. */}
    <chip
      name="Y2"
      footprint={
        <footprint>
          <smtpad portHints={['1']} pcbX={-1.1} pcbY={-0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['2']} pcbX={1.1} pcbY={-0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['3']} pcbX={1.1} pcbY={0.85} width="1.2mm" height="1.0mm" shape="rect" />
          <smtpad portHints={['4']} pcbX={-1.1} pcbY={0.85} width="1.2mm" height="1.0mm" shape="rect" />
        </footprint>
      }
      pcbX={-6.0}
      pcbY={-22.0}
      manufacturerPartNumber="XTAL-32M-10PF"
      pinLabels={{ pin1: 'XA', pin2: 'CASE1', pin3: 'XB', pin4: 'CASE2' }}
      schX={4}
      schY={-2}
    />

    {/* The DC-DC inductor, between VREG and DCC_SW. Section 5.1 of the same
        datasheet: running the radio on its LDO alone "negates the need for the
        15 uH inductor between pins 7 and 9", and section 13.1.11 says what that
        costs, which is that "the RX or TX current is almost doubled". On a
        payload sized around a single cell, doubling the transmit current to
        save one inductor is the wrong trade.

        Section 5.1.5 gives the selection rule rather than a part: shielded,
        DC resistance at most 2 ohm, rated for at least 100 mA, self resonant
        above 20 MHz. */}
    <inductor name="L3" inductance="15uH" footprint="0805" pcbX={1.0} pcbY={-22.0} schX={4} schY={-4} />

    <chip
      name="J3"
      footprint={<footprint>{uflPads()}</footprint>}
      pcbX={2.0}
      pcbY={-40.0}
      manufacturerPartNumber="U.FL-R-SMT-1(10)"
      pinLabels={{ pin1: 'ANT', pin2: 'GND', pin3: 'GND2' }}
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
      pcbX={-7.0}
      pcbY={-33.5}
      manufacturerPartNumber="MAX-M10S"
      /* u-blox MAX-M10S datasheet UBX-20035208 R08, with the connection
         requirements from the Integration manual UBX-20053088. Transcribed
         rather than assumed: the guess had VCC on pin 1 and pin 1 is GND, and
         RF_IN on pin 5 when it is pin 11.

         The good news from reading it is what this design does NOT need. The
         module integrates its own LNA, SAW filter and LTE band 13 notch filter,
         and RF_IN carries a built-in DC block matched to 50 ohm: "no additional
         RF front-end component is needed" for a passive antenna. So unlike the
         radio, whose front end is the one thing still blocking this board, the
         GNSS front end is inside the can. */
      pinLabels={{
        pin1: 'GND1',
        pin2: 'TXD',
        pin3: 'RXD',
        pin4: 'TIMEPULSE',
        pin5: 'EXTINT',
        pin6: 'V_BCKP',
        pin7: 'V_IO',
        pin8: 'VCC',
        pin9: 'RESET_N',
        pin10: 'GND10',
        pin11: 'RF_IN',
        pin12: 'GND12',
        pin13: 'LNA_EN',
        pin14: 'VCC_RF',
        pin15: 'VIO_SEL',
        pin16: 'SDA',
        pin17: 'SCL',
        pin18: 'SAFEBOOT_N',
      }}
      schX={8}
      schY={-9}
    />

    {/* The GNSS antenna, and the second thing this board declared and never
        connected.
        ------------------------------------------------------------------
        RF_IN went nowhere. A GNSS receiver with no antenna does not get a poor
        fix, it gets no fix ever, which makes the Track tier, whose entire
        reason to exist is position, a tier that cannot do the one thing it is
        for. Like the crystal, it built cleanly and routed cleanly and every
        check in this repository passed.

        A socket rather than an antenna soldered down, for the same reason as
        the radio side: this payload flies inside a body tube, a GNSS antenna
        needs to see sky, and where it ends up is a question about the airframe
        rather than about the board. Somebody with a fibreglass tube and
        somebody with a nose cone bay need the antenna in different places, and
        a connector lets both of them move it.

        Passive, not active. An active antenna has an amplifier in it and wants
        DC fed up the coaxial line through a bias network, which is more parts
        and a short circuit waiting to happen if the wrong antenna is plugged
        in. TODO(confirm-on-hardware): confirm a passive antenna acquires
        through the airframe wall. If it does not, the bias network is the
        answer and it is a change to this block. */}
    <chip
      name="J5"
      footprint={<footprint>{uflPads()}</footprint>}
      pcbX={-8.0}
      pcbY={-40.0}
      manufacturerPartNumber="U.FL-R-SMT-1(10)"
      pinLabels={{ pin1: 'SIG', pin2: 'GND', pin3: 'GND2' }}
      schX={13}
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
      pcbX={9.0}
      pcbY={-19.0}
      manufacturerPartNumber="PKLCS1212E4001-R1"
      pinLabels={{ pin1: 'IN', pin2: 'GND' }}
      schX={8}
      schY={-13}
    />

    {/* Three dice in one package, wired common cathode ON THE BOARD rather than
        inside the part.
        ------------------------------------------------------------------
        A PLCC-6 5050 usually brings out six independent pins, an anode and a
        cathode per colour, rather than a shared cathode. So "common cathode" is
        a decision this schematic makes by tying the three cathodes together at
        the ground pour, not a property to look for in a catalogue. The
        arrangement is the same either way from the microcontroller's side: a pin
        high through a resistor lights that colour.

        Doing it this way also means a part sold as common anode is not a
        different board. Six independent pins can be wired either direction; it
        is only a part with the commoning already inside it that constrains you.

        TODO(verify): the pin ORDER is supplier specific and this is where the
        risk sits. The common arrangement is pads 1 and 2 blue, 3 and 4 red, 5
        and 6 green, anode first, which is what is assumed below. Confirm it
        against the datasheet of the part actually ordered, because getting it
        wrong reverse-biases three dice and lights nothing, and because the
        series resistors above cannot be sized until that datasheet supplies a
        forward voltage per colour anyway. */}
    <chip
      name="D1"
      footprint="led5050"
      pcbX={6.5}
      pcbY={-31.0}
      manufacturerPartNumber="RGB-LED-CC"
      pinLabels={{
        pin1: 'A_B',
        pin2: 'K_B',
        pin3: 'A_R',
        pin4: 'K_R',
        pin5: 'A_G',
        pin6: 'K_G',
      }}
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
      /* C&K JS Series catalogue, page I-53, "PCB LAYOUT RECOMMENDED". Three
         pads and no others: the housing and actuator are both nylon and the
         Materials list has no metal shield, so there is nothing on this part to
         ground and no mounting land to add. Any footprint for this part with
         more than three pads did not come from C&K.

         The trap is the geometry, not the count. The pads are STAGGERED, not in
         a line: terminal 2, the common, exits one long side alone on the body
         centreline, and terminals 1 and 3 exit the other long side at 2.5 mm
         either side of it. Almost every other slide switch this size has three
         pins in a row, which is the footprint muscle memory draws, and it
         leaves this part with a lead in mid-air.

         Printed dimensions: pad width 1 mm, throws 2.5 mm either side of
         centre, 3 mm clear between the rows, 8.00 mm across the whole pattern.
         Pad length is not printed; 2.5 mm is (8.00 - 3) / 2, which is
         arithmetic on two printed numbers rather than a published figure.

         TODO(confirm-on-hardware): the plan view and the land pattern drawing
         are not labelled top or bottom view, so which throw is terminal 1 and
         which is terminal 3 is ambiguous. It does not matter here, because the
         two throws are functionally symmetric and this design uses one of them,
         but confirm with a meter before assuming a specific throw. */
      footprint={
        <footprint>
          <smtpad portHints={['1']} pcbX={-2.5} pcbY={-2.75} width="1.0mm" height="2.5mm" shape="rect" />
          <smtpad portHints={['2']} pcbX={0} pcbY={2.75} width="1.0mm" height="2.5mm" shape="rect" />
          <smtpad portHints={['3']} pcbX={2.5} pcbY={-2.75} width="1.0mm" height="2.5mm" shape="rect" />
        </footprint>
      }
      pcbX={8.5}
      pcbY={-5.0}
      manufacturerPartNumber="JS102011SCQN"
      pinLabels={{ pin1: 'T1', pin2: 'COM', pin3: 'T3' }}
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
    {/* Four VBUS pins and four grounds, all of them wired. They are rated
        collectively, not individually, so leaving some unconnected does not
        halve the current rating, it concentrates the whole current into the
        ones that are connected. */}
    <trace from=".J1 .A4" to=".U1 .VDD" />
    <trace from=".J1 .A9" to=".U1 .VDD" />
    <trace from=".J1 .B4" to=".U1 .VDD" />
    <trace from=".J1 .B9" to=".U1 .VDD" />
    <trace from=".C2 .pin1" to="net.V3V3" />
    {/* One pulldown per configuration channel, and they must never be tied to
        each other: the source uses which of the two it sees pulled down to work
        out which way round the plug went in. Tie them together and orientation
        detection stops. */}
    <trace from=".J1 .A5" to=".R1 .pin1" />
    <trace from=".J1 .B5" to=".R2 .pin1" />
    <trace from=".U1 .PROG" to=".R3 .pin1" />

    {/* USB data straight to the microcontroller. One connector does power,
        charging, configuration and offload. */}
    {/* The diagonals. A6 and B6 are both data plus, A7 and B7 are both data
        minus, and because the rows run opposite ways those pairs sit diagonally
        rather than in a column. Whichever way the cable goes in, one of each
        pair is the live one, so both have to reach the microcontroller. */}
    <trace from=".J1 .A6" to=".U3 .USB_DP" />
    <trace from=".J1 .B6" to=".U3 .USB_DP" />
    <trace from=".J1 .A7" to=".U3 .USB_DM" />
    <trace from=".J1 .B7" to=".U3 .USB_DM" />

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
    <trace from=".U3 .RADIO_NRESET" to=".U8 .NRESET" />
    {/* DIO2 is the RF switch control. SetDio2AsRfSwitchCtrl makes the radio
        drive it high in TX and low everywhere else, so the switch follows the
        radio without the firmware having to keep them in step. It is brought
        to the microcontroller as well because the same pin is a general
        interrupt line when that mode is off, and which of the two this design
        uses is not settled until the RF front end below is. */}
    <trace from=".U3 .RADIO_DIO2" to=".U8 .DIO2" />

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

    <trace from=".J1 .A1" to="net.GND" />
    <trace from=".J1 .A12" to="net.GND" />
    <trace from=".J1 .B1" to="net.GND" />
    <trace from=".J1 .B12" to="net.GND" />
    {/* The shell. The drawing's own signal column reads GND for it, on both
        halves of the table, so this is the manufacturer's instruction rather
        than a convention. */}
    <trace from=".J1 .SHELL00" to="net.GND" />
    <trace from=".J1 .SHELL01" to="net.GND" />
    <trace from=".J1 .SHELL10" to="net.GND" />
    <trace from=".J1 .SHELL11" to="net.GND" />
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
    <trace from=".U5 .VSS3" to="net.GND" />
    <trace from=".U5 .VSS8" to="net.GND" />
    <trace from=".U5 .VSS9" to="net.GND" />
    <trace from=".U6 .GND" to="net.GND" />
    {/* Pin 7 is the reserved pin the datasheet makes mandatory: "Connect to
        GND", with no alternative offered. The other four are permitted to be
        either grounded or left open, and they are grounded so that a reviewer
        reading the netlist sees a decision rather than an absence. */}
    <trace from=".U6 .RESV7" to="net.GND" />
    <trace from=".U6 .RESV2" to="net.GND" />
    <trace from=".U6 .RESV3" to="net.GND" />
    <trace from=".U6 .RESV10" to="net.GND" />
    <trace from=".U6 .RESV11" to="net.GND" />
    <trace from=".U7 .GND2" to="net.GND" />
    <trace from=".U7 .GND4" to="net.GND" />
    <trace from=".U7 .GND5" to="net.GND" />
    {/* Pin 11 takes ground or open. Pin 3 takes VS or open, and grounding it
        would be the mistake the datasheet is warning about by phrasing the two
        differently. Both are tied rather than left open, so the netlist records
        a decision. */}
    <trace from=".U7 .RESV11" to="net.GND" />
    <trace from=".U8 .GND2" to="net.GND" />
    <trace from=".U8 .GND5" to="net.GND" />
    <trace from=".U8 .GND8" to="net.GND" />
    <trace from=".U8 .GND20" to="net.GND" />
    <trace from=".U8 .EPAD" to="net.GND" />
    <trace from=".J3 .GND" to="net.GND" />
    <trace from=".J3 .GND2" to="net.GND" />
    <trace from=".U9 .GND1" to="net.GND" />
    <trace from=".U9 .GND10" to="net.GND" />
    <trace from=".U9 .GND12" to="net.GND" />
    {/* VIO_SEL picks the IO voltage: grounded selects 1.8 V, open selects 3.3 V.
        This board has one 3V3 rail, so it stays open, and that is a decision
        rather than an omission. A 1.8 V design would additionally have to hold
        that rail to plus or minus 2 percent, which a buck-boost following a
        lithium cell has no business promising. */}
    <trace from=".LS1 .GND" to="net.GND" />
    {/* The commoning. Three separate cathodes, one ground net. */}
    <trace from=".D1 .K_R" to="net.GND" />
    <trace from=".D1 .K_G" to="net.GND" />
    <trace from=".D1 .K_B" to="net.GND" />
    <trace from=".SW1 .T1" to="net.GND" />
    <trace from=".R8 .pin2" to="net.GND" />

    {/* The barometer's address select pin is tied low rather than left to
        float, so the part answers at a known address instead of an arbitrary
        one. */}
    <trace from=".U5 .SDO" to="net.GND" />

    <trace from=".U2 .VOUT" to="net.V3V3" />
    <trace from=".U3 .VDD" to="net.V3V3" />
    <trace from=".C3 .pin1" to="net.V3V3" />

    {/* Crystal, per "Hardware design with RP2350" figure 10. XIN goes straight
        to one terminal; XOUT reaches the other through the 1k damping
        resistor, so the load capacitor C5 sits on the crystal side of it
        rather than on the pin side. Getting that the wrong way round puts the
        resistor inside the tank and changes what the capacitors do. */}
    <trace from=".U3 .XIN" to=".Y1 .XA" />
    <trace from=".Y1 .XB" to=".R13 .pin1" />
    <trace from=".R13 .pin2" to=".U3 .XOUT" />
    <trace from=".C4 .pin1" to=".Y1 .XA" />
    <trace from=".C4 .pin2" to="net.GND" />
    <trace from=".C5 .pin1" to=".Y1 .XB" />
    <trace from=".C5 .pin2" to="net.GND" />
    <trace from=".Y1 .CASE1" to="net.GND" />
    <trace from=".Y1 .CASE2" to="net.GND" />
    <trace from=".U4 .VCC" to="net.V3V3" />
    <trace from=".U5 .VDD" to="net.V3V3" />
    {/* VDDIO is the digital interface supply and shares the 3V3 rail with the
        analog one. The part accepts 1.2 V to 3.6 V on it, so a design running
        the microcontroller at a lower IO voltage would split them; this one has
        one rail and every part on the board is specified at it. */}
    <trace from=".U5 .VDDIO" to="net.V3V3" />
    {/* CSB selects the interface. Figure 25, the I2C connection diagram, drives
        it to VDDIO, and the datasheet recommends it be driven by a programmable
        pin that is already at VDDIO at power on. It also carries an internal
        pull-up to VDDIO of 75 to 125 kilohm, and section 5.2 says it may be left
        open in I2C, which contradicts the figure. Tied high is the reading that
        is true under both: the pin is at VDDIO either way, and this way it does
        not depend on an internal resistor whose value has a 50 percent spread. */}
    <trace from=".U5 .CSB" to="net.V3V3" />
    <trace from=".C6 .pin1" to="net.V3V3" />
    <trace from=".C6 .pin2" to="net.GND" />
    <trace from=".C7 .pin1" to="net.V3V3" />
    <trace from=".C7 .pin2" to="net.GND" />
    <trace from=".R4 .pin2" to="net.V3V3" />
    <trace from=".R5 .pin2" to="net.V3V3" />
    <trace from=".U6 .VDD" to="net.V3V3" />
    <trace from=".U6 .VDDIO" to="net.V3V3" />
    <trace from=".C8 .pin1" to="net.V3V3" />
    <trace from=".C8 .pin2" to="net.GND" />
    <trace from=".C9 .pin1" to="net.V3V3" />
    <trace from=".C9 .pin2" to="net.GND" />
    <trace from=".C10 .pin1" to="net.V3V3" />
    <trace from=".C10 .pin2" to="net.GND" />
    <trace from=".U7 .VS" to="net.V3V3" />
    <trace from=".U7 .VDDIO" to="net.V3V3" />
    <trace from=".U7 .RESV3" to="net.V3V3" />
    <trace from=".C11 .pin1" to="net.V3V3" />
    <trace from=".C11 .pin2" to="net.GND" />
    <trace from=".C12 .pin1" to="net.V3V3" />
    <trace from=".C12 .pin2" to="net.GND" />
    {/* VBAT and VBAT_IO are both supplies: the radio core and the digital
        interface. VDD_IN feeds the power amplifier regulator, and on an SX1262
        the datasheet's own pin table says it connects to pin 10, which is
        VBAT. All three sit on the 3V3 rail rather than on the cell, so the
        radio sees a regulated supply at the bottom of the discharge. */}
    <trace from=".U8 .VBAT" to="net.V3V3" />
    <trace from=".U8 .VBAT_IO" to="net.V3V3" />
    <trace from=".U8 .VDD_IN" to="net.V3V3" />
    <trace from=".U8 .VREG" to=".L3 .pin1" />
    <trace from=".L3 .pin2" to=".U8 .DCC_SW" />
    <trace from=".U8 .XTA" to=".Y2 .XA" />
    <trace from=".U8 .XTB" to=".Y2 .XB" />
    <trace from=".Y2 .CASE1" to="net.GND" />
    <trace from=".Y2 .CASE2" to="net.GND" />
    <trace from=".U9 .VCC" to="net.V3V3" />
    <trace from=".U9 .V_IO" to="net.V3V3" />
    {/* V_BCKP keeps the battery-backed RAM and the real time clock alive across
        a power cycle, which is the difference between a warm start and a cold
        one. It is on the main rail rather than on a coin cell or a supercap,
        which is a real limitation stated plainly: while the payload has power
        the fix survives a firmware restart, and once the cell is disconnected
        the next start is cold.

        TODO(confirm): whether a supercap earns its mass here. The case for it is
        a scrubbed launch where the payload is powered down and brought back an
        hour later, and the case against is that a cold start on the pad is
        minutes of waiting rather than a lost flight. Decide it after timing a
        real cold start, which is the measurement named on the gnss row in the
        bill of materials. */}
    <trace from=".U9 .V_BCKP" to="net.V3V3" />
    <trace from=".U9 .RF_IN" to=".J5 .SIG" />
    <trace from=".J5 .GND" to="net.GND" />
    <trace from=".J5 .GND2" to="net.GND" />
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
    <trace from=".SW1 .COM" to=".U3 .ARM" />
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

# UM3 electrical evidence

This audit covers the published **1548-I mainboard**, **2012-K printhead**, and
**2015-H capacitive electrode**. It does not establish which board revision is
installed in the owner's printer, or verify every production revision supported
by the vendor firmware. The UM3 build must remain blocked until revision-specific
power/safety behavior and missing thermal integration are resolved.

## Sources and method

The following files are from official hardware repository revision
`c417f37bb34a8bfd0dc6b1fc28b0b381417ce9cb`. Page numbers below are PDF page
numbers, starting at 1.

| Source | SHA-256 |
| --- | --- |
| [1548-I mainboard schematic, 6 pages](https://github.com/Ultimaker/Ultimaker3/blob/c417f37bb34a8bfd0dc6b1fc28b0b381417ce9cb/PCB%20files/1548-I%20Ultimainboard/Schematics%20and%20layout%20pdf/Schematic%20Prints.PDF) | `93e72b655ff823131cc151501c453bb53f47ab0d5b8fab4f419a04824cb22df3` |
| [2012-K printhead schematic, 3 pages](https://github.com/Ultimaker/Ultimaker3/blob/c417f37bb34a8bfd0dc6b1fc28b0b381417ce9cb/PCB%20files/2012-K%20Print%20head%20PCB/Schematic%20and%20layout%20pdf/Schematic%20Prints_printhead_2012-K_01.PDF) | `8fa7a99e5d4fde1af5640d9ee5c738c197d87199280c607348e9ef82b1fe73a7` |
| [2015-H capacitive sensor schematic, 2 pages](https://github.com/Ultimaker/Ultimaker3/blob/c417f37bb34a8bfd0dc6b1fc28b0b381417ce9cb/PCB%20files/2015-H%20Capacitive%20sensor/Schematic%20and%20layout%20pdf/Schematic%20Prints.PDF) | `03e1de25e9b327c4952cd91924e7d9b3bb9546620323fe054df7e193ae63a27b` |

Installed Ghostscript extracted text and rendered the schematics; relevant pages
and small labels were checked visually at up to 300 dpi. Text extraction alone
interleaves net labels and pin names and is insufficient for assigning pins.
Arduino pin numbers below were cross-checked against the ATmega2560 section of
vendor `Marlin/fastio.h` at UM3 revision
`4545818112027387f09e87f930117ab8f3804327`.

## Mainboard signals corroborated by the schematic

| Function | AVR port | Arduino pin | Evidence |
| --- | --- | --- | --- |
| X step / direction / enable | PA3 / PA1 / PA5 | 25 / 23 / 27 | Page 4, U8 A4988 |
| Y step / direction / enable | PC5 / PC4 / PC6 | 32 / 33 / 31 | Page 4, U10 A4988 |
| Z step / direction / enable | PC2 / PC1 / PC3 | 35 / 36 / 34 | Page 4, U11 A4988 |
| E0 step / direction / enable | PL6 / PL7 / PC0 | 42 / 43 / 37 | Page 5, U12 A4988 |
| E1 step / direction / enable | PL0 / PL2 / PL1 | 49 / 47 / 48 | Page 5, U13 A4988, labelled Extra |
| X / Y / Z endstop inputs | PA0 / PA4 / PA7 | 22 / 26 / 29 | Page 2, J1 / J3 / J6 |
| X/Y current-reference PWM | PL3 | 44 | Page 4, shared XREF/YREF filter |
| Z current-reference PWM | PL4 | 45 | Page 4, ZREF filter |
| Extruder current-reference PWM | PL5 | 46 | Page 5, shared EXTRAREF filter |
| Hotend 0 / hotend 1 heater gates | PE4 / PE5 | 2 / 3 | Page 3, Q3 HEATER1 / Q2 HEATER2 |
| Heated-bed heater gate | PG5 | 4 | Page 3, Q1 HEATED-BED |
| Bed temperature input | PK2 / ADC10 | Analog 10 | Page 2, U5 INA826 output |
| Power relay low-side drive | PA2 | 24 | Page 3, R112 / T4 / K1 |
| Relay safety/high-side signal | PA6 | 28 | Page 3, U7 output / R109 / T3 |
| Board-ID divider | PK6 / ADC14 | Analog 14 | Page 3, R49 / R50 |
| Other voltage divider | PK5 / ADC13 | Analog 13 | Page 3, R122 / R128 |
| Host serial RX / TX | PH0 / PH1, USART2 | 17 / 16 | Page 1, RXD2 / TXD2 through U16 to J1 |

The A4988 enable inputs are active low and have external pull-ups, keeping the
drivers disabled when those signals are undriven (pages 4-5). Heater MOSFET gates
have pull-down resistors and turn on when driven high (page 3). Endstop inputs
have external 4.7 kOhm pull-ups and RC filtering (page 2); switch wiring, fault
interpretation, and homing inversion still need the complete firmware contract.

These signal assignments corroborate the matching subset of UM3's stale board
72 pins and the sibling S-line candidates. They do not justify copying the
entire S-line pin file: its TMC driver, board-revision, case-fan, and alternate
heater definitions describe additional hardware absent from these sheets.

### Power/revision mismatch remains unresolved

Page 3 visibly shows:

- R49 = 100 kOhm and R50 = 10 kOhm at ADC14, connected to the VCC/2 supply net.
- R122 = 100 kOhm and R128 = 10 kOhm at ADC13, connected to the +24 V net.
- An NE555 U7 safety circuit driving relay high-side transistor T3, with its
  output also connected to PA6; T4 on PA2 controls the low side.

However, UM3 `Board.cpp` identifies `BOARD_REV_I` using a **100 kOhm / 14.7 kOhm**
divider and says that revision has removed the safety circuit, requiring PA6 to
be driven as an output. The published 1548-I revision notes on page 6 list added
diodes but do not establish that resistor/circuit change. A schematic can also
include unpopulated components, so its drawing alone cannot prove the actual
assembly.

The mapping between firmware `BOARD_REV_I` and the published 1548-I design is
therefore **unresolved**. Do not select the PA6 output workaround from the PDF
title alone. Obtain matching assembly/BOM/source evidence and confirm the
installed revision before implementing that branch. The 2621B board's power
sequencing and pins are not covered by these schematics.

## Printhead circuitry corroborated by the schematic

| Peripheral | 7-bit I2C address | Evidence |
| --- | --- | --- |
| ADS1015 temperature ADC U2 | `0x48` | 2012-K page 1 |
| DS2482-100 hotend 0 / hotend 1 bridges U1 / U5 | `0x18` / `0x19` | 2012-K page 1 |
| FDC1004 capacitive converter U3 | `0x50` | 2012-K page 1 |
| PCA9635 fan / RGB output driver U6 | `0x44` | 2012-K page 2 |

These addresses match the vendor's added ADS101X, DS2482, FDC1004, and PCA9635
drivers. The 2015-H electrode sheet 2 is a passive SENSOR/SHIELD pair; the
measurement converter is on the printhead, not the electrode board.

### Temperature acquisition

2012-K page 1 shows ADS1015 AIN0 connected to hotend 1's PT100 divider, AIN1 to
hotend 2, AIN2 to the +3.3 V supply, and AIN3 to the reference divider. Each PT100
divider has a 1.21 kOhm, 0.1% pull-up. The reference uses a 1.21 kOhm pull-up and
205 Ohm, 0.1% lower resistor. The drawing explicitly specifies differential PGA
16, giving a +/-0.256 V range.

That corroborates vendor `TemperatureADS101X::setupAIN()` measuring AINn minus
AIN3 with the +/-0.256 V PGA, and the driver's signed 12-bit sample extraction.
It does **not** establish the missing UM3 lookup-table entries, sample averaging,
control cadence, stale-sample detection, hotend-presence thresholds, or heater
removal policy. S-line table 21 is a candidate for numerical comparison, not an
independently verified UM3 control implementation.

For the bed, mainboard page 2 shows an INA826 amplifier feeding ADC10. It does
not support substituting a printhead ADS1015 conversion for the bed path. Any
external-bed-ADC path belonging to another production board requires separate
evidence.

### Fans, RGB outputs, and probing

2012-K page 2 corroborates these PCA9635 output numbers:

| Output | Function |
| --- | --- |
| 3 | Front hotend-cooling fan |
| 4 / 5 | Left / right part-cooling fans |
| 10 / 11 / 12 | Left blue / green / red LED |
| 13 / 14 / 15 | Right red / green / blue LED |

This matches vendor `fan_driver.cpp` and `setPCA9635led()`. Fan outputs drive
transistor stages; preserve the vendor's `255 - fan_speed` conversion together
with its PCA9635 output mode rather than interpreting the register value as a
direct fan duty cycle.

2012-K page 1 connects the bed electrode to FDC1004 CIN1 and the shield to SHLD1
through 100 Ohm series resistors. This corroborates the electrical sensor path,
but cannot establish probe sample frequency, filtering, peak thresholds,
stopping distance, or failed-probe recovery. Those remain firmware/runtime
compatibility requirements.

## Remaining release blockers

- Identify the installed production mainboard revision and its actual assembly.
- Resolve the board-ID/safety-circuit mismatch and verify startup/shutdown
  polarities and ordering for every supported revision.
- Recover or independently validate the complete UM3 thermal conversion and
  safety implementation, including sample timing and every heater-output path.
- Recover the capacitive-probe algorithm constants and motion integration.
- Verify timer ownership and current-control calibration on real hardware.

No electrical measurements, hardware execution, thermal calibration, or
firmware flashing were performed. This is static schematic corroboration only.

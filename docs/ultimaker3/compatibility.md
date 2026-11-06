> Status update: the original audit below documents the incomplete public vendor
> source. The historical reconstruction now exactly reproduces stock, and the
> modern experimental UM3 image builds with that implementation connected to
> Marlin 2.1.2.9. See [integration.md](integration.md) and the current
> [inventory](inventory.json) for verification scope and remaining gates.

# Ultimaker 3 Extended compatibility audit

This is an evidence inventory, **not a completed or flashable port**. Entries marked
`upstream_candidate` still need comparison and tests; they do not establish
compatibility. The machine-readable inventory is [inventory.json](inventory.json).

[Binary recovery](recovery.md) now supplies tested temperature/PID, interlock,
power and coordinate components. The source-completeness section below records
the original vendor gaps; these components close only the isolated math/state
entries. Acquisition, output safety, commands and motion integration remain open.

## Pinned evidence

- Modern base: Marlin 2.1.2.9, `7d300b42acadb3e7467e9f4b4fc03ebea592c2d0`.
- UM3 reference: UltimakerMarlin `Ultimaker3`,
  `4545818112027387f09e87f930117ab8f3804327`.
- Common ancestor: `2015989f84918810be615deb57ad3baf9f66c203` (2013-06-18).
- S-line comparison only: `f91240bb04e52818746a866bfe6e6af543209b79`.
- Stock host: official `um-update-5.3.0.20211003.swu`, SHA-256
  `5599f1108837fc365b4f5fd567d1561db749a7b28f7befad1e3117d75b182df2`.

The [official package](https://software.ultimaker.com/releases/firmware/9066/stable/5.3.0.20211003/um-update-5.3.0.20211003.swu)
contains a SquashFS filesystem and a trailing signature. Its local SHA-256 records
the inspected bytes; signature authenticity has not been independently verified.
Source paths below are paths inside that package unless prefixed `vendor:`.
The inventory records member hashes so analysis can be reproduced without
redistributing the host filesystem.

## Source completeness is a hardware blocker

The [legacy build reproduction](vendor-build.md) now verifies this concern using
the official Arduino 1.0.5 GCC 4.3.2 tools. CMake configuration and an Arduino Blink
control build pass, but the unmodified UM3 tree fails compilation. Supplying its
existing default board value globally removes an include-order defect while
leaving the missing thermal, planner and probing interfaces unresolved.

The UM3 commit adds drivers and replaces `Marlin_main.cpp`, but leaves its UM2
parent versions of `Configuration.h`, `Configuration_adv.h`, `pins.h`,
`Marlin.h`, `MarlinSerial.cpp/.h`, `planner.cpp/.h`, `stepper.cpp/.h`,
`temperature.cpp/.h`, `thermistortables.h`, and `watchdog.cpp/.h` unchanged.
These files cannot collectively implement the added main program:

| Subsystem | Concrete gap | Required resolution |
| --- | --- | --- |
| Configuration | UM2 serial port 0, one extruder, E=282 and Z=230 defaults remain. | Separate verified UM3 defaults from settings sent by the host. |
| Board/pins | `Board::detect()` references undefined `ADC_INPUT_VOLTAGE_PIN`; newer-board initialization also requires absent voltage and power-fail pins. | Cross-check each supported UM3 revision against its schematics; do not import S-line pins wholesale. |
| Temperature conversion | Added ADS101X driver reads signed 12-bit differential samples at ±0.256 V gain. Existing table 20 describes the old INA826 path; required newer conversion wiring/table is absent. | Recover complete UM3 thermal acquisition/conversion and verify channels, gain, compensation, saturation and failure handling. |
| Thermal control | Main uses `temperatureInit`, `hotend_pid`, `heated_bed_pid`, `pwr`, presence and removal-state variables absent from the retained temperature core. | Integrate the verified PID cadence, power budget, removal interlocks and every heater-output path. |
| Safety | New `Stop.h` reason macros conflict with retained `Marlin.h`; rev I deliberately drives the safety pin as an output. | Preserve board-specific startup/shutdown sequencing and host error mapping. |
| Motion | Missing planner-free-space, interrupt-control and runtime-direction interfaces; main stores negative rounded XYZ nozzle offsets in steps, but retained planner does not implement that contract. | Verify logical/physical coordinates, tool changes, homing, limits and actual step position semantics. |
| Probe | Capacitive algorithm requires absent sample/filter/threshold configuration and tight integration with step position and movement termination. | Recover UM3 constants and validate probe timing and error paths. |

S-line contains related implementations but also changes these subsystems. Its
values are candidate evidence, not proof of UM3 behavior. Host defaults below do
not fill the missing AVR conversion or safety logic. No dummy sensor, guessed
temperature table, disabled protection or no-op command may close these entries.

## Authoritative stock host defaults

`usr/share/griffin/griffin/machines/9511.json` identifies **Ultimaker 3 Extended**
and inherits `um3.json`. The download's `9066` URL component is not the Extended
machine identifier. `9066.json` identifies the regular Ultimaker 3.

| Setting | Host value | Owner/evidence |
| --- | --- | --- |
| Build and movement limits | Min `(0,0,0)`, max `(215,215,310)` mm | `9511.json`; emitted as `M12000/M12001` |
| Steppers | X=80, Y=80, Z=400, E=369 steps/mm | `um3.json`; `M92` |
| Direction inversion | X=true, Y=false, Z=true, E0=true, E1=false | `um3.json`; `M12004` |
| Motor current | XY=1300, Z=1300, E=1250 mA | `um3.json`; `M907` |
| Hotends | Two; nominal second-core X offset 18 mm, Y offset 0 | `um3.json`; individual calibration remains host-owned |
| Sensors | Capacitive bed sensor enabled; filament flow sensor absent | `um3.json` |
| Power | Total 221 W, idle 46 W, maximum 75 W per slot; 24 V bed and hotends | `um3.json`; `M290/M292` |
| Bed resistance model | 3.678255 Ω at 60 °C; 0.012285 Ω/°C | `um3.json`; `M291` |
| Hotend PID defaults | Kff=.57, Kp=9, Ki=.2, Kd=40, Ki_max=100, Kpcf=.06 | `um3.json`; `M301` |
| Bed PID defaults | Kff=2.5, Kp=125, Ki=0, Kd=0, Ki_max=60 | `um3.json`; `M304` |
| PID functional range | 25 °C | `printer/heatableObject/heatableObject.py` |
| Temperature compensation | Core 0 `(1,0)`; core 1 `(.985,.75)`; glass bed `(1,0)` | `um3.json`; measured × factor + offset via `M12005/M12006` |
| Bed maximum | 115 °C at the host | `um3.json`; not evidence of the AVR safety threshold |

`printer/drivers/marlin/applicationLayer.py:start()` maps current properties to
commands. On connection it queues `M115`, `M80`, then forces all property links
to update. Preserve these runtime updates and host-stored individual calibration;
modern Marlin EEPROM defaults must not overwrite them. The host applies bed-level
compensation, manages physical nozzle lifting and intercepts print-file commands
including `M109`, `M190`, `T0/T1`, `G28`, pauses and `G4`; raw maintenance commands
still reach the AVR. The same opcode can therefore have distinct host and AVR
responsibilities.

## Serial and command contract

`osiHelpers/networkPacket.py` constructs `FF FF`, sequence 0–254, payload length
1–96, ASCII payload and big-endian CRC16 over sequence, length and payload.
`transportLayer.py` recognizes `o`, `r`, `q`, CRC8-protected response data,
`LOG:`, `ProtoError:`, errors and warnings. It assumes **15 usable planner slots**
and limits itself to **5 outstanding receive packets**. The stale vendor command
queue has 8 slots; this is not the host's outstanding-packet limit. Queue
accounting and acknowledgement timing must match.
`applicationLayer.py` parses hotend status as
`Tn:current/target@powerppresent eenabled fraw/steps` without the illustrative
spaces before `e` and `f`; the actual concatenated grammar is recorded in the
inventory. Bed status is `Bcurrent/target@power`.

The inventory lists every G/M dispatch case in the published UM3 main program,
plus `Tn`; each retains an explicit unimplemented, blocked or upstream-candidate
status. It is an opcode inventory, not a claim that the incomplete source was
built. Required conflicts include:

| Opcode | UM3 meaning | Marlin 2.1.2.9 conflict |
| --- | --- | --- |
| G30/G31 | Capacitive probe and baseline | Generic single probe / sled functions |
| M143 | Print-head LEDs | Cooler maximum temperature when enabled |
| M145 | Disallow hotend removal / enable sensor-error checks | LCD preheat presets |
| M149 | Read print-core serial | Temperature units |
| M150 | Read print-core EEPROM page | Color LEDs |
| M290 | Power budget | Babystepping |
| M301/M304 | Per-target extended PID including lowercase `i`, feed-forward and functional range | Standard PID interfaces have different parameters/scaling |
| M401 | Quick-stop and position response | Deploy probe |
| M405–M407 | Flow sensor enable/reset/raw read | Filament-width controls |
| M204/M205/M218/M907 | Legacy acceleration/jerk, step-based XYZ offsets, grouped motor currents | Similar names require parameter/coordinate comparisons |

Two source defects must not be carried forward: the vendor command queue writes
a terminator one byte past its 96-byte slot at maximum payload, and resend logic
relies on old `MarlinSerial::flush()` discarding RX rather than draining TX.
Allocate a separate terminator byte, explicitly discard RX where required, and
test both. Vendor `println()` emits CRLF; use exact host-compatible fixture bytes.

## Bootloader and installation behavior

`um3.json` configures `ucFirmwareUpgrade.avr.MultiStageProgrammer`,
`/usr/share/griffin/griffin/machines/jedi.hex`, `/dev/ttyS1` at 250000 baud,
`/dev/gpio/avr` and `/dev/gpio/avr_reset`. The firmware's AVR UART number must
be verified separately; the Linux device name alone does not establish it.

`initiateHardwareInitialization.py` loads that HEX and calls `program()` at host
startup. `ucFirmwareUpgrade/common/firmwareUpdateBase.py` compares the configured
image against MCU flash, skips matching images and programs mismatches. A custom
MCU image alone is therefore replaced at the next initialization. Installation
must back up and replace the configured `jedi.hex` as well; full stock updates
or recovery can restore the stock file. Preserve host calibration separately.

The packaged `bootloader-atmega2560-uart250000-2.hex` occupies
`0x3e000..0x3e7ab`. Packaged chip defaults are fuses `FFD0FD` and 256-byte flash
pages. Reserve the entire bootloader region `0x3e000..0x3ffff`: application
flash must stay below **253952 bytes**. This is evidence for the stock package,
not a reading of this individual printer's fuses or bootloader.

The stock `jedi.hex` contains 77942 data bytes at `0..0x13075`. Its size is a
baseline only. Build artifacts and static SRAM reports do not establish stack
headroom, interrupt timing, electrical correctness or hardware safety.

No hardware tests or stock firmware execution were performed in this audit.
The unresolved thermal, safety, probing and motion entries block a drop-in claim.

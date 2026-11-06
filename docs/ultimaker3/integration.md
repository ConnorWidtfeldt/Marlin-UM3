# Integrated UM3 Extended development image

`pio run -e um3_extended` now links a complete ATmega2560 / 16 MHz firmware
using **Marlin 2.1.2.9** and the verified UM3 host/hardware implementation.
It is an experimental image, not a hardware-qualified drop-in replacement.
The historical stock reconstruction remains unchanged on its separate branch.

## Implementation boundary

The `um3_stock` namespace contains the command frontend, binary protocol,
peripherals, capacitive algorithm and thermal implementation from stock baseline
`7a80a3bb615362f513e1608447a6eb92efa35098`. That baseline generates exactly the
shipped 5.3.0 HEX, SHA-256
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
Original GPL notices and the [import provenance](../../Marlin/src/feature/ultimaker3/stock/PROVENANCE.md)
are retained. No legacy planner, stepper ISR or arc implementation is compiled.

[The native adapter](../../Marlin/src/feature/ultimaker3/native_adapter.cpp)
connects these interfaces to the current Marlin planner, stepper and arc engine.
Stock XYZ quantization subtracts signed tool offsets **after rounding to steps**;
E truncates toward zero. Quick-stop reconstructs logical position by adding the
selected tool's step offset to executed steps. M205 X retains the Euclidean XY
jerk meaning. The material-cooling setting is snapshotted into each native planner
block and sent through the stock PCA9635/PWM fan driver, including its 100 ms
kick-start; later M106/M107 commands do not change queued blocks. These contracts have boundary/overflow
tests; they do not make the
new planner's acceleration scheduling identical to the historical planner.

The dedicated configuration preserves compiled ROM defaults (including Z=240,
200 steps/mm); Griffin later supplies Extended Z=310, 400 steps/mm and per-machine
calibration. No Marlin EEPROM settings are loaded. Input shaping, Linear Advance,
S-curve acceleration and tuning-dependent motion increases remain disabled.

## Resource ownership

| Resource | Owner and integration |
| --- | --- |
| USART2, 250000 baud | Native buffered UART; stock formatter/framer preserves LF, sequence, CRC, ACK/resend and planner credits. Native ASCII and emergency parsing do not consume binary input. |
| Timer0 overflow | Arduino clock. |
| Timer0 compare B / ADC | Stock 1.024 ms tick, 32.768 ms thermal averaging; ISR also polls native endstops and services native planner cleanup. Bed pin 4 retains the stock Timer0 PWM sharing. |
| Timer1 | Modern Marlin stepper ISR. Starts after stock hardware/planner initialization. |
| Timer3 | Stock 8-bit hotend PWM. |
| Timer5 | Stock 8-bit A4988 current-reference PWM, prescaler 1. |
| TWI | Stock priority driver and ADS1015, FDC1004, DS2482/DS2431, fan and lighting interfaces. |
| Watchdog | Native AVR 4-second hardware watchdog. Only completed fresh hotend ADC channel reads refresh it during normal operation. |

The generic HAL PWM timer initialization is skipped for this target: changing
ICR/TOP while retaining Arduino `analogWrite` would corrupt both heater duty
and current calibration. The integrated profile supports detected production
board IDs 2 and 3. Unknown, prototype and S-line board IDs are refused with the
relay off. Shared vendor source contains conditional newer-board routines, but
this target cannot select them.

## Deliberate safety changes from stock

- The command queue reserves a terminator beyond the complete 96-byte payload.
- M17/M18/M84 use native enable-state APIs, keeping the next move coherent.
- Raw M42 writes are limited to the system light; they cannot bypass heaters,
  current control, motor drivers, serial, TWI or the safety relay.
- All heater-off paths call the output owner to detach hardware PWM. A direct
  PORT write alone does not detach the PWM peripheral.
- The tested bounded power allocator limits both slots and the bed to the total
  budget, including overload cases the vendor allocator did not cap globally.
- Latched faults abort queued motion, cut the relay, disable heaters and refuse
  new heating/power-up requests. M999 does not restore old targets.
- Completed ADS read failures can be retried by M999 using real I2C transactions;
  an unfinished/wedged transaction remains latched. Stalled acquisition cannot
  continually refresh the hardware watchdog using cached temperatures.
- Invalid PID coefficients produce zero output; invalid targets/calibration are
  rejected or faulted before the relevant native conversion/output path.
- Native hotend/bed heating-watch and thermal-runaway state machines run on each
  acquired sample before stock heater output. Their additional thresholds use
  the upstream configuration defaults. Stock removal, sensor counters,
  communication timeout and full-output heating-progress protection remain.

Native hotend min/max faults retain their stock error categories. Additional
runaway/heating-watch faults use the existing heater-error category; bed faults
use the existing bed-error category, with a `LOG:Marlin thermal protection` reason
where available. The stock host has no separate modern runaway error field.
Modern quick-stop retains its approximately one-second cleanup delay; the stock
host's receive timeout is 20 seconds. These temporal changes need printer testing.

## Verification

The host integration harness compiles the **production** frontend, protocol,
thermal/PID, probe and EEPROM layers with test-only GPIO/TWI/motion boundaries.
No test hardware definitions are on the firmware include path. It covers:

- Official packet/response fixtures, exact LF output, corrupt CRC, invalid
  lengths/characters, wrong
  sequences, byte fragmentation, an eight-command queue filling/rejecting,
  planner saturation/credit updates, complete 96-byte commands and wraparound.
- Board IDs 2 and 3, unsupported-board refusal, initialization and identity.
- Runtime geometry, steps/mm, directions, current units, signed tool offsets,
  executed-position quick-stop, unexpected-endstop position logs, intentional-hit
  suppression, stepper enable/disable and compensation.
- Print-core serial/page reads, capacitive sampling/noise and missing-sensor
  abort, heating/removal, power limits, invalid PID values, latched shutdown,
  recovery, failed ADC reads, wedged acquisition and missing/stuck endstops.

Captured responses are independently accepted by the unmodified, SHA-pinned
5.3.0 host protocol parser, and actual M105 reports pass its application-layer
temperature/core-presence/target parser. The six isolated component suites remain complementary math
and protocol checks. Both the integrated UM3 and unaffected upstream AVR images
build; reports enforce application addresses below `0x3e000` and static SRAM at
most 6144 bytes. Build source hashes, toolchain identities, HEX/ELF/map, sizes and
checksums are delivered together. The build epoch participates in incremental
build signatures as well as GCC's environment.

The native harness mocks the modern motion/backend boundary; it does **not**
execute AVR instructions or measure interrupt latency. Successful capacitive
contact/height accuracy, live homing/motor timing, native thermal fault timing,
combined heating, stack high-water use, physical revision-I wiring and actual
updater installation remain unqualified. `release_ready` remains false and
`verify.py check-release` continues to fail. Compilation and offline mocks do
not qualify an image for unattended or physical use.

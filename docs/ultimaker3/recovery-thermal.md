# Thermal behavior recovered from shipped binaries

This is an evidence ledger for reconstruction, not a hardware qualification.
The findings below come from ordinary LLVM symbol/disassembly tools, direct
Ghidra 12.2 decompilation, byte extraction, and controlled Unicorn replay of
selected shipped ARM functions. They are **not REA Evidence records**. The REA
adapter rejected the installed Ghidra version, so direct tool evidence is
identified explicitly rather than assigned invented provider IDs.

## Artifact identity and boundary

- Packaged ARM simulator: `usr/bin/jedi-marlin-simulator`, SHA-256
  `bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.
- AVR image: `usr/share/griffin/griffin/machines/jedi.hex`, SHA-256
  `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
- Both were extracted from the pinned stock UM3 `5.3.0.20211003` package, whose
  AVR package version is `2019.01.27`.
- Source comparison: UM3 `4545818112027387f09e87f930117ab8f3804327` and sibling
  S-line `f91240bb04e52818746a866bfe6e6af543209b79`.

The simulator is a symbol-bearing ARM ELF with simulated AVR registers and I/O.
Its symbols establish what the missing thermal interfaces are named. Its
instructions establish behavior in that build. The exact temperature tables
below are independently present in the AVR flash image. Other ARM behavior still
requires AVR corroboration or controlled tests before claiming hardware parity.
ARM's C++ `double` is 64-bit while AVR's is 32-bit; numerically equivalent source
expressions can therefore round differently between these builds.

## Named routines and objects

All ARM addresses below are ELF virtual addresses, not file offsets or AVR
addresses. Function ranges are start plus the symbol size.

| Routine/object | ARM address | Size |
| --- | --- | --- |
| `setAllowHotendRemoval(uint8_t,bool)` | `0x3443c` | `0x90` |
| `nozzle_temperature_watchdog(uint8_t)` | `0x344cc` | `0xfc` |
| `manage_heater()` | `0x345c8` | `0x370` |
| `setHeaterOutput(int8_t,uint8_t)` | `0x34938` | `0x146` |
| `analog2temp_sub(int16_t,table,uint8_t)` | `0x34a80` | `0x16c` |
| `analog2temp(int16_t,uint8_t)` | `0x34bec` | `0x44` |
| `analog2tempBed(int16_t)` | `0x34c30` | `0x62` |
| `updateTemperaturesFromRawValues()` | `0x34c94` | `0x3c4` |
| `temperatureInit()` | `0x35058` | `0x168` |
| `disable_all_heaters()` | `0x35288` | `0xc8` |
| Timer 0 compare-B ISR, `__vector_22()` | `0x354e4` | `0x1dc` |
| `TemperaturePID::TemperaturePID()` | `0x35d10` | `0x9c` |
| `TemperaturePID::update(float)` | `0x35dac` | `0x890` |
| `TemperaturePID::resetState()` | `0x3663c` | `0x22` |
| `PowerBudgetManagement::getActualBedOutput()` | `0x366b8` | `0x7a` |
| `PowerBudgetManagementBed::getActualHeaterOutput()` | `0x36778` | `0x160` |
| `PowerBudgetManagementHotendSlot::getActualHeaterOutput()` | `0x36924` | `0x54` |
| `hotend_pid[2]` | `0x58844` | `0x88` |
| `heated_bed_pid` | `0x588cc` | `0x44` |
| `hotend_temperature_adc` | `0x58910` | `0x54` |
| `bed_temperature_adc` | `0x58964` | `0x54` |
| `pwr` | `0x589b8` | `0x54` |

These named globals are present in the shipped simulator although the published
UM3 tree's retained temperature core does not define them.

## Exact ADC tables, corroborated in AVR flash

Both tables are arrays of signed little-endian 16-bit `(raw, Celsius)` pairs.
Their raw values include an oversampling factor of **8**. They exactly match the
sibling S-line table definitions with `OVERSAMPLENR=8`; copying the stale UM2
oversampling configuration would give incorrect conversion.

| Table | Example ARM VA | AVR flash byte offset | Length | SHA-256 of exact bytes |
| --- | --- | --- | --- | --- |
| 20, bed INA826 | `0x403f8` | `0x1e78` | 116 bytes, 29 rows | `f6e1e186f15bb3febdda4ba7a2b924c5654f8e9a78dd0ebfd5b622df59e62808` |
| 21, printhead ADS101X | `0x4046c` | `0x1e44` | 52 bytes, 13 rows | `e2e0c9e105f891ad1f11167f49e9a4776a8492fedcf5bdf32a26b7e34c33c691` |

Table 21's exact scaled pairs are:

```text
(-17448,-50) (-14472,0) (-11632,50) (-8904,100) (-6296,150)
(-3800,200) (-1400,250) (896,300) (3104,350) (5216,400)
(7248,450) (9200,500) (16288,700)
```

Table 20's exact scaled pairs are:

```text
(0,0) (1816,1) (1960,20) (2096,40) (2232,60) (2360,80)
(2496,100) (2632,120) (2760,140) (2888,160) (3016,180)
(3144,200) (3272,220) (3392,240) (3520,260) (3640,280)
(3760,300) (3880,320) (4000,340) (4120,360) (4232,380)
(4352,400) (4912,500) (5448,600) (5952,700) (6440,800)
(6896,900) (7336,1000) (7744,1100)
```

The simulator has many identical copies because the arrays were defined in a
header. The AVR image has one exact occurrence of each byte sequence. ELF
`.rodata` has a VA-to-file-offset difference of `0x10000`, so example table 21
bytes are at simulator file offset `0x3046c`. The AVR offsets above refer to the
addressed bytes obtained by parsing Intel HEX, not ASCII offsets in `jedi.hex`.

`analog2temp_sub` searches from row 1 for the first table raw value greater than
the sample, then linearly interpolates with the preceding row. Values above the
last row return the last temperature; below the first row extrapolate using the
first segment. The signed arithmetic is significant for negative ADS readings.

At `0x34ca8..0x34ce4`, ADC hotend channels 0 and 1 are fetched and multiplied by
8 before storage/conversion. Board ID 4 also reads external bed channel 0 and
multiplies by 8 (`0x34cfa..0x34d1c`); other boards retain the internal ADC bed sum.
`analog2tempBed` uses table 21 for board 4 and table 20 otherwise. Both bed and
hotend Celsius results then pass through `GainCompensator::compensate()`.

## PID constants and layout

The simulator's PID class is 68 bytes, including ARM alignment padding. The
published `temperaturePID.h/.cpp` has the same observed field sequence and
algorithm, so it is a much stronger source candidate than the stale main
temperature engine. Do not reuse the ARM byte offsets as AVR struct offsets.

| ARM field offset | Meaning |
| --- | --- |
| `0x00/04/08/0c` | Kff/Kp/scaled Ki/scaled Kd |
| `0x10/14/18` | Ki limit/fan feed-forward/unused extrusion feed-forward |
| `0x1c` | extrusion index, initialized 255 |
| `0x20/24/28` | functional range/min input/max input |
| `0x2c/2d` | max/min output bytes |
| `0x30/34/38/3c` | target/integral/derivative/previous sample |
| `0x40` | debug flag |

- Constructor functional range is **25°C** (`0x35d60`); gains/target/state start
  at zero, integral limit/max output at 255, minimum output at zero.
- A consecutive sample jump with absolute magnitude **greater than 5°C** returns
  zero immediately and replaces the previous sample (`0x35de0..0x35e00`).
- Error above the functional range resets integral/derivative and requests max
  output. Error below negative range or target exactly zero requests min output.
- Otherwise P is `Kp*error`, I accumulates `error*Ki` and is clamped to ±Ki_max.
  D is `(1-K1)*(Kd*sample_delta) + K1*old_D`, with K1 **0.99**. The exact ARM
  double literals are at `0x36470` (`0.010000000000000009`) and `0x36478` (`0.99`).
- Feed-forward is `Kff*(target-35°C)` (`0x36488`); fan feed-forward is Kpcf times
  `getCoolingFanSpeed()`. The total is clamped to min/max output and converted
  to uint8; outside the configured input range the result is min output.
- M301/M304's inline setters contain the exact double **0.032768** at
  `0x24b80` and `0x24f08`. They scale Ki by this sample interval and Kd by its
  reciprocal. This matches the sibling formula `8*4/(16000000/64/256)`.
- ISR `__vector_22` has a four-state sampling cycle after startup and publishes
  when the count exceeds 7 (`0x35614`). Combined with Timer 0's 1.024 ms interval,
  this gives **32.768 ms**, not the stale source comment's 128 ms. The timer
  configuration must still be checked against the AVR build before integration.

## Interlocks, fault thresholds and fan behavior

The following simulator observations describe the behavior to reconstruct and
test; they do not independently qualify heater control on physical hardware.

- `setAllowHotendRemoval` accepts extruder indices 0/1. Enabling removal is refused
  if current temperature is **greater than 70°C** (`0x344c8`) or target is nonzero.
- Raw hotend presence requires a successful DS2482 reset and scaled raw less than
  **`0x3f80` = 2032*8** (`0x34808`). A present hotend at **62°C or above** disables
  removal allowance (`0x3492c` is 62.0).
- The shared fan switches on if any non-removal-allowed hotend exceeds **40°C**;
  otherwise it can switch off if a hotend is below **35°C** or removal is allowed
  (`0x34930/0x34934`). Values in the hysteresis band preserve the prior state.
- `setHeaterOutput` hard-zeros a hotend output if removal is allowed or
  `stop_heaters_pwm` is true (`0x34950..0x34976`). Bed output while PWM is stopped
  is either 0 or 255 (`0x349fc..0x34a12`).
- Board 4 uses software bed PWM with output divided by 2; others call
  `analogWrite(4,output)` (`0x34a14..0x34a42`). The newer sibling separates nozzle
  and bed setters and divides at a different point; preserve behavior, not its
  unrelated board-V4 implementation.
- Full-power watchdog only times periods when the last actual nozzle output is
  255. A rise of **more than 1°C** refreshes baseline (`0x3450e`); elapsed time
  **greater than 30000 ms** faults (`0x3457a` literal `0x7530`). This differs from
  the stale UM2 config's 10°C setting.
- Simulator hotend min/max arrays at `0x42f90/0x42f9c` contain **5/365°C**. The
  fault checks apply compensation to these thresholds too. Bad hotend samples
  add 10 to an error count (overtemperature when absent adds 4), trigger at
  **count >100**, and normal samples decay by 1. Removal allowance resets counts.
- Bed maximum is **200°C** (`0x35054`); over-limit samples increment a counter,
  trigger after **count >20**, and below-limit samples decay by 1. This is the
  AVR-control safety threshold candidate, separate from the host's 115°C limit.
- Watchdog reset is after conversion and fault checking (`0x35032`), preserving
  the relationship between fresh temperature processing and liveness.

## Reconstructed modules and independent verification

`Marlin/src/feature/ultimaker3/temperature_pid.{h,cpp}` adapts the published GPL
PID implementation using the constants and arithmetic above. It retains the
vendor setter names and explicitly receives the cooling-fan byte, avoiding I/O
dependencies in the control math. The unused extrusion-feed-forward fields and
fixed output-limit fields are omitted. `setDebugDump` retains a queryable flag;
the original `||PID:` serial diagnostic is not emitted by this standalone module.
The UM3 command/serial integration must implement that output if the host uses it.

`temperature_conversion.{h,cpp}` contains the exact signed table values in AVR
program memory. Its raw-sum interfaces explicitly use factor 8, independent of
modern Marlin's oversampling setting. The ADC interface validates a signed 12-bit
ADS101X sample before scaling. Gain and offset are caller-supplied; the module
does not invent calibration or persist values. The named bed function uses table
20 for the internal ADC/INA826 path. A board-4 external bed ADC must select the
table-21 converter; board detection and sample acquisition are not implemented
by these math functions.

Two deliberate defensive departures are isolated from valid stock behavior:

- The stock constructor does not initialize input bounds. The reconstruction
  starts with inverted bounds and returns zero until `setControlRange` supplies
  finite, ordered limits. It does not copy undefined uninitialized memory.
- Nonfinite sensor/target/bounds/gains, negative integral limits or functional
  ranges, and nonfinite computed PID outputs return zero and clear PID state,
  preventing undefined float-to-integer conversion and malformed settings from
  entering either the full-power or proportional control branches. Invalid
  conversion compensation or an out-of-range ADS sample returns NaN for the
  eventual sensor-fault handler to reject. These paths are defensive additions,
  not claims about observed vendor behavior.

Run `python3 buildroot/tests/um3/run_thermal.py` to compile the actual modules
with AddressSanitizer and UndefinedBehaviorSanitizer. The committed
`stock_oracle.json` was generated by replaying the shipped ARM constructor at
`0x35d10`, PID update at `0x35dac`, and interpolation at `0x34a80` with Unicorn
2.1.4. ELF load segments were mapped without executing the binary entry point or
dynamic linker. The real fan getter reads its mapped `current_fan_speed` global
at `0x580c8`; no implementation stub supplies the PID result. This is a binary
function oracle, not a second source implementation of the equations.

The thermal suite passes **320 checks**, including all **177 PID outputs** with
exact byte equality and **86 conversions** within 0.0001°C of the stock replay.
It also exercises sample-rejection boundaries, recovery, input limits, target
zero, feed-forward, integral clamps/reset, compensation, signed ADC domains,
table extrapolation/clamping, and the defensive invalid-input paths. Native
compilation disables floating-point contraction. ARM's 64-bit double arithmetic
is preserved for comparison; AVR naturally evaluates the original unsuffixed
constants as its 32-bit double. Exact ARM equality does not establish an AVR
trajectory's final PWM byte at all rounding boundaries.

Both production `.cpp` files also compile as isolated objects with the pinned
AVR GCC 7.3 toolchain, `-mmcu=atmega2560 -DF_CPU=16000000L -Os`, and warnings as
errors. The conversion object has 168 bytes of `.progmem.data`, zero `.data`, and
zero `.bss`; its tables do not consume static SRAM. This isolated-object check
does not establish final linked flash/SRAM headroom or physical timing.

## Remaining work

The exact ADC table identity, simulator PID/conversion behavior, and standalone
math verification are recovered. Still required before closing the thermal gate:

1. Corroborate the named control routines and timer/output paths in the AVR image,
   including architecture-specific numerical behavior and I²C sample freshness.
2. Implement and exercise sample freshness, interlocks, watchdogs, fault counters,
   power-limit application, and diagnostic protocol integration. The conversion
   and PID math replay does not cover those control paths.
3. Integrate with modern Marlin's sample readiness, PWM and emergency shutdown
   paths so every heater-output path remains subject to the recovered gates.
4. Validate board-revision power/safety wiring, real sensor faults, probe timing
   and thermal calibration on physical hardware.

Only selected simulator instructions were executed under controlled emulation.
The recovered modules are not wired to Marlin's heater outputs, and no printer
or heater was operated. Thermal safety and drop-in hardware parity remain open.

## Recovered removal and heating-progress helpers

`thermal_interlock.{h,cpp}` now separates two recovered per-hotend state machines
from the unimplemented physical output and shutdown integration. The earlier
remaining-work item for interlocks/watchdogs is partially addressed by these
helpers; their actual attachment to every heater path remains open.

`HotendRemovalInterlock` starts with removal allowed. The stock simulator data
at `0x5756c` contains `01 01`, one initial flag per hotend, and the recovered
`temperatureInit()` at `0x35058` does not change those flags. It is therefore
incorrect to substitute a default that immediately allows heating. The caller
must hard-zero a hotend output while removal is allowed, independently of PID.

The request behavior follows `setAllowHotendRemoval()` at `0x3443c`: enabling
requires current temperature at most 70°C and target exactly zero. Rejected
enable requests preserve the preceding permission, whether true or false;
disabling permission is unconditional. The stock routine also rejects indices
above 1; this helper has one instance per slot, so the eventual command handler
must validate the hotend index before selecting it. The helper's return value
reports acceptance; the vendor setter itself returns void. `observe()` follows
the `manage_heater()` block at `0x3481e..0x34860`: a present core at 62°C or
above clears removal permission, and subsequent cooling does not restore it.

`HeatingProgressWatchdog` follows `nozzle_temperature_watchdog()` at `0x344cc`.
Its BSS state is a zero timestamp (`0x58a38`), baseline temperature (`0x58a40`),
and separately tracked last actual PWM byte (`0x58a48`). Only an actually
delivered PWM of 255 starts or continues a timing window. A zero timestamp or
a rise strictly greater than 1°C replaces both timestamp and baseline. With no
such progress, an unsigned elapsed time strictly greater than 30000 ms faults.
Any output below 255 clears only the timestamp. The helper deliberately preserves
the stock zero-timestamp sentinel, including a first update at time zero, and
uses 32-bit unsigned subtraction across clock wraparound.

The helper returns a fault boolean. It does not turn off outputs, latch a fault,
or emit the stock stop reason. The recovered routine calls `disable_all_heaters`
and `stop(4 + 50 * hotend_index)` on timeout; the caller must integrate equivalent
immediate shutdown and latching before this becomes a thermal safeguard.

Explicit hardening rejects nonfinite temperatures/targets when enabling removal;
an invalid enable request still preserves the prior permission. Observing a
nonfinite temperature clears physical-removal permission. A nonfinite watchdog
temperature returns a fault even when the heater is off and clears its baseline.
Those additions are conservative choices for unknown sensor state, not results
claimed from the finite-input stock replay. The separate thermal controller must
continue to block heater output on invalid sensor values.

Run `python3 buildroot/tests/um3/run_interlock.py`. Its **79 checks** include
**12 removal request outcomes** and **19 watchdog updates** produced by actual
stock ARM instruction replay. The request routine needs no mocked calls. The
watchdog replay mocks only its external clock (`millis`, `0x39d34`) and shutdown
boundaries (`disable_all_heaters`, `0x35288`; `stop`, `0x27168`) so the binary
function can run without accessing real I/O. The oracle records captured stop
reason 4 for slot 0; the reconstructed boolean helper compares the fault event.
Remaining checks cover initial permission, exact 70/62°C boundaries, preserving
rejected requests, invalid sensors, strict progress/timeout thresholds, partial
PWM resets, zero-time startup, and wraparound. Auto-revoke is checked against the
disassembled branch contract rather than a replay of all of `manage_heater()`.

The native suite passes under AddressSanitizer/UndefinedBehaviorSanitizer and the
production helper compiles independently for ATmega2560 with AVR GCC 7.3, `-Os`,
and warnings as errors. This is isolated state-machine verification. It does not
qualify sample freshness, ADC or I²C communication, fault counters, physical
interlocks, power outputs, or shutdown wiring.

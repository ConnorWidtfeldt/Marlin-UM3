# Historical UM3 thermal-core reconstruction

This branch reconstructs a missing historical thermal implementation for a
linked comparison image. It is separate from the Marlin 2.1.2.9 port and is not
qualified for installation. The original UM3 tip
`4545818112027387f09e87f930117ab8f3804327` contains a UM2 temperature engine that
does not define the PID, power-budget, removal and accumulator interfaces used
by its newer main loop.

The replacement starts from GPL sibling S-line source
`f91240bb04e52818746a866bfe6e6af543209b79`, then removes or changes each observed
S-line divergence against the shipped stock UM3 artifacts. It does not import
the S-line thermal core unchanged.

## Evidence identity

- Stock `5.3.0.20211003` ARM simulator `usr/bin/jedi-marlin-simulator`, SHA-256
  `bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.
- Stock AVR `usr/share/griffin/griffin/machines/jedi.hex`, SHA-256
  `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
- Ordinary LLVM disassembly and direct Ghidra 12.2 decompilation of the ARM ELF.
  These are not REA Evidence IDs. The REA adapter rejected the installed Ghidra
  version; no provider Evidence IDs are invented here.
- Exact signed-table bytes and the hardware watchdog sequence were also checked
  in addressed AVR bytes parsed from Intel HEX. AVR addresses below are byte
  offsets in flash, not text-file offsets.

## Reconstructed behavior

| Area | Shipped evidence | Reconstruction |
| --- | --- | --- |
| Runtime objects | ARM `0x58844/0x588cc/0x58910/0x58964/0x589b8` | Two hotend PID objects, bed PID, external ADCs at I2C 0x48/0x49, power-budget object. |
| Startup permission | ARM data `0x5756c`, bytes `01 01` | Hotend removal/sensor-error suppression starts true for both slots; startup does not silently permit heating. |
| Removal request | ARM `0x3443c` | Reject invalid slot, require temperature <=70 and target exactly zero to enable, rejected requests preserve state; disabling is unconditional. |
| Heating progress | ARM `0x344cc` | Monitor last delivered PWM 255; refresh at >1C rise or timestamp zero; fault at unsigned elapsed >30000ms; otherwise clear timestamp when not full power. |
| Main thermal loop | ARM `0x345c8` | Poll ADCs, consume ready sample, update PID/request power, run watchdog, apply actual power, then update core presence/removal/fan state. |
| Hotend presence | ARM `0x347f0..0x3481c` | One-wire reset succeeds and signed raw value <2032*8. |
| Removal auto-revoke | ARM `0x3481e..0x34860` | Present core >=62C disables removal permission. |
| Shared fan | ARM `0x34862..0x3491e` | Any enabled core >40C forces 255; otherwise a core <35C or removal permission permits off; hysteresis retains prior output. |
| Combined heater setter | ARM `0x34938` | Original `setHeaterOutput(int8_t,uint8_t)` interface, negative index selects bed; hotend removal or stop-PWM forces zero, stopped bed permits only zero/255; accumulate delivered outputs. |
| Conversion | ARM `0x34a80/0x34bec/0x34c30` | Signed interpolation; low-end extrapolation, high-end clamp; board 4 bed uses table21, other beds table20. |
| Calibration | ARM `0x34c94` | ADC hotend result *8, board 4 bed result *8; apply factory gain and offset to Celsius values. Host owns calibration. |
| Thermal fault counters | ARM `0x34d9a..0x35030` | Compensated hotend 5/365C limits; bad present sample +10, absent overtemperature +4, trigger >100, normal decay1, removal resets. Bed >200C increments, trigger >20, normal decay1. |
| Initialization | ARM `0x35058` | ADC init, PID ranges hotend5..365C/bed5..200C, output direction, ADC input setup, OCR0B128 and compare-B interrupt enable. |
| Shutdown | ARM `0x351c0/0x35288/0x35350/0x35384/0x353b8` | Zero targets; lower output pins; board 4 bed clears software-PWM state; existing stock stop reasons remain authoritative. |
| Board 4 bed PWM | ARM `0x353c8` | Scale /2 in combined setter; count initially1, latched PWM initially0; latch at count0, high when nonzero, low when latched PWM <=count; wrap `(count+1)&127`. |
| Sampling ISR | ARM `0x354e4` | Call software-PWM helper, four-state acquisition cycle after initial state5, publish after 8 samples, preserve pending raw values until consumed, reset accumulators. |
| MCU watchdog | AVR `0x11758/0x11770` | Unconditional four-second reset watchdog and `wdr`; ARM simulator routines are no-ops and cannot establish this hardware behavior. |

The reconstructed table20 is byte-identical to AVR flash `0x1e78`, 116 bytes,
SHA-256 `f6e1e186f15bb3febdda4ba7a2b924c5654f8e9a78dd0ebfd5b622df59e62808`.
Table21 is byte-identical to AVR `0x1e44`, 52 bytes, SHA-256
`e2e0c9e105f891ad1f11167f49e9a4776a8492fedcf5bdf32a26b7e34c33c691`.
Both use oversampling factor8. The timer cycle is 4*8*1.024ms =32.768ms at 16MHz;
the sibling's inherited 128ms source comment was corrected.

The AVR watchdog initializer loads 0x28, opens the WDTCSR timed write window with
0x18, then writes 0x28 (`WDE|WDP3`, four-second hardware reset) at AVR flash
`0x11758..0x1176e`. The reset routine is `wdr; ret` at `0x11770..0x11772`.
No empty watchdog or placeholder heater implementation is supplied.

Additional independent AVR anchors were recovered while preparing the linked
comparison:

| AVR flash range | Corroborated behavior |
| --- | --- |
| `0x10332..0x1039e` | Removal setter: slot<2, <=70C, target0, rejected request preserves flag. |
| `0x103a0..0x1048c` | Thermal init: hotend5/365C and bed5/200C ranges, PE4/PE5 hotend outputs, PG5 standard bed, board 4 DDRH6, ADC setup, OCR0B128 and TIMSK0 compare-B bit2. |
| `0x102d2..0x10330` | Specialized bed-output helper: stop-PWM accepts only255, board 4 divides output by2, other boards use analogWrite pin4, accumulates unscaled output. |
| `0x1048e..0x104e6` | Bed/all-heater target zeroing and output pin shutdown; board 4 clears the software bed PWM byte. |
| `0x104e8` | Heating-progress watchdog entry; last-output255 check and float1C threshold are visible before elapsed-time comparison. |
| `0x10904..0x10b7c` | Main-loop tail: watchdog-reset call, PID/power processing without S-line log/skip, delivered-output suppression, raw-presence threshold0x3f80, 62C auto-revoke and40/35C fan literals. |
| `0x10b7e..0x10bd2` | Board 4 software PWM: actual PORTH at data address0x102 bit6, latched PWM, count mask0x7f. PH6 is Mega digital pin9. |
| `0x10bd4..0x10d5c` | Timer0 compare-B ISR, reached by vector-table entry at0x58: ADCSRB8, ADMUX0x42/41/40, publish at count>=8, ready handshake and sum resets. |

The interpolation candidate at AVR `0x10150..0x102d0` has the same signed table
search, `lpm` access, floating interpolation and last-row clamp. The linked-image
comparison should align its generated code before assigning exact source parity.
The stock AVR power object is at SRAM0x1623, with bed state starting0x1650;
hotend PID update is called at flash0x10e12 from the main-loop tail. These are
anchors for further comparison, not imported AVR struct offsets.

## S-line differences deliberately removed

- Board V4 and timer2 bed hardware PWM paths are absent in the shipped UM3.
- S-line's split nozzle/bed setters are replaced by the stock combined setter.
- S-line's `prev_temp_error_cnt` logging/PID-skip behavior is absent in stock
  `manage_heater`; the reconstructed loop always updates the request unless
  stop-PWM is active and always runs the watchdog.
- S-line divides bed PWM later, counts 127 ticks and stores a different initial
  off threshold. The stock setter division and 128-tick cycle are retained.
- Additional unneeded thermistor tables and UM2 autotune/target APIs are removed;
  the actual UM3 main owns runtime PID targets, gains, power parameters and
  calibration. No stale compiled UM2 PID default is substituted for host setup.

## Remaining differences and verification limits

The control evidence includes both the ARM simulator and the explicit AVR
anchors above. The complete reconstructed AVR functions must still be
aligned and compared after linking the historical image. This is semantic
reconstruction, not original source recovery or an assertion of byte equality.

The factored `temperature_error_checking` helper preserves the observed ordering
but is a source organization choice: the shipped ARM fault checking is part of
`updateTemperaturesFromRawValues`. Compiler versions, function inlining, global
layout and AVR/ARM floating-point widths may change generated code. ARM double
is 64-bit and AVR double is 32-bit; the legacy PID files retain vendor arithmetic.

The shipped ARM symbol table marks the combined heater setter local (`t`) and
software PWM global (`T`). The reconstruction preserves that linkage; no external
file currently calls the private heater setter.

Pin/timer ownership and the actual stock board revision remain separate evidence
requirements. Board 4's additional bed pin and output path are retained because
the shipped binary contains them; this does not add S-line BOARD_V4 hardware.
The legacy code preserves stock finite-input semantics and does not import the
modern port's additional NaN hardening into a comparison-image reconstruction.

The historical compile/link is run by the main build task. An isolated native
AVR GCC 7.3 check compiles both `temperature.cpp` and `watchdog.cpp` with the
historical packing/unsigned/section flags and Arduino 1.0.5 headers. The remaining
warnings concern the existing UART macro's `defined` expansion and intentional
ISR fallthrough; no thermal compile errors remain. Final core build and linked
comparison results must be recorded by that task. No heater, printer, bootloader
or flashing procedure has been exercised by this reconstruction.

## Linked stock-parity investigation

The clean historical build at source `f0d92f0b66833e9afe10262fbbc60371b780bcd8`
uses AVR GCC 4.3.2 and Arduino 1.0.5. The following results distinguish actual
instruction comparison from function-size coincidences:

- `watchdog_init()` at reconstructed ELF0x16b72 has all24 bytes identical to
  stock AVR0x11758. `watchdog_reset()` at ELF0x16b8a has all4 bytes identical to
  stock0x11770. Neither routine contains relocation-dependent addresses; these
  are direct whole-body byte comparisons, including WDTCSR register writes.
- GCC 4.3.2 emits different code for most thermal routines: removal setter0x80
  versus stock0x6e, interpolation0x198 versus stock0x182, software PWM0x4e
  versus stock0x56. Those mismatches are not attributed to source bugs solely
  from size or instruction-layout differences.
- Compiling the current software-PWM source with local AVR GCC 7.3 produces
  0x56 bytes. Every one of its8 SRAM-reference operands maps to the independently
  identified stock variable (Board ID0x020b, count0x0296, requested PWM0x1600,
  latched PWM0x15ea). Resolving the object's local branch relocations and comparing
  the remaining instruction bytes finds a single differing byte at function+0x1a.
  GCC 7.3 branches directly to the low-write block(+0x38) for zero PWM; stock
  branches through the compare block(+0x2c). Both paths perform the same low
  write in this state. This is consistent with compiler branch threading and is
  not yet evidence for changing recovered source semantics.
- The local GCC 7.3/avr-libc watchdog macro emits22 bytes instead of24, changing
  register allocation and dropping one setup instruction. The original GCC 4.3
  build's macro already matches stock exactly. Compiler experiments must record
  the avr-libc headers/runtime as well as the compiler version.

Those compiler-dependent differences alone did not justify source changes.
The subsequent calibrated comparison below identified concrete reconstruction
differences. Function evidence does not independently establish that the entire
linked HEX payload is identical.

## Calibrated thermal instruction parity

The matching profile is AVR GCC 4.8.1 (`1:4.8.1+Atmel3.4.4-2`), binutils 2.24
(`2.24+Atmel3.4.4-1`), avr-libc 1.8.0 (`1:1.8.0+Atmel3.4.4-1`), Arduino 1.0.5,
and the original C++ linker driver. Its linked image uses the stock thermal
addresses and static object layout. The original C++ linker driver also restores
the stock runtime-library ordering; changing to the C driver relocates float
helper functions despite otherwise matching control instructions.

Four source forms introduced during reconstruction required restoration:

1. The requested-output update now uses explicit stop-PWM `if/else` arms rather
   than a ternary expression inside the setter call. The latter compiled 10 bytes
   shorter and changed the loop's register use. The explicit arms match stock.
2. Hotend removal and stop-PWM checks are two consecutive `if` statements. A
   combined `||` changed the observed short-circuit instruction sequence.
3. Bed-output initialization uses `if(board!=4) standard_output; else XL_output`.
   The sibling's switch placed the output blocks in a different order.
4. The bed setter uses the same standard-board-first condition order before
   selecting `analogWrite` or the board 4 software-PWM byte.

These preserve the recovered behavior and yield ordinary compiler output. No
inline instruction padding, binary patches, fixed-address output sections, or
stub routines are used to force a match. Source variants were compiled in an
isolated network-disabled container with the repository mounted read-only; the
tested variant differs from the final source only by an evidence comment.

| Stock routine | AVR start | Body bytes | Result |
| --- | --- | --- | --- |
| `analog2temp_sub` | `0x10150` |386| Exact instructions, constants and callee identities. |
| Bed specialization of `setHeaterOutput` | `0x102d2` |96| Exact after restoring standard-board-first condition. |
| `setAllowHotendRemoval` | `0x10332` |110| Exact. |
| `temperatureInit` | `0x103a0` |238| Exact after restoring standard-board-first condition. |
| `disable_bed` | `0x1048e` |50| Exact. |
| `disable_all_heaters` | `0x104c0` |40| Exact. |
| `nozzle_temperature_watchdog` | `0x104e8` |240| Exact, including strict threshold comparisons and shutdown callees. |
| `manage_heater` | `0x105d8` |1446| Exact after restoring explicit request arms and consecutive output checks. |
| `soft_pwm` | `0x10b7e` |86| Exact. |
| Timer0 compare-B ISR | `0x10bd4` |394| Exact. |
| `watchdog_init` | `0x11758` |24| Exact. |
| `watchdog_reset` | `0x11770` |4| Exact. |

For the 10 thermal routines, the final isolated object was checked against all
stock instruction bytes after resolving its 310 relocation operands to identified
stock globals, local targets and called functions. Constants and callee identities
were checked, not discarded by an address mask. The rebuilt `stock-match-a` ELF
then established direct raw-byte equality, at identical addresses with no
translation, for every routine above except the still-unapplied bed condition at
that build's snapshot. Its only thermal mismatch was 18 bytes at
`0x102f4..0x10305`; the isolated 96-byte bed variant proved the correction and was
then applied for the next complete build. Full-image acceptance remains owned by
the complete payload comparison rather than these function-level checks.

The final `stock-exact-a` complete build then matched all 77942 addressed flash
bytes and the literal stock HEX file, SHA-256
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
Every thermal routine above matches raw stock bytes at the same addresses,
including the applied bed correction. See [binary-equivalence.md](binary-equivalence.md)
for the complete-image gate and its limits.

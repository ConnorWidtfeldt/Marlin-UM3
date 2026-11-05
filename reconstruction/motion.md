# Reconstructing the stock UM3 motion core

This branch repairs the incomplete historical UM3 source for comparison with the
shipped firmware. It is an experimental reconstruction, not a flashable release
or a claim to have recovered the original source. No motor or printer hardware
was exercised during this work.

## Sources and analysis method

The starting UM3 source is
`4545818112027387f09e87f930117ab8f3804327`. The complete planner and stepper in
the published sibling S-line revision
`f91240bb04e52818746a866bfe6e6af543209b79` supplied candidate source. Candidate
behavior was compared with the symbol-bearing ARM simulator shipped in UM3
firmware `5.3.0.20211003`, AVR package version `2019.01.27`:

- `usr/bin/jedi-marlin-simulator`, SHA-256
  `bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.
- `usr/share/griffin/griffin/machines/jedi.hex`, SHA-256
  `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.

The supporting evidence is direct Ghidra 12.2 static decompilation and LLVM
symbol/disassembly inspection, not REA Evidence records. The REA adapter could
not use the locally installed Ghidra, so direct analysis was recorded separately.
ARM addresses below are ELF virtual addresses; they are not AVR flash offsets.

The candidate file SHA-256 values before adaptation are:

| Candidate file | SHA-256 |
| --- | --- |
| `planner.cpp` | `e0c6bc2efcdddd22cfcdb2dc9a1871a3072abc50772330f828fd39b0cdd291bb` |
| `planner.h` | `a2db5c568c0e7dd66f6d01a032c6e05325bc9edeca557746c67eb61be6747b09` |
| `stepper.cpp` | `53d64d237a0f572345d9a3d6eb122d408fc8bd0232d80db59155979c98ead506` |
| `stepper.h` | `f770792b547bcbaa836003b44c56c01c648eb20943567d18ba9eb83e7fe80045` |

## Implemented shared behavior and corroboration

| Behavior | Shipped ARM evidence | Reconstruction |
| --- | --- | --- |
| Planner initialization | `plan_init`, `0x2f9bc`: clear ring/position/history and call `reset_acceleration_rates` | Initialize all motion defaults at definition and calculate step accelerations at startup. |
| Ring capacity | `plan_buf_free_positions`, `0x31074`: `15 - ((head-tail)&15)` | Implement the host-used free-slot API and preserve the 16-entry ring with one reserved slot. The unused stale `movesplanned()` API is absent from the final reconstruction. |
| Planner full wait | `plan_buffer_line`, `0x2fb08` | Service heaters and inactivity while waiting for room. Remove obsolete local LCD/lifetime dependencies. |
| Tool coordinates | `plan_buffer_line`, `0x2fb08`; `plan_set_position`, `0x30e7c` | Round runtime XYZ millimeters to steps, then subtract selected tool's integer XYZ offsets. E is truncated to steps and receives no tool offset. Remove stale volumetric conversion. |
| Extrusion multiplier | `plan_buffer_line`, `0x2fb08`: global scalar, divide step count by 100 | Use UM3's scalar `extrudemultiply`, matching the retained command dispatcher. |
| Tiny moves | `plan_buffer_line`: threshold 5, but preserve nonzero Z moves | Use `MINIMAL_STEPS_FOR_PLANNED_MOVE=5`; retain the shipped behavior rather than discarding Z probing steps. |
| Planner acceleration and junctions | `plan_buffer_line`, `0x2fb08`: per-axis limits, slowdown, XYZ/E jerk, previous speed update | Restore the full shared look-ahead/trapezoid core, not a forwarding stub. |
| Cooling fan synchronization | `check_axes_activity`, `0x2fa34`: tail-block fan or target fan; 100 ms kickstart; `setCoolingFanSpeed` call at `0x2fafc` | Restore UM3 fan driver call and motion-buffered fan updates; no unrelated idle-axis disabling loop. |
| Runtime motor direction | `invert_direction[5]` at `0x57554`, consumed by Timer 1 ISR `0x314c8`; host `M12004` handler | Restore runtime direction array and E0/E1 selection instead of fixed UM2 compile-time direction macros. |
| Endstops and stepping | Timer 1 ISR `__vector_17`, `0x314c8` | Keep full Bresenham execution, two-consecutive-read endstop handling, heater-independent position counting, and serial receive polling during steps. |
| ISR rates | ISR: 40000 cap at `0x3158e`; 20000/10000 thresholds immediately after | Preserve table-based timing and 1/2/4 steps per interrupt. |
| Late ISR correction | `0x32f4c–0x32fe6`: compare OCR1A with TCNT1+16, move compare into future if late | Retain shared correction rather than allowing the timer to wrap. |
| Position changes | `st_set_position`, `0x3384c`; `st_set_e_position`, `0x338d8` | Synchronize outstanding moves, then atomically update executed step counters. Preserve original public `long` reference API. |
| Quick stop | `quickStop`, `0x33ae0`: drain ring, clear current block, reconstruct position, call planner setter | Preserve `void` UM3 API. Logical XYZ is `(executed_steps + selected_tool_offset)/steps_per_unit`; E has no offset or volumetric divisor. |
| Separate interrupt startup | `st_enable_interrupt`, `0x33ca0`: CTC mode, divide-by-8 Timer 1, OCR1A=0x4000, TCNT1=0, enable, `sei` | Restore the missing function and keep interrupt enabling separate from pin initialization. |

The candidate's planner statistics, retracted-length tracking, additional
`disable_all_steppers` API, and quick-stop extruder return value have no matching
symbols or corresponding code in this shipped UM3 build. They were removed from
the adaptation. Actual UM3 ring completion simply advances the tail at
`0x33040–0x3305e`; it does not update S-line statistics.

## Arc bounds and the stale software-clamp call

The retained UM3 `motion_control.cpp` calls an undefined
`clamp_to_software_endstops` for intermediate arc points. Adding a guessed
implementation would introduce behavior absent from the shipped motion code.
The sibling `motion_control.cpp` has the same arc algorithm with that call
absent. Inspection of the entire shipped `mc_arc` function, ARM
`0x2e720–0x2ebf0` (symbol size `0x4d0`), independently establishes that it does
not reference `min_pos`, `max_pos`, or a clamp helper, and contains no coordinate
bounds checks. It updates interpolated XYZE at `0x2eafa–0x2eb6c`, passes the
result directly to `plan_buffer_line` at `0x2eb96`, and passes the final target
directly at `0x2ebd0`. The reconstruction therefore removes the stale call,
without adding a no-op helper or disabling arc support.

The actual UM3 endpoint safety check remains in `Marlin_main.cpp`:
`get_coordinates()` rejects a specified XYZ endpoint outside runtime
`min_pos - ALLOWED_POSITION_ERROR` through `max_pos + ALLOWED_POSITION_ERROR`,
calls `stop(STOP_REASON_POSITION_ERROR)`, and restores the previous coordinate
for that axis. G2/G3 check `!isStopped()` before parsing coordinates; there is
no second stop check between `get_arc_coordinates()` and `prepare_arc_move()`.
Consequently this pre-command gate must not be described as proof that a newly
raised position error prevents arc generation. These checks cover the parsed
endpoint, not every intermediate point of an arc.
An arc can leave the permitted region despite having endpoints inside it; that
is a limitation of the recovered stock behavior, and requires separate review
before any future modern-port policy is chosen.

## M18/M84 source-form reconstruction

After compiler and library-link calibration, the remaining `loop()` instruction
difference was traced to the shared M18/M84 handler, rather than a changed set or
order of M-code cases. The stock all-axis disable block is at AVR
`0x7370–0x738c`: synchronize, disable E0/E1, finish/disable all steppers, then
branch to the common command completion at `0x7100`. The first reconstructed
form placed those same actions at `0x5914–0x5932`, with command completion at
`0x711c`. The stock E-axis predicate at `0x590c` branches to the selected-axis
path at `0x732e` or jumps to the all-axis block. The original reconstruction's
corresponding predicate was at `0x590e` and fell through to the all-axis block.

A comparison of complete basic-block instruction bodies found only this moved
disable block and its dependent branch-range changes. For example, the M206
handler-dispatch branch at `0x4ee2` became a four-byte `jmp` instead of the
stock two-byte `rjmp` as the extra block moved its target. The flash region
`0x21a–0x396` contains linker-generated four-byte JMP trampolines for many
functions; it is not an M-code switch jump table.

The reconstruction now expresses the handler with a positive `any_axis`
predicate, selected-axis actions in the first arm, and all-axis actions in the
`else` arm. It preserves the exact X/Y/Z/E `code_seen` short-circuit order,
actions, and conditional compilation. This is an equivalent C++ source form
guided by observed stock control flow, not a claim to have recovered the original
spelling. Compiling this form with the calibrated toolchain reproduces **every
byte of the entire `loop()` function**, AVR `0x4128–0x73c9` (12,962 bytes), with
SHA-256 `4da78e7bd260b3ad9522344f4884664a8e462fd55fc8a38497f13bc3172ec806`.

## Defaults and host ownership

The simulator's named default arrays are also present as unique exact packed
float sequences in checksum-validated `jedi.hex`: steps/mm `(80,80,200,369)` at
AVR flash `0x12cbb`, homing speeds `(6000,6000,2400,0)` at `0x12c7f`, maximum speeds
`(300,300,40,45)` at `0x12ccb`, and maximum XYZ `(230,230,240)` at `0x12c6f`.
These byte addresses include the Intel HEX extended linear address base.
These are compiled defaults, not final Extended calibration. The stock host
supplies `(80,80,400,369)` steps/mm and Extended bounds `(215,215,310)`, plus
individual machine and print-core calibration. It also supplies motor direction
and current settings. The planner stores runtime values and introduces no EEPROM
ownership or new calibration defaults.

## Validation and exact-image result

### Stock AVR instruction comparison

The final `artifacts/stock-exact-a/equivalence.json` comparison verifies the
**entire firmware image as exactly identical**: all 77,942 addressed bytes,
address ranges, entry records, and the literal Intel HEX file agree with stock.
There are zero changed, missing, or extra addresses. Both HEX files have SHA-256
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
This includes every motion routine, the entire main loop, all linked call
targets, and linker-generated trampolines. No byte patch, padding, operand mask,
comparison normalization, or stock-byte substitution was used.

The final linker discrepancy was traced to an unused `movesplanned()` wrapper
and declaration initially retained from the incomplete UM2-era planner API.
The host and retained UM3 dispatcher use `plan_buf_free_positions()` instead;
the packaged symbol-bearing simulator has no `movesplanned` symbol. Although
linker garbage collection discarded the unused wrapper's 14-byte function
section, that extra input section shifted BFD section IDs and changed the
ordering of its generated trampoline hash table. Removing this unsupported dead
API reproduced the stock trampoline order, resolving the final 172 differing
bytes. This was source cleanup backed by symbol/caller and linker evidence,
rather than an artificial change to the output binary.

The initial calibrated AVR GCC **4.8.1 / Atmel 3.4.4** comparison verified the
reconstructed motion implementation at instruction level: all **30 routines,
14,332 bytes** have exactly the stock addresses, lengths, opcode bytes, relative
branches, immediate constants, SRAM addresses, and indexed field offsets. The
only differing bytes in these routines at that stage were absolute `CALL/JMP` target
operands pointing to relocated mathematical-library functions. Their call
identities and order agree under one consistent callee mapping. This stricter
check does **not** mask register operands, data addresses, numeric constants, or
relative branches.

Nineteen routines at that stage were already wholly raw-byte identical, including the complete
**3694-byte Timer 1 ISR**, stepper initialization, synchronization, executed
position setters/getters, interrupt control, fan activity handling, planner
initialization/free-space reporting, forward/reverse passes, and endstop flags.
The other eleven differed only in the absolute library target operands described
above. No change to the motion algorithms was warranted by that calibrated pass.
The subsequent source-form, linker, and unused-API corrections produced the final
whole-image result recorded above.

The earlier comparisons below retain the diagnostic evidence showing why the
compiler had to be calibrated before assigning differences to source.

The first complete comparison uses reconstructed commit
`f0d92f0b66833e9afe10262fbbc60371b780bcd8`, the unmodified `artifacts/clean-a`
AVR GCC 4.3.2 output, and the stock AVR image identified above. The addresses in
this table are **AVR byte addresses**, independently located through the stock
AVR decompilation and call graph. Ghidra's `code:` function addresses are word
addresses and must be doubled; function byte sizes must not be doubled.

| Routine | Stock AVR address, bytes | `clean-a` address, bytes | Comparison |
| --- | --- | --- | --- |
| `mc_arc` | `0xc494`, 1562 | `0x11126`, 1782 | Arc segmentation and direct planner call topology agree; substantial arithmetic/register allocation differences remain. |
| `calculate_trapezoid_for_block` | `0xcd5a`, 938 | `0x121d2`, 984 | Not byte-equivalent; numerical/control-flow equivalence is not established by size. |
| `planner_reverse_pass_kernel` | `0xd104`, 244 | `0x11ed6`, 258 | Not byte-equivalent. |
| `planner_reverse_pass` | `0xd1f8`, 124 | `0x11fd8`, 140 | Not byte-equivalent. |
| `planner_forward_pass_kernel` | `0xd274`, 234 | `0x11d7e`, 244 | Not byte-equivalent. |
| `planner_forward_pass` | `0xd35e`, 94 | `0x11e72`, 100 | Not byte-equivalent. |
| `planner_recalculate_trapezoids` | `0xd3bc`, 274 | `0x125aa`, 304 | Not byte-equivalent. |
| `planner_recalculate` | `0xd4ce`, 12 | `0x126da`, 14 | Both invoke reverse, forward, and trapezoid passes; tail-call generation differs. |
| `check_axes_activity` | `0xd4da`, 156 | `0x12064`, 170 | Tail fan selection and 100 ms kickstart agree; register/call allocation differs. |
| `plan_buffer_line` | `0xd576`, 4952 | `0x126e8`, 6026 | Not byte-equivalent; requires a calibrated compiler comparison and further instruction tracing. |
| `plan_set_position` | `0xe8ce`, 460 | `0x11b76`, 520 | XYZ rounding and offset subtraction, E truncation, and stepper setter call agree; code generation differs. |
| `plan_set_e_position` | `0xea9a`, 58 | `0x11b3a`, 60 | E multiplication/truncation and setter call agree; code generation differs. |
| `plan_buf_free_positions` | `0xead4`, 16 | `0x11ac2`, 16 | Identical opcode/operand stream after identifying the relocated head/tail SRAM addresses. |
| `set_extrude_min_temp` | `0xeae4`, 18 | `0x11ad2`, 18 | Identical instructions except relocated SRAM addresses. |
| `reset_acceleration_rates` | `0xeaf6`, 144 | `0x11ae4`, 86 | Different loop/inlining decisions; no byte-equivalence claim. |
| `plan_init` | `0xeb86`, 106 | `0x1210e`, 196 | Stock calls acceleration reset; GCC 4.3.2 inlines it in this build. |
| `checkHitEndstops` | `0xebf0`, 280 | `0x1512e`, 310 | Not byte-equivalent. |
| `isEndstopHit` | `0xed08`, 26 | `0x13e72`, 26 | Same three-flag boolean result; instruction ordering differs. |
| `endstops_hit_on_purpose` | `0xed22`, 14 | `0x13e8c`, 14 | Identical instructions except relocated hit-flag SRAM addresses. |
| `enable_endstops` | `0xed30`, 6 | `0x13e9a`, 6 | Identical instructions except relocated enable-flag SRAM address. |
| `st_wake_up` | `0xed36`, 12 | `0x13ea0`, 12 | **Raw bytes identical.** |
| Timer 1 ISR | `0xed42`, 3694 | `0x1428c`, 3746 | Not byte-equivalent; compiler allocation and timing still need separate qualification. |
| `st_init` | `0xfbb0`, 164 | `0x13eac`, 168 | All GPIO/register actions align; reconstructed code reloads the Timer 3 register pointer with two extra `ldi` instructions. |
| `st_synchronize` | `0xfc54`, 24 | `0x1418a`, 24 | Same head/tail loop and calls to the identified heater/inactivity functions, with relocated operands. |
| `st_set_position` | `0xfc6c`, 154 | `0x141f0`, 156 | Same synchronize/atomic four-counter update; register allocation differs. |
| `st_set_e_position` | `0xfd06`, 46 | `0x141c0`, 48 | Same synchronize/atomic E-counter update; register allocation differs. |
| `st_get_position` | `0xfd34`, 28 | `0x13f54`, 36 | Same indexed atomic four-byte counter read; address/register calculations differ. |
| `finishAndDisableSteppers` | `0xfd50`, 30 | `0x141a2`, 30 | First call target relocates; following **26 GPIO bytes identical**. |
| `quickStop` | `0xfd6e`, 376 | `0x13fce`, 444 | Ring drain, selected XYZ offsets, unoffset E, and planner setter call agree; arithmetic/register allocation differs. |
| `st_enable_interrupt` | `0xfee6`, 86 | `0x13f78`, 86 | **Raw bytes identical.** |

Raw-byte SHA-256 checks, with no opcode or operand normalization:

- Entire `st_enable_interrupt`, 86 bytes:
  `c9f1b541c61684258fedc2042995c0c51d857579bcae08ca6474fc94b5dfecfd`.
- Entire `st_wake_up`, 12 bytes:
  `e3d05e338964b20b1fe7c76527fe43af0140b54e9fb6074e1a35171845f6e5d8`.
- GPIO tail of `finishAndDisableSteppers`, stock `0xfd54`, 26 bytes:
  `ec83ad8b544e3bd2dd988febe34b660bf3a8c97ad653097a445c300bb5148b49`.

The stock AVR planner's block stride is `0x4d` (77 bytes), independently agreeing
with the reconstructed AVR field layout and its 16-entry ring. This confirms an
AVR layout fact rather than copying the ARM simulator's 84-byte padded layout.
Matching after operand normalization is only a comparison aid: it is not a proof
that every masked data or call address refers to the correct object. The matched
small functions above also had their object/call identities inspected. No motion
source changes were justified by the initial compiler-mismatched pass; the
remaining large routines needed comparison under the calibrated stock compiler
before differences could be assigned to source rather than code generation.

An unchanged-source AVR GCC 4.9.2 / Atmel 3.5.3 candidate substantially reduces
the discrepancy. Its trapezoid calculation, endstop reporting, and E-position
setter have the same complete opcode/operand sequence after relocation
identification. More broadly, **all 30 motion routines listed above have the
same ordered sequence of external calls** under one consistent mapping of stock
and reconstructed callees. This includes 135 calls in `plan_buffer_line`, 50 in
`mc_arc`, 39 in trapezoid calculation, four in the Timer 1 ISR, and 13 in
`quickStop`. The planner calls include 25 float multiplications, 20 float
divisions, five rounds, five squares, and three square roots, alongside the same
heater/inactivity wait, extrusion diagnostics, and final planner calls. This
checks operations and call topology rather than inferring behavior from sizes.

GCC 4.9.2 still produces 5350 bytes for `plan_buffer_line` versus 4952 stock,
3732 versus 3694 for the ISR, and 408 versus 460 for `plan_set_position`.
Instruction inspection shows repeated stock `lds/sts` accesses versus cached
pointer `ld/std` accesses in the candidate, different register allocation, and
different placement of loop bodies. The inspected motion data pointers differ
by a consistent relocation of ten bytes, including steps/mm, tool offsets,
planner head/tail, planned position, executed position, and current block.
No out-of-place motion object or additional/omitted callee was established in
that pass. These findings narrowed the work to compiler calibration and deeper
instruction/branch comparison; they did not justify editing source merely to
match a function size. The later raw-byte result above resolves those motion
routine differences.

`git diff --check` passed for the reconstructed motion files. Each of
`planner.cpp`, `stepper.cpp`, and `motion_control.cpp` compiled independently for ATmega2560 at 16 MHz
using AVR GCC 7.3, Arduino 1.0.5 core/variant headers, and the historical packing,
enum, and section flags. The integrated historical AVR build is run separately
and recorded in the branch's build report.
The modern branch's small contract tests cover ring credits, M218 signs/ties, and
stopped coordinates; they do not exercise this historical ISR or complete
planner. Compilation establishes that the restored implementation is connected,
not that it is safe to flash.

The calibrated comparison now corroborates the reconstructed look-ahead,
trapezoid, AVR multiplication, pin-pulse paths, and their exact linked call
targets. The whole-image gate above also verifies the final linker-generated
trampoline bytes. ARM register simulation alone does not establish AVR interrupt
latency or electrical timing; the AVR comparison above supersedes it for the
owned instruction paths. Hardware qualification remains separate, especially
when transferring these behaviors to modern Marlin: queue saturation and
cancellation under interrupts, minimum-step and zero-length edge cases, runtime
direction/offset changes, stuck or missing endstops, and actual homing/probing.

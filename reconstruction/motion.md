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
| Ring capacity | `plan_buf_free_positions`, `0x31074`: `15 - ((head-tail)&15)` | Implement host-used free-slot API, preserve 16-entry ring with one reserved slot, retain historical `movesplanned()` API. |
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

## Validation and remaining differences

`git diff --check` passed for the reconstructed motion files. Each of
`planner.cpp`, `stepper.cpp`, and `motion_control.cpp` compiled independently for ATmega2560 at 16 MHz
using AVR GCC 7.3, Arduino 1.0.5 core/variant headers, and the historical packing,
enum, and section flags. The integrated historical AVR build is run separately
and recorded in the branch's build report.
The modern branch's small contract tests cover ring credits, M218 signs/ties, and
stopped coordinates; they do not exercise this historical ISR or complete
planner. Compilation establishes that the restored implementation is connected,
not that it is safe to flash.

The shared look-ahead/trapezoid source, AVR multiplication assembly, speed tables,
and pin pulse execution remain candidate implementations where full
instruction-by-instruction equivalence has not been established. ARM simulator
instructions corroborate the operations above; ARM register simulation does not
establish AVR interrupt latency or electrical timing. ARM and AVR also differ in
`double` precision and ABI layout. Outstanding qualification includes comparing
planned blocks and executed counters with stock AVR, queue saturation/cancellation
under interrupts, minimum-step and zero-length edge cases, runtime direction and
offset changes, stuck/missing endstops, and actual homing/probing behavior.

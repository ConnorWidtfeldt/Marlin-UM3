# Motion and probe contracts recovered from shipped artifacts

These findings support reconstruction, not hardware acceptance. They come from
LLVM symbol/disassembly inspection, direct byte extraction, and direct Ghidra
12.2 static decompilation. They are **not REA Evidence records**. The REA adapter
could not use the locally installed Ghidra; the direct export is recorded as a
separate static analysis method. Decompiled text is not the original source.

## Artifact identity and scope

- Stock package: UM3 `5.3.0.20211003`; packaged AVR version `2019.01.27`.
- `usr/bin/jedi-marlin-simulator`, SHA-256
  `bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.
- `usr/share/griffin/griffin/machines/jedi.hex`, SHA-256
  `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
- Published UM3 revision: `4545818112027387f09e87f930117ab8f3804327`.
- Sibling S-line comparison: `f91240bb04e52818746a866bfe6e6af543209b79`.

All ARM addresses below are ELF virtual addresses. The symbol-bearing simulator
retains the relevant planner, probe, and global names. Its instructions establish
behavior in that build. Except for the matching data arrays identified below,
they do not independently establish AVR timing or identical floating-point
results. ARM uses 64-bit `double`; this AVR toolchain uses 32-bit `double`.

## Planner, offsets, and stopped coordinates

| Routine | ARM address | Recovered behavior |
| --- | --- | --- |
| `plan_buffer_line(...)` | `0x2fb08`, size `0x1374` | XYZ millimeters multiplied by runtime steps/mm, rounded by `lround`, then selected-tool integer offset subtracted. E is converted without tool offset. Full-ring wait services heaters and inactivity handling. |
| `plan_set_position(...)` | `0x30e7c`, size `0x1b8` | Same XYZ conversion; E truncates toward zero. Updates stepper position, then resets previous nominal/axis speeds. |
| `plan_buf_free_positions()` | `0x31074`, size `0x36` | `15 - (uint8_t(head-tail) & 15)`. Sixteen ring entries reserve one slot. |
| `quickStop()` | `0x33ae0`, size `0x1c0` | Disables stepper interrupt, drains queued blocks, clears current block, reenables interrupt, reads executed steps, reconstructs logical position, then calls `plan_set_position`. |
| M218 handler inside `process_command` | X: `0x23e9c–0x23ecc`; Y: `0x23ee8–0x23f1a`; Z: `0x23f36–0x23f68` | Stores `-lround(mm * runtime_steps_per_mm)` in integer tool offsets. |

M218's negative sign is significant: a positive physical nozzle displacement
becomes a negative stored offset. `lround` has half-away-from-zero semantics,
including negative half values. XYZ conversion computes a float product before
the call. This agrees with the published UM3 M218 code, rather than interpreting
the old offset array as millimeters.

After a stop, logical XYZ is
`(executed_physical_steps + selected_tool_offset_steps) / runtime_steps_per_mm`.
Logical E is `executed_E_steps / runtime_E_steps_per_mm`; there is no E tool
offset. Reconstructing from the last planned endpoint loses the actual stopped
position. This helper alone does not drain the planner or stop an interrupt.

The production `motion_contract` module implements the free-slot formula,
M218 conversion, and stopped-position conversion. It rejects nonfinite input,
nonpositive calibration, and integer overflow without partially modifying output.
Those rejection checks are deliberate validation at the new API boundary; they
are not claimed as behavior recovered from the old binary. The remaining
planner and interrupt behavior requires integration with modern Marlin.

## Compiled defaults are not Extended calibration

| Named ARM data | ARM address | Compiled values | Unique matching AVR flash offset |
| --- | --- | --- | --- |
| `axis_steps_per_unit` | `0x5751c` | X=80, Y=80, Z=200, E=369 steps/mm | `0x12cbb` |
| `homing_feedrate` | `0x574c8` | 6000, 6000, 2400, 0 mm/min | `0x12c7f` |
| `max_feedrate` | `0x5750c` | 300, 300, 40, 45 mm/s | `0x12ccb` |
| `max_pos` | `0x574e0` | 230, 230, 240 mm | `0x12c6f` |

The AVR observations are exact packed little-endian float sequences in a
checksum-validated Intel HEX image. They independently corroborate these arrays,
not every instruction in the ARM build. `STOCK_*` arrays preserve that distinction.
The offsets were corrected after independently resolving Intel HEX extended
addresses; the former references omitted `0x10000`. The array values are unchanged.

Griffin's `machines/um3.json` supplies Z=400 steps/mm and `9511.json` supplies
Extended movement bounds 215×215×310 mm. `applicationLayer.py` emits runtime
`M92`, `M12000/M12001`, and individual calibration values. Installing the ROM
defaults as final Extended calibration would therefore be wrong.

Other observed defaults are acceleration and retract acceleration 3000 mm/s²,
XY/Z/E jerk 20/0.4/5 mm/s, and homing retract 7 mm per XYZ axis. The simulator's
`invert_direction[5]` bytes at `0x57554` are `1,0,1,1,0`; its `home_dir[3]` at
`0x40b34` is `-1,+1,+1`. These arrays do not establish board connector polarity.
The host explicitly supplies `M12004` for X/Y/Z and E0/E1 directions. No fixed
pin or direction assumptions were introduced by this recovery.

## Capacitive probe constants

The public UM3 `cap_sense_probe.cpp` retains the algorithm but references absent
configuration macros. S-line values are useful candidates only after independent
corroboration. These candidates match the shipped simulator's instructions,
literal pools, and symbol sizes:

| Setting in `ProbeSettings` | Recovered value | ARM evidence |
| --- | --- | --- |
| `SAMPLE_COUNT_1`, `SAMPLE_COUNT_2` | 50, 50 | `processSample` at `0x27ee0`: comparisons 49/99; 100-entry sample rings |
| `RESIDUAL_BUFFER_SIZE` | 25 | Comparison 25 and signed 64-bit division by 25 in `processSample` |
| `SAMPLE_FREQUENCY_HZ` | 100 | Float literal `0x28334`; double literal `0x28680` |
| `RESIDUAL_MULTIPLIER` | 1000 | Float literal `0x28338`, used at `0x280be` and `0x280c6` |
| `ALPHA` | 0.85 | Double literal at `0x28670`, used at `0x2837c` |
| `BETA` | 0.04 | Double literal at `0x28678`, used at `0x283b0` |
| `PEAK_THRESHOLD` | 150000 | Immediate constructed at `0x289ec–0x289f0` |
| `PEAK_DROP_NUMERATOR`, `PEAK_DROP_DENOMINATOR` | 3, 4 | `3 * (maximum / 4)` at `0x28b70–0x28ba6`; integer division precedes multiplication |
| `MAX_NOISE_STD` | 20 | `doTheProbing` at `0x28f0a` adds 20 after subtracting baseline |
| `DEFAULT_MOVE_MM` | 10 | G30 default writes float `0x41200000` at `0x214ac–0x214b8` |

Additional observed behavior retained in the published probe source: rejection
count decays by 0.9 (`0x28668`); squared residual rejection threshold is 16 times
variance; reject count greater than 10 aborts. `captureSample` at `0x28e40`
rejects raw 0 and -1, stops after 50 failed reads, and calls `quickStop`.
G30 defaults to 48 mm/min, verbosity 1. `probeWithCapacitiveSensor` at `0x29424`
initializes failed-result height to 150 mm (`0x2946e–0x29476`). These values do
not authorize moving to the failed-result height before host interpretation.

Recovered named buffer sizes corroborate the configuration: `cbuf` at `0x57e04`
is 200 bytes (100 uint16 samples), `zbuf` at `0x57f30` is 400 bytes (100 floats),
and `residual_buffer` at `0x57ecc` is 100 bytes (25 floats). Each of `stat1`,
`stat2`, and `stat_residual` occupies 40 ARM bytes, consistent with five int64
statistics. `workPacket` is 40 ARM bytes. These are observations, not portable
struct definitions; enum sizes, alignment, and pointers differ on AVR.

The sibling `CONFIG_BED_LEVELING_Z_HEIGHT=7` has no identified use in the UM3
probe implementation and was not promoted to a recovered setting. The probe
algorithm, I²C acquisition, measurement cadence, stop timing, and host result
format still require implementation and validation; constants alone do not
provide a functioning probe.

## Implemented checks and remaining gates

`python3 buildroot/tests/um3/run_motion.py` compiles the production helpers as
C++11 with ASan/UBSan and checks every valid 16-slot ring state, byte wraparound,
offset ties and signs, invalid calibration/overflow, tool-dependent stopped XYZ,
unoffset E, and atomic rejection of an invalid position conversion. The module
also compiles for ATmega2560 at 16 MHz with AVR GCC 7.3. These tests cover helper
behavior, not live interrupts or motor motion.

No full planner or capacitive probe was reconstructed in this change. Remaining
gates include modern planner capacity integration, stop/interrupt ordering,
runtime geometry and tool offset updates, homing/endstop failure behavior,
AVR sensor timing and numeric parity, and actual calibration/probing sequences.
The recovered arrays and constants must not be treated as a flashable or
hardware-validated firmware replacement.

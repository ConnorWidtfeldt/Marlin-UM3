# Recovering missing UM3 behavior

The stock 5.3.0 package contains both `jedi.hex` and an unstripped ARM
`jedi-marlin-simulator`. Its symbols expose the missing interfaces, and selected
instructions can be replayed without starting Griffin or the simulator application.
This provides evidence for reconstruction despite the incomplete public source.
It does not recover the original C++ source or establish physical AVR behavior.

The modern target now links the full verified stock frontend and hardware/thermal
implementation to Marlin 2.1.2.9's motion engine. See [integration.md](integration.md).
The isolated helpers and replay experiments below describe the earlier recovery
stage; production power/coordinate helpers are reused and the complete stock
thermal/PID/probe paths are separately exercised by the integration harness.

## What was reconstructed

| Area | Implemented production code | Evidence and verification |
| --- | --- | --- |
| Temperature | Exact signed tables, factor-8 scaling, interpolation and gain/offset compensation | Both binaries contain identical table bytes; 86 ARM replay outputs plus boundary tests |
| PID | Stock state, 32.768 ms coefficient scaling, feed-forward, fan compensation and clipping | Published PID code and shipped instructions; 177 replay outputs |
| Heater interlocks | Removal-allowance state and full-power heating-progress watchdog | 12 removal and 19 watchdog replay outcomes, plus threshold/invalid-input tests |
| Power | Two hotend slots and temperature-dependent bed budget | Published complete power code and shipped instructions; 12 bed replay outputs and 26,880 allocation cases |
| Motion | Planner credits, M218 offset conversion and executed-step coordinate reconstruction | Named ARM instructions, AVR ROM defaults, native boundary/overflow tests |
| Probe | Corroborated missing constants and buffer sizes | Static instructions and data; full algorithm is now imported; physical accuracy remains unverified |

The detailed ledgers are [thermal](recovery-thermal.md), [power](recovery-power.md)
and [motion/probe](recovery-motion.md). Each distinguishes observed behavior from
defensive changes and unresolved integration. All modules compile for ATmega2560
at 16 MHz. Their earlier isolated checks are complementary to the integrated target checks.
The [assembly comparison](assembly-comparison.md) records the initial compiler
experiments and subsequent exact whole-image result. The separate
[historical reconstruction](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/binary-equivalence.md)
now generates the literal stock HEX, with zero differing bytes across all 77942
addressed flash bytes. Its connected drivers, probe and thermal engine are now imported in the modern
branch; the historical planner/stepper/arc implementations are replaced by native
Marlin facilities. The table above describes only
the isolated modern production modules.

New API validation deliberately fails closed on invalid configuration. The power
allocator also caps hotends against the remaining total, a documented improvement
over the vendor overload case. These changes are explicit departures from invalid
or overloaded stock behavior, not claims of exact binary equivalence.

## Tools and provenance

The requested `reverse-engineer-anything` workflow was used. Its registered REA
adapter could not open the native target: it rejected installed Ghidra 12.2, and
its configured Java 21 was too old for that release. No REA/client configuration
was changed. Analysis continued directly with installed Ghidra 12.2, OpenJDK 25,
LLVM tools and controlled Unicorn 2.1.4 replay. These results have no REA Evidence
IDs and are not represented as provider-generated Evidence records.

Ghidra exported 120 selected ARM functions and all 306 functions recognized in
the AVR analysis. The reusable [ExportUm3.java](../../buildroot/share/ultimaker3/ExportUm3.java)
script records symbols, function addresses and pseudocode. The raw decompilation,
Ghidra databases, logs and per-file SHA-256 manifest are retained locally under
ignored `artifacts/recovery/`; they are not required to build or run the fixtures.
[recovery.json](recovery.json) commits the artifact identities, coverage and limits.

The simulator SHA-256 is
`bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`;
the AVR HEX SHA-256 is
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
[host-baseline.json](host-baseline.json) pins their containing official package.

## Reproducing the binary comparison

First verify the downloaded stock package with the documented `inspect-host`
check. Extract its `usr/bin/jedi-marlin-simulator` member into a temporary directory
using 7-Zip. The following optional tools regenerate the committed expected values:

```sh
python3 -m venv /tmp/um3-replay
/tmp/um3-replay/bin/pip install -r buildroot/tests/um3/requirements-recovery.txt
/tmp/um3-replay/bin/python buildroot/tests/um3/replay_stock.py \
  /path/to/jedi-marlin-simulator --check buildroot/tests/um3/stock_oracle.json
```

The producer checks the executable digest before mapping its ELF load segments.
It runs only the selected Thumb functions with bounded instruction counts,
explicit fixture state, and no application entry point or dynamic linker. Only
watchdog clock and shutdown-call boundaries are mocked; the watchdog body runs.
The 306 resulting outputs/outcomes are stored in `stock_oracle.json`. Normal tests
need neither Unicorn nor the binary: they feed these values to production C++.

To repeat the static export with an installed supported Ghidra/Java combination:

```sh
analyzeHeadless /tmp/um3-ghidra um3-simulator \
  -import /path/to/jedi-marlin-simulator \
  -scriptPath "$PWD/buildroot/share/ultimaker3" \
  -postScript ExportUm3.java /tmp/um3-decompiled
analyzeHeadless /tmp/um3-ghidra um3-avr -import /path/to/jedi.hex \
  -processor avr8:LE:16:atmega256 -cspec gcc \
  -scriptPath "$PWD/buildroot/share/ultimaker3" \
  -postScript ExportUm3.java /tmp/um3-avr-decompiled
```

Use a new project directory when repeating imports. Ghidra AVR code addresses
are word addresses; ledger AVR flash offsets are byte addresses. Decompiled
register fragments and inferred types require manual interpretation.

## Checks completed and remaining work

The native protocol, thermal, interlock, motion and power suites pass with
AddressSanitizer/UBSan on macOS/Clang and Linux/GCC. All six production translation
units pass AVR GCC 7.3 object compilation with warnings as errors. The evidence
checks pass. The full UM3 build remains intentionally blocked by the inventory.

ARM uses 64-bit `double`; AVR uses 32-bit `double`. Replay establishes the selected
ARM behavior, not exact AVR rounding, scheduling or I/O. ARM functions have not
been reliably matched to AVR functions; the table/default byte matches are
independent corroboration of data only. Static export counts do not establish
complete function discovery.

Remaining required work includes board-revision/safety-circuit resolution,
I2C acquisition and sample freshness, print-core identity/presence, timer/PWM
ownership, fault latching and every heater-output gate, serial/command dispatch,
runtime configuration, planner/stepper binding, homing and the full capacitive
probe. The [inventory](inventory.json) retains those gates. No UM3 firmware image
was produced, and no physical hardware or printing tests were performed.

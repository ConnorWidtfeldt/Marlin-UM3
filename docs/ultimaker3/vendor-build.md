# Published UM3 source build: Arduino 1.0.5

On 2026-11-05 the unmodified `Ultimaker3` revision
`4545818112027387f09e87f930117ab8f3804327` was built using its supplied
`Marlin/CMakeLists.txt`, the official Arduino 1.0.5 Linux distribution, and that
distribution's original AVR GCC/G++ **4.3.2**. Configuration succeeded;
compilation failed. No `jedi.hex` was produced.

This is the repository identified by a Team Ultimaker member in the
[March 2021 source-code discussion](https://community.ultimaker.com/topic/36217-marlin-source-code-for-version-52/).
The problem is the consistency of that published tree, not uncertainty about
which project `jedi-marlin` refers to.

## Environment and controls

- Official download: `https://downloads.arduino.cc/arduino-1.0.5-linux64.tgz`.
- Archive SHA-256: `3047aa2a0482aea9eeafbea0c0a43756c8bda0772aaeff97856e1d70ad16cfaf`.
- Compiler and Arduino core/libraries came from that archive; no modern AVR
  toolchain was substituted. Arduino's `hardware/tools/readme.txt` identifies
  the bundled Linux compiler as extracted from Ubuntu 9.04 packages.
- Build helper: CMake 3.16.3 and Make, running in an AMD64 Ubuntu 20.04 container.
  The original GCC binaries ran through Docker Desktop's AMD64 support on the
  Apple Silicon host. No AVR firmware was executed.
- Source and Arduino files were mounted read-only; the running container had
  networking disabled. All 531 archived source files were checked against their
  Git blob identities and remained unchanged.
- Arduino 1.0.5's own `examples/01.Basics/Blink/Blink.ino` compiled and linked for
  **ATmega2560 at 16 MHz**, using the same core sources and tools. Its ELF had
  2442 bytes of text, 2 bytes of data and 25 bytes of BSS. This positive control
  establishes that the historical compiler, headers, core and linker work in the
  test environment; it is not a printer hardware test.

## Results

| Attempt | Configure | Compile/link | UM3 HEX |
| --- | --- | --- | --- |
| Published CMake target, original configuration | Pass | Fail | Absent |
| Same source, default `MOTHERBOARD=72` supplied globally | Pass | Fail | Absent |
| Arduino Blink positive control | N/A | Pass | Example only |

The first attempt also encounters `Unknown MOTHERBOARD value`: some new files
include `pins.h` before `Configuration.h`. The controlled retry adds
`-DMOTHERBOARD=72` to C and C++ flags, matching the existing configuration default.
This removes that include-order error without changing source or guessing pins.
The independent UM3 interface errors remain:

| Location | Compiler diagnostic |
| --- | --- |
| `Board.cpp:62` | `ADC_INPUT_VOLTAGE_PIN` was not declared |
| `Marlin_main.cpp:145` | Conflicting declaration of scalar `extrudemultiply`; retained header declares an array |
| `Marlin_main.cpp:300` | `temperatureInit` was not declared |
| `Marlin_main.cpp:302` | `st_enable_interrupt` was not declared |
| `Marlin_main.cpp:321` | `plan_buf_free_positions` was not declared |
| `Marlin_main.cpp:927,934` | `hotend_pid` and `heated_bed_pid` were not declared |
| `cap_sense_probe.cpp:139` | `CONFIG_BED_LEVEL_SENSOR_SAMPLE_COUNT1/2` were not declared |

There were 460 compiler error diagnostics in the original attempt and 417 in the
board-flag retry. Many are cascading errors; those totals are not counts of
independent defects. They corroborate the incompatible main/header/core versions
already identified in [compatibility.md](compatibility.md).

## Reproduction details

Extract the pinned vendor revision and official Arduino archive into separate
directories. In a Linux AMD64 environment with CMake and Make, expose the Arduino
directory at `/usr/share/arduino`, source at `/src`, and a writable output
directory at `/out`. Put `/usr/share/arduino/hardware/tools/avr/bin` first in PATH.
The compiler wrappers in that directory select the bundled historical libraries.

The original-source build command was:

```sh
cmake -S /src/Marlin -B /out/vendor \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_C_COMPILER=/usr/share/arduino/hardware/tools/avr/bin/avr-gcc \
  -DCMAKE_CXX_COMPILER=/usr/share/arduino/hardware/tools/avr/bin/avr-g++ \
  -DCMAKE_C_FLAGS=-mmcu=atmega2560 \
  -DCMAKE_CXX_FLAGS=-mmcu=atmega2560 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build /out/vendor -- -k -j2
```

Cross-compiler selection is supplied before CMake's initial `project()` compiler
checks. The vendor file sets these too late for initial toolchain detection.
The retry uses a fresh build directory, adding `-DMOTHERBOARD=72` to both flag
arguments. No firmware source changes or no-op substitutes were used.

[vendor-build.json](vendor-build.json) records exact archive/image identities,
outcomes and log hashes. Full logs, compile commands, the Dockerfile and the
control-build script are retained locally in
`artifacts/vendor-arduino-1.0.5/` (ignored build evidence, not firmware for release).

This test confirms failure of the published source with its documented compiler.
It does not identify the exact unpublished build inputs, establish equivalence
to the installed stock binary, or resolve thermal, probing and board-safety
behavior. Those requirements continue to block the replacement firmware.

## Subsequent reconstruction

The failed unmodified build above is retained as baseline evidence. The separate
[`reconstruct/um3-stock` branch](https://github.com/ConnorWidtfeldt/Marlin-UM3/tree/reconstruct/um3-stock)
now repairs the configuration, headers, serial, planner/stepper and thermal
implementations using corroborated binary evidence and identified sibling source.
The first GCC 4.3.2 Docker build linked successfully but differed from stock.
Subsequent compiler calibration selected GCC 4.8.1 / binutils 2.24 / avr-libc 1.8.0
and restored the original C++ link driver. Source-form and linker-input corrections
now produce **exactly the official stock HEX**: 77942 flash bytes, 5501 static SRAM.

See the [exact-match evidence](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/binary-equivalence.md)
and [reproduction instructions](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/README.md).
This does not change the failure of the published unmodified tree or qualify the
modern UM3 port.

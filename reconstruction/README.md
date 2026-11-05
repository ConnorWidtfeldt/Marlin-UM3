# Docker build of the reconstructed stock UM3 baseline

This branch repairs the incomplete historical UM3 vendor source so it can be
compiled and compared against the shipped AVR firmware. It is not the modern
Marlin 2.1.2.9 port and is not a qualified replacement image.

From this checkout, with Docker running:

```sh
sh reconstruction/build.sh
```

An optional first argument selects the output directory. On Apple Silicon,
Docker Desktop must support Linux AMD64 containers. No Arduino installation or
AVR compiler is needed on the host. The image uses a pinned Ubuntu base and the
SHA-256-verified official Arduino 1.0.5 SDK, including original AVR GCC 4.3.2.
Image creation downloads packages and the SDK; the firmware compilation itself
runs without networking, with read-only source/root filesystem, as the invoking
user, with no attached devices or upload operation.

Outputs under `artifacts/stock-comparison/` include the comparison HEX, ELF,
linker map, disassembly, size report, checksums, source hashes, exact Docker image
ID and tool versions. The image name includes `NOT-FOR-FLASHING`. A failed run
clears the previous published output names before compiling, so an old HEX is not
mistaken for a successful new build. Intermediate CMake files remain under
`build/` for diagnosis.

Build date/time strings are fixed to the source commit's UTC timestamp, including
with historical GCC versions that do not honor SOURCE_DATE_EPOCH themselves.
The SDK/compiler and base image are pinned; host build-helper packages are
installed from Ubuntu's repositories and recorded by the tool-version output.
Cross-machine reproducibility must be assessed from generated checksums, not
assumed from a successful build. The source dirty flag and full source hashes
make local experimental modifications visible in the report.

The development budgets are 253952 application bytes (8 KiB stock bootloader
reservation) and 6144 static SRAM bytes (at least 2 KiB remaining for stack and
interrupts). Dynamic allocation and actual stack/interrupt high-water behavior
are not established by those numbers. No hardware release flag is generated.

## What was repaired

- [Configuration and shared declarations](configuration.md): replace stale UM2
  interfaces with stock-binary-backed UM3 settings, verified pin names and
  physical port aliases; retain host ownership of runtime calibration.
- [Serial](serial.md): restore the shipped receive-ring layout, LF framing and
  exact protocol diagnostics. The capacitive I2C functions already existed;
  enabling the missing configuration exposes their real implementations.
- [Motion](motion.md): restore complete planner/stepper implementations with
  tool offsets, runtime directions, credits and quick-stop position recovery.
- [Thermal](thermal.md): restore ADC conversion, PID objects, power budgeting,
  presence/removal logic, PWM, fault counters and the hardware watchdog.

The reconstruction adapts GPL sibling S-line source only as identified candidate
code, removing newer-machine differences where the stock UM3 binary disagrees.
The ledgers distinguish independently corroborated behavior from code that still
needs AVR comparison. No no-op driver or dummy safety implementation substitutes
for missing behavior. The current board-revision/schematic discrepancy remains a
hardware-acceptance blocker even if the source builds successfully.

The integrated Docker build on 2026-11-05 passed with AVR GCC 4.3.2: 97428 bytes
application flash and 5759 bytes static SRAM, leaving 2433 bytes for heap, stack
and interrupts. The native serial sanitizer and report boundary checks pass.
Those observations establish a linked comparison artifact, not stock binary
equivalence or runtime memory safety. Clean-build checksums are retained with the
local build artifacts. A Linux CI workflow is provided; a GitHub Actions run has
not been verified.

Run the production serial sanitizer check separately with a native C++ compiler:

```sh
python3 reconstruction/run_serial_test.py
python3 reconstruction/test_report.py
```

The modern `um3` branch has additional binary-replay math/state fixtures. Those
checks apply to its isolated components; they do not by themselves test this
branch's complete historical ISR, planner, thermal engine or hardware outputs.

The next acceptance work is comparing the linked historical baseline with stock
AVR function bodies, constants, call targets and state transitions, calibrating
compiler/options first. Matching source compilation is only the beginning of
that comparison. Flashing, fuses, bootloader changes, host firmware installation
and calibration changes are outside this build script.

The final historical linker fix uses `avr-gcc` and explicit `-lm`, matching the
vendor UM2 Makefile's convention. The C++ link driver selected incompatible
floating-point support ordering and failed with `R_AVR_13_PCREL` truncations.
No math-library object was patched or substituted. The role of explicit `-lm`
and library ordering is documented in the
[AVR-LibC manual](https://avrdudes.github.io/avr-libc/avr-libc-user-manual-2.0.0/using_tools.html).

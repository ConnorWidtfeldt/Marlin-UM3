# Reproducible stock UM3 baseline

The reconstructed historical firmware now produces **exactly the official UM3
5.3.0 `jedi.hex`**: the literal HEX file and all 77942 addressed flash bytes match.
See [binary-equivalence.md](binary-equivalence.md) for the comparison, provenance
and limits. This branch is separate from the modern Marlin 2.1.2.9 port.

With Docker running, from this checkout:

```sh
sh reconstruction/build.sh
python3 -c "import json; assert json.load(open('artifacts/stock-comparison/provenance.json'))['stock_hex_identical']"
```

An optional first argument selects the output directory. On Apple Silicon,
Docker Desktop must support Linux AMD64 containers. The image uses a pinned
Ubuntu base, Arduino 1.0.5 **core sources**, Debian/Atmel AVR GCC 4.8.1, binutils
2.24 and avr-libc 1.8.0. Every compiler/library/SDK archive is SHA-256 locked in
[toolchain.sha256](toolchain.sha256); download locations are in
[toolchain.urls](toolchain.urls). No host Arduino installation is needed.

Image creation downloads packages; compilation runs without networking, with
read-only source/root filesystem, as the invoking user, with no attached devices
or upload operation. The build never reads the stock HEX or copies its bytes.

Outputs under `artifacts/stock-comparison/` include HEX, ELF, linker map,
disassembly, size report, checksums, source hashes, Docker image ID and compiler
package versions. `provenance.json` records the source commit/dirty state and
`stock_hex_identical`, computed against the pinned official HEX SHA-256. The
build retains a differing experimental image for diagnosis; CI requires this
flag to be true. Failed builds clear old published artifact names first.

The default `SOURCE_DATE_EPOCH=1548629527` restores the stock build strings
`Jan 27 2019` and `22:52:07`; the original build timezone is unknown. This is
separate from recorded source identity. Overriding it intentionally changes the
HEX. Host CMake/make/Python packages come from Ubuntu repositories; their versions
and exact Docker image are recorded. HEX reproducibility has been verified;
cross-host ELF/debug-path identity has not.

## Reconstruction evidence

- [Configuration and declarations](configuration.md): stock defaults, pin names,
  physical port aliases and host ownership of runtime calibration.
- [Serial](serial.md): receive-ring layout, LF framing and protocol diagnostics.
- [Motion](motion.md): full planner/stepper behavior, tool offsets, runtime
  directions, queue credits and quick-stop coordinate recovery.
- [Thermal](thermal.md): conversion, PID, power budgeting, removal interlocks,
  PWM, faults and watchdog.
- [Compiler fingerprint](compiler-evidence.md): independent unchanged controls,
  toolchain selection and the final linker trampoline-order correction.

GPL sibling S-line source supplied candidates, checked against the shipped UM3
instructions and corrected where behavior differed. Ordinary source compilation
produces the match, without code padding, dummy implementations, binary patches
or copying stock output into the artifact. Matching the linked UM3 image does
not establish unused conditional source or original source-text identity.

The matching image uses 77942 application flash bytes and 5501 static SRAM bytes,
leaving 2691 bytes for heap, stack and interrupts. Development limits are 253952
application bytes (8 KiB bootloader reservation) and 6144 static SRAM bytes.
Stack/interrupt high-water usage has not been measured.

Run the native checks separately:

```sh
python3 reconstruction/test_report.py
python3 reconstruction/test_compare_hex.py
python3 reconstruction/run_serial_test.py
```

To independently compare an extracted official HEX, whose carrier digest is
checked before comparison:

```sh
python3 reconstruction/compare_hex.py /path/to/official/jedi.hex \
  artifacts/stock-comparison/jedi-reconstructed-NOT-FOR-FLASHING.hex \
  --output artifacts/stock-comparison/equivalence.json
```

The comparator checks checksums, extended addresses, overlapping records, holes,
entry records and complete programmed-byte equality; it also reports literal
file equality. It does not mask addresses or normalize away code differences.

macOS and isolated Linux serial sanitizer/comparator checks passed. A Linux CI
workflow enforces the exact stock HEX digest; a GitHub Actions run has not been
verified. The initial GCC 4.3.2 build at commit `f0d92f0b6` remains in published
history (97428 flash / 5759 SRAM); it was a compilation milestone and did not
match stock. Its C-link-driver/libm workaround is unnecessary with the matching
4.8.1 toolchain, so the original C++ link driver/library order is restored.

No firmware has been installed or tested on a printer. The board-revision /
schematic discrepancy remains a hardware-acceptance issue. No flashing, fuses,
bootloader, host installation or calibration changes are performed by these
scripts. The modern `um3` branch still requires integration and its separate
release gate remains closed.

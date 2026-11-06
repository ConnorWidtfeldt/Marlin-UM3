# UM3 Extended port status

The dedicated `um3_extended` target now builds a complete experimental firmware
image on Marlin **2.1.2.9**, using its current planner/stepper/arc engine and the
verified UM3 stock host, peripheral, probe and thermal implementation.
**Hardware acceptance remains outstanding; do not treat this as a qualified
replacement.** See the [integration evidence and limits](integration.md) and
[compatibility inventory](inventory.json).

The separate historical [`reconstruct/um3-stock` branch](https://github.com/ConnorWidtfeldt/Marlin-UM3/tree/reconstruct/um3-stock)
continues to reproduce the exact official 5.3.0 HEX. Its
[comparison report](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/binary-equivalence.md)
is the source baseline for this port. The modern image intentionally differs.

## Reproduce the checks

Use Python 3.11 or newer and a C++ compiler with AddressSanitizer/UBSan. Install
PlatformIO into a virtual environment; the platform, compiler and Arduino AVR
framework versions are pinned in `ini/ultimaker3.ini`.
GCC's build date comes from the source commit timestamp via `SOURCE_DATE_EPOCH`.
Source debug paths are normalized; cross-host bit-identical ELF output has not
been verified.

```sh
python3 -m venv .venv-um3
. .venv-um3/bin/activate
python -m pip install -r buildroot/share/ultimaker3/requirements.txt
python buildroot/tests/um3/test_evidence.py
python buildroot/tests/um3/run.py
python buildroot/tests/um3/run_thermal.py
python buildroot/tests/um3/run_interlock.py
python buildroot/tests/um3/run_motion.py
python buildroot/tests/um3/run_power.py
python buildroot/tests/um3/run_integration.py
pio run -e um3_extended
pio run -e um3_upstream_check
python buildroot/share/ultimaker3/verify.py avr-components --core "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
python buildroot/share/ultimaker3/verify.py check-release
```

The final `check-release` command still **must fail**: release qualification
is separate from building the experimental image. The UM3 build itself succeeds
and produces `.pio/build/um3_extended/firmware.hex`. It does not flash a device.

`um3_upstream_check` builds the unchanged default Marlin RAMPS configuration for
an ATmega2560. Its image is exclusively an upstream regression artifact and
**must not be installed on an Ultimaker**. `avr-components` compiles the six isolated
UM3 modules for ATmega2560 to temporary objects, without linking a
firmware image. `avr-protocol` remains an alias for existing callers.

Package the UM3 HEX, ELF, map, size report, source hashes, checksums and provenance:

```sh
python buildroot/share/ultimaker3/verify.py build-report \
  --core "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}" \
  --environment um3_extended --output artifacts/um3-extended-NOT-FOR-FLASHING
```

For the upstream regression report, select `um3_upstream_check` and a separate
output directory. The report identifies its environment and dirty-worktree state,
and records `hardware_release: false` and outstanding qualification blockers. The stock
bootloader reserves 8192 bytes, so application addresses must remain below
`0x3e000` (253952 bytes). Static SRAM is limited to 6144 bytes to reserve at least
2048 bytes for stack and interrupts. That reserve is a conservative development
policy, not a measured worst-case stack bound; hardware stress measurements are
still required for release.

## Reproduce the evidence

Download the [official 5.3.0 package](https://software.ultimaker.com/releases/firmware/9066/stable/5.3.0.20211003/um-update-5.3.0.20211003.swu)
to a directory outside the repository. Install a 7-Zip version with SquashFS/XZ
support (`7zz` on macOS, `7z` on Ubuntu). The inspector verifies the complete
package SHA-256 before streaming selected files; it does not mount the image or
run its programs. The trailing vendor signature has not been independently
verified.

```sh
python buildroot/share/ultimaker3/verify.py inspect-host /path/to/um-update-5.3.0.20211003.swu --check
# On Ubuntu add: --extractor 7z
git clone --bare --single-branch --branch Ultimaker3 \
  https://github.com/Ultimaker/UltimakerMarlin.git /tmp/um3-vendor.git
python buildroot/share/ultimaker3/verify.py audit-vendor /tmp/um3-vendor.git
```

[host-baseline.json](host-baseline.json) records the parsed Extended overlay,
stock settings, flash address ranges and member hashes. The stock Python modules
used for fixture verification are not redistributed. The decoder test runner's
optional `--stock-root /path/to/usr/share/griffin` checks all module hashes before
executing only the four packet/CRC/response modules. See its
[README](../../buildroot/tests/um3/README.md).

## Maintaining the Git history

The `um3` branch descends directly from Marlin release `2.1.2.9`. Configure remotes
in a fresh clone (remotes are local Git settings, not transferred by cloning):

```sh
git remote add upstream https://github.com/MarlinFirmware/Marlin.git
git remote add ultimaker https://github.com/Ultimaker/UltimakerMarlin.git
```

Vendor snapshots are retained as annotated `reference/ultimaker3-2021-02-19`
and `reference/s-line-2021-02-19` tags. Do not force-update them. Preserve vendor
copyright and provenance when adapting code; keep UM3 commits grouped by behavior.

For a future stable version, fetch that exact upstream tag, create a branch
named `integrate/marlin-<tag>` from `um3`, and merge the tag with `--no-ff`.
Resolve source and configuration changes, rerun the evidence, protocol, AVR and
inventory checks, and review differences in the hardware and host interfaces.
Advance `um3` and HomeFab's submodule pointer only after those checks pass. Do not
rebase published commits or automatically merge moving bugfix branches. Until the required inventory and hardware checks are qualified, maintenance
checks and compiled artifacts qualify development code only.

## Eventual hardware acceptance and rollback

Before any flashing, confirm the installed host version, physical board revision,
bootloader and calibration. Keep an off-printer backup of the original configured
`/usr/share/griffin/griffin/machines/jedi.hex`, its checksum, and the machine's
settings/calibration under `/var/lib/griffin`. Inspect the actual installed
configuration before assuming these paths are unchanged.

The stock initialization verifies MCU flash against its configured HEX and
reprograms mismatches. Eventual installation must therefore update that host file
as well as MCU flash. A full vendor update/recovery can restore the stock image.
Do not change the bootloader or fuses as part of this port. Restore the saved HEX
through the stock update path to roll back; retain calibration separately.

After all required offline inventory entries are verified, validate in this order:

1. Cold startup, identity, board detection, power sequencing and fault reporting.
2. Cold sensor readings, core presence/removal, fans, lighting and communications.
3. Endstop states and directions, then low-speed single-axis motion and homing.
4. Travel limits, tool coordinates and capacitive probing with immediate access to power removal.
5. Each heater separately, then combined power limiting, communication loss,
   removal interlocks, watchdog and thermal faults using a controlled test setup.
6. Single-material and dual-material prints, cancellation, recovery and repeated restarts.

No hardware tests, print-performance comparisons or stack high-water measurements
have been performed. Successful offline builds and mocked checks do not establish
electrical safety, probing latency or print quality.

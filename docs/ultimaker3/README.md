# UM3 Extended port status

**Development branch; not a drop-in firmware replacement.** The linked vendor
source is incomplete. The verified wire decoder is implemented, but it is not
connected to Marlin's command queue or hardware. The `um3_extended` environment
deliberately stops with `UM3_RELEASE_BLOCKED` before producing an image.

The first source-readiness milestone found missing temperature, motion, probe
and safety implementations. See [compatibility.md](compatibility.md),
[hardware.md](hardware.md), and the exhaustive command/subsystem
[inventory.json](inventory.json). A complete UM3 source release corresponding to
the stock `jedi-marlin` **2019.01.27** package, especially the files identified in
[vendor-baseline.json](vendor-baseline.json), is needed to resolve those gaps.
S-line implementations are comparison evidence only.

An [actual build with the documented Arduino 1.0.5 compiler](vendor-build.md)
confirms that the published tree fails compilation; the same toolchain
successfully builds Arduino's Blink control example. `jedi-marlin` is the vendor
package name for this intended source project, not a separate repository.

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
pio run -e um3_upstream_check
python buildroot/share/ultimaker3/verify.py avr-protocol --core "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
python buildroot/share/ultimaker3/verify.py check-release
```

The final command currently **must fail**. `pio run -e um3_extended` must also
fail and must not produce `.pio/build/um3_extended/firmware.hex`. These failures
are exercised in CI; they do not count as a successful UM3 firmware build.

`um3_upstream_check` builds the unchanged default Marlin RAMPS configuration for
an ATmega2560. Its image is exclusively an upstream regression artifact and
**must not be installed on an Ultimaker**. `avr-protocol` compiles the actual
decoder for ATmega2560 to a temporary object, without linking a firmware image.

Generate the regression HEX, ELF, map, size report, checksums and provenance:

```sh
python buildroot/share/ultimaker3/verify.py build-report \
  --core "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}" \
  --environment um3_upstream_check --output artifacts/upstream-avr-regression
```

The report identifies its environment and dirty-worktree state. The stock
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
rebase published commits or automatically merge moving bugfix branches. While
the source gate is blocked, maintenance checks qualify development code only.

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
have been performed. Successful offline builds will not by themselves establish
electrical safety, probing latency or print quality.

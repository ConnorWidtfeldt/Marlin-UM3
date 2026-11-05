# Exact UM3 stock firmware reproduction

## Result

The reconstructed source, compiled with the locked toolchain, generates the
**same literal Intel HEX file** as the official UM3 5.3.0 package. Both SHA-256:

```
448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65
```

All **77942 flash bytes**, at `0x00000..0x13075`, match at the same addresses.
There are zero differing, missing or extra addresses and equal entry records.
[The machine-readable comparison](equivalence.json) records both identities.
The decoded contiguous payload SHA-256 is
`5eac67eb2d7ddb0ae14af293040154f844c51003df512176385319ac2583d487`.
This includes vectors, compiler-generated trampolines, instructions, library
code, constants, tables, build strings and initialized SRAM data in flash.

The reference is `usr/share/griffin/griffin/machines/jedi.hex` extracted from the
[official package](https://software.ultimaker.com/releases/firmware/9066/stable/5.3.0.20211003/um-update-5.3.0.20211003.swu),
whose complete SHA-256 is
`5599f1108837fc365b4f5fd567d1561db749a7b28f7befad1e3117d75b182df2`.
Its trailing vendor signature was not independently verified. No stock HEX is
an input to compilation; only its digest is used for the result check.

## What closed the gaps

1. Match the **compiler, headers and runtime libraries together**: Debian/Atmel
   GCC 4.8.1, binutils 2.24 and avr-libc 1.8.0, with Arduino 1.0.5 core sources.
   Unchanged CRC and Arduino routines identified this profile independently.
2. Restore the original C++ link driver and implicit library order. The older
   GCC 4.3.2 C-driver workaround changed runtime helper layout.
3. Restore the stock timestamp and uppercase `OPEN` endstop response.
4. Recover exact source branch forms for thermal output/init paths and the
   M18/M84 all-axis versus selected-axis block. Equivalent C++ expressions had
   generated different instruction layouts; ordinary corrected source now
   matches without instruction padding or post-link patches.
5. Remove the unused legacy `movesplanned()` wrapper, absent from the shipped
   symbolized simulator and unreferenced in the reconstructed target. The
   linker discarded its body but allocated an input-section ID. Binutils' AVR
   trampoline hash incorporates those IDs, so that one section reordered 94
   of 99 entries and left 172 differing bytes despite identical remaining code.
   Removing the unsupported function restored the complete stock table.

The final image contains 77942 flash bytes and 5501 static SRAM bytes, leaving
2691 bytes for heap, stack and interrupts. The application remains below the
stock `0x3e000` bootloader boundary. This reserve is not a measured stack bound.

## Verification and interpretation

Two fresh Docker output directories are built from the committed reconstruction
with a fixed stock timestamp and read-only source. The independent comparator
checks the official file's pinned digest, then compares complete addressed byte
maps and the carrier files. The build's provenance records the source commit,
source dirty flag, compiler packages, source hashes and Docker image ID. Native
report/comparator tests and serial ASan/UBSan checks run on macOS and Linux.
See [the reproduction commands](README.md).

This establishes a reproducible implementation of the **linked stock UM3 AVR
configuration**. It removes the missing-stock-implementation uncertainty for
porting this target. It does not recover the original source text or establish
unused build variants. It also reproduces any stock firmware defects.

Moving the behavior to modern Marlin is still a separate integration task:
planner/ISR timing, modern command dispatch, HAL/pin ownership, thermal output
paths and host calibration ownership need verification in the new architecture.
The modern firmware will intentionally differ in binary output. No physical
printer testing, stack high-water measurements or flashing has occurred; the
modern release gate remains closed.

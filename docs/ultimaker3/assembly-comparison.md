# Stock assembly comparison

The separate historical reconstruction now builds **exactly the official UM3
5.3.0 HEX**: all 77942 addressed flash bytes and the literal file are identical,
SHA-256 `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
The [full evidence](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/binary-equivalence.md)
and [compiler investigation](https://github.com/ConnorWidtfeldt/Marlin-UM3/blob/7a80a3bb615362f513e1608447a6eb92efa35098/reconstruction/compiler-evidence.md)
record the matching GCC 4.8.1 / Atmel 3.4.4 toolchain, source corrections and
trampoline ordering. No output bytes are patched or copied from stock.

This establishes the linked stock target as a baseline for porting. The modern
port intentionally changes implementation and will require behavioral integration
tests; its binary is not expected to match historical firmware. Hardware acceptance
remains separate. The initial investigation below is retained as history.

## Initial bounded AVR experiment, 2026-11-05

Compare unchanged vendor CRC8, CRC16 and power-budget source from
`4545818112027387f09e87f930117ab8f3804327` against the stock 5.3.0 AVR image,
SHA-256 `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
The source can compile to individual objects even though the full tree fails.

| Compiler | Result in sampled vendor routines |
| --- | --- |
| AVR GCC 4.3.2, official Arduino 1.0.5 | No full-function matches in the sampled CRC/power routines |
| AVR GCC 7.3.0, pinned PlatformIO toolchain, original Arduino headers | Two small CRC constructors match exactly after resolving their relocations; the main CRC routines and sampled power routines do not match |

The search covers 12 distinct routine bodies at least 16 bytes long across five
source files, excluding unused CRC self-tests. Constructor ABI aliases are counted
once. Internal relative branches are resolved; the 4.3.2 CRC table relocations
are resolved from exact unique table bytes in stock flash. Remaining external
relocation bits are masked during candidate search. A masked match alone is not
equivalence: its referenced targets and data still require verification.

The two exact GCC 7.3 matches are:

| Routine | Stock AVR byte address | Size | Resolved tail-jump target |
| --- | --- | --- | --- |
| `Crc8::Crc8(message,length)` | `0x2c04` | 22 bytes | packet update at `0x2bd0` |
| `Crc16::Crc16(message,length)` | `0x2c7a` | 28 bytes | packet update at `0x2c46` |

Both constructor bodies match every byte once the jump targets and internal
branches are resolved. Their called packet-update implementations still differ
from this compiler's output. The 50 matching bytes are too small and simple to
identify the stock compiler or validate reconstructed thermal/motion behavior.
Conversely, the negative searches do not establish behavioral differences: code
generation, options, source versions and inlining can all change instructions.

Both compilations use the vendor CMake C++ options: ATmega2560, `-Os`,
`-funsigned-char`, `-funsigned-bitfields`, `-fpack-struct`, `-fshort-enums`,
`-ffunction-sections`, `-fdata-sections`, `ARDUINO=105`, and `F_CPU=16000000`.
The 4.3.2 objects come from the recorded [historical build](vendor-build.md).
The comparison script, object files, compiler/source identities and raw search
results are retained locally under ignored `artifacts/assembly-comparison/`.
This is a limited experiment, not a validated general binary-diff tool or a
firmware-wide similarity percentage.

## Compiler identity and a stronger verification path

The ARM simulator ELF `.comment` contains GCC `4.9.2-10` and `4.8.4-1` markers.
These establish that ARM link inputs carry those compiler identities. They do
not identify which compiler built each routine or the AVR firmware. Intel HEX
does not preserve the AVR ELF's symbol/debug/comment sections, so the stock AVR
compiler was not identified by that initial experiment. The README's Arduino 1.0.5 recommendation is a candidate
build environment, not proof of the unpublished production environment.

Matching the historical compiler and SDK is useful, but byte identity also
depends on source revision, all configuration/preprocessor inputs, ABI and
optimization options, library implementations, linker behavior and layout.
Different code can implement the same behavior; matching relocated instruction
bytes alone does not verify the referenced data, callees or physical I/O.

The initial proposed verification path was to reproduce substantial
unchanged routines to calibrate compiler/options, then reconstruct missing
routines while retaining the vendor API/layout and compare their control flow,
instructions, constants, field offsets and called targets against stock AVR.
Keep that reconstruction separate from intentional hardening and modern Marlin
integration so each difference has an explanation. The existing ARM replay tests
remain complementary behavioral evidence; they do not replace AVR comparison.

The initial experiment alone did not establish full-build equivalence. The
subsequent reconstruction linked above meets that byte-identity gate; hardware
acceptance remains outstanding.

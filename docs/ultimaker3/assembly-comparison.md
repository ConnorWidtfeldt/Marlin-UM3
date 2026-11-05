# Stock assembly comparison: current evidence

There is no complete source-built UM3 image to compare with stock `jedi.hex`.
The historical vendor tree fails compilation, while `um3_upstream_check` builds
an unrelated upstream RAMPS regression image. The reconstructed UM3 components
compile to AVR objects, but their defensive checks, APIs and layouts deliberately
differ from stock. Whole-image similarity is therefore not an acceptance metric
for the modern port.

## Bounded AVR experiment, 2026-11-05

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
compiler remains unknown. The README's Arduino 1.0.5 recommendation is a candidate
build environment, not proof of the unpublished production environment.

Matching the historical compiler and SDK is useful, but byte identity also
depends on source revision, all configuration/preprocessor inputs, ABI and
optimization options, library implementations, linker behavior and layout.
Different code can implement the same behavior; matching relocated instruction
bytes alone does not verify the referenced data, callees or physical I/O.

The next historical-baseline acceptance work should first reproduce substantial
unchanged routines to calibrate compiler/options. Then reconstruct missing
routines while retaining the vendor API/layout and compare their control flow,
instructions, constants, field offsets and called targets against stock AVR.
Keep that reconstruction separate from intentional hardening and modern Marlin
integration so each difference has an explanation. The existing ARM replay tests
remain complementary behavioral evidence; they do not replace AVR comparison.

No stock-equivalent full build, exact AVR compiler identification, or hardware
acceptance was established by this experiment.

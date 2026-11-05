# AVR compiler and SDK fingerprint evidence

These are direct binary/object observations, not REA provider Evidence records.
The installed REA native adapter cannot use the available Ghidra version, so
this investigation reuses the existing AVR disassembly and LLVM/ELF evidence.
No provider/client configuration was changed.

The completed reconstruction now reproduces the entire stock HEX file exactly:
all 77942 addressed flash bytes, entry records, and the literal Intel HEX carrier
agree. The final result is recorded in `artifacts/stock-exact-a/equivalence.json`;
the source-built HEX SHA-256 is
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
The controls below record how the matching compiler, SDK, source forms and
linker behavior were established.

## Target and controls

- Target: official UM3 5.3.0 image's `jedi.hex`, SHA-256
  `448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
- Decoded target: addressed Intel HEX bytes already verified against the pinned
  package, not a concatenation that loses extended addresses.
- Unchanged controls: vendor `crc8.cpp` and `crc16.cpp` at
  `4545818112027387f09e87f930117ab8f3804327`.
- Initial compiler controls: Arduino 1.0.5's AVR GCC 4.3.2 and PlatformIO AVR
  GCC 7.3.0. Subsequent Debian/Atmel GCC 4.8.1 and GCC 4.9.2 packages were tested
  with their corresponding binutils and avr-libc packages.

The CRC comparison resolves function-local branch displacements, exact table
addresses from unique table bytes in stock flash, and calls to the identified
CRC functions. It compares the resulting complete bytes at the stock routine
addresses, rather than treating masked address fields as proof. Constructor
aliases are counted once.

| Distinct routine | Stock AVR byte address | Stock bytes |
| --- | --- | --- |
| `Crc8::update(byte)` | `0x2bba` | 22 |
| `Crc8::update(message,length)` | `0x2bd0` | 52 |
| `Crc8::Crc8(message,length)` | `0x2c04` | 22 |
| `Crc16::update(byte)` | `0x2c1a` | 44 |
| `Crc16::update(message,length)` | `0x2c46` | 52 |
| `Crc16::Crc16(message,length)` | `0x2c7a` | 28 |

## Matching toolchain found through independent controls

The Debian/Atmel **AVR GCC 4.8.1 + binutils 2.24 + avr-libc 1.8.0** profile
reproduces all six unchanged CRC bodies exactly after resolving their table,
call and internal-branch addresses. It also reproduces seven independent Arduino
core/watchdog routines byte-for-byte, including nontrivial 212-byte and 196-byte
functions. The GCC 4.9.2 profile remains close in total image size but fails these
same discriminating control comparisons.

| Exact GCC 4.8.1 control | Stock address | Bytes | Address adjustment |
| --- | --- | --- | --- |
| `millis` | `0x2186` | 24 | None; timer-state addresses also match |
| `micros` | `0x219e` | 72 | None |
| `delay` | `0x21e6` | 212 | None |
| `delayMicroseconds` | `0x22ba` | 22 | None |
| `init` | `0x22d0` | 196 | None |
| `watchdog_init` | `0x11758` | 24 | Built at `0x1174e` before remaining source corrections |
| `watchdog_reset` | `0x11770` | 4 | Built at `0x11766` before remaining source corrections |

The initial full GCC 4.8.1 candidate contained 77932 application bytes versus
stock 77942, with identical 5501-byte static SRAM totals. Its remaining source
differences were resolved independently, as recorded in the motion and thermal
ledgers; they were not accepted merely because the sizes were close. The control
matches identify a reproducible
code-generation/SDK profile, not necessarily the exact operating-system image
or original compiler package used at Ultimaker.

Selected Debian package identities:

| Package | SHA-256 |
| --- | --- |
| `gcc-avr_4.8.1+Atmel3.4.4-2_amd64.deb` | `029949f15a116f1d8380e1ab68e5e71ec41be9d71224c267264271013f653e14` |
| `binutils-avr_2.24+Atmel3.4.4-1_amd64.deb` | `97908c3ed3f1d95749615245ddeda892e8c1bd31132754f23fdb3059b43b7139` |
| `avr-libc_1.8.0+Atmel3.4.4-1_all.deb` | `24ee56527140f91be522381258f8422984466075aa3d2bb5971653c063c732f9` |
| `libmpfr4_3.1.5-1_amd64.deb` | `95730a4709b898ffaf677f9b3ab6f6ebef5a96866589a8cf5f775448b3413a98` |

The old Arduino 1.0.5 **core source** is retained. The selected compiler uses its
matching avr-libc headers and libraries; it does not replace those with the
Arduino archive's older avr-libc 1.6.4 files. The original `-Os` C++ and `-O3` C
options are retained, and linker relaxation remains disabled.

## Bounded optimization/header matrix

Default control options are the vendor CMake C++ options: ATmega2560, `-Os`,
unsigned char/bitfields, packed structs, short enums, function/data sections,
`ARDUINO=105` and `F_CPU=16000000`. These options are not inferred solely from
binary appearance; they provide the recorded baseline to vary.

| Candidate | Complete exact CRC bodies out of six | Observation |
| --- | --- | --- |
| GCC 4.8.1 / Atmel 3.4.4, default | 6 | All six complete bodies match after relocation |
| GCC 4.9.2 / Atmel 3.5.3, default | 2 | Constructor matches; byte-update and packet register allocation still differ |
| GCC 4.3.2, default | 0 | Inlines byte updates into packet loops/constructors; sizes differ substantially |
| GCC 4.3.2, disabling small-function inlining, all inlining, or SCEV propagation | 0 | Outlining gets some constructor sizes closer but packet loops remain 68 bytes versus stock 52 |
| GCC 7.3.0, default | 2 | Both constructors match after relocation; byte routines are 20/40 bytes versus stock 22/44 |
| GCC 7.3.0, disabling small-function inlining, SCEV, forward propagation, VRP, bit CCP, CCP, GCSE, or split-wide-types | 2 | Does not remove the remaining code-generation differences |
| GCC 7.3.0 with historical avr-libc headers | 2 | CRC16 byte routine becomes 38 bytes; older headers alone do not match stock |
| GCC 7.3.0, `-mstrict-X` | 2 | CRC16 reaches 44 bytes but uses a Y-frame and push/pop absent from stock; size equality is not equivalence |
| GCC 7.3.0, `-O1`, `-O2`, `-Og`, or `-mcall-prologues` | 0 | Does not match this control set |

The GCC 4.3.2 matrix stopped before unsupported `-fno-tree-forwprop`; unbuilt
variants are not counted. A `-mint8` probe failed to compile the unchanged CRC
self-test and is excluded. All tests compile unchanged source to objects; no
stock function bytes were patched into the source or build.

The original comparison used Arduino 1.0.5 *core headers* with GCC 7.3 but still
used GCC 7.3's own `<avr/pgmspace.h>`. An explicit old-avr-libc-header control
separates those two inputs. A compiler version and its header/library package
must be identified together before calling the toolchain reproduced.

The initial matrix prioritized intermediate compiler families, which led to the
GCC 4.8.1 profile above. The control set alone cannot distinguish every package
patch or unused optimizer setting, but it is substantially stronger than size
agreement or masked matches.

## Earlier Arduino core contrasts

The source-built Arduino 1.0.5 `init()` function is 196 bytes and appears exactly
at stock flash byte address `0x22d0`. Its sequence has no linked globals or calls;
this is a stronger SDK/control match than a small constructor. It does not
identify GCC, because fixed register initialization can remain unchanged across
compiler versions.

Stock `delayMicroseconds()` occupies 22 bytes at `0x22ba`; the GCC 4.3.2 build was
20 bytes. The stock adds `SBIW r24,0` after the decrement, while both use the same
four-times scaling, subtraction and delay loop. This is an observed codegen
or source-expression distinction. The selected GCC 4.8.1 profile reproduced it
exactly without normalization.

Stock `delay()` at `0x21e6` and the GCC 4.3.2 reconstruction are both 212 bytes,
with the `micros()` logic inlined twice. The current vendor C flags (`-O3` rather
than the C++ `-Os`) are therefore a plausible baseline; changing C optimization
blindly would discard this signal. The selected profile subsequently reproduced
the complete routines and their timer-state addresses exactly.

The reconstructed GCC 4.3.2 `watchdog_init()`/`watchdog_reset()` bodies match
stock at `0x11758`/`0x11770` exactly (24/4 bytes, no relocations). The explicit
header control matters here: GCC 7.3 with its default avr-libc header emits a
22-byte initializer; the same compiler with Arduino 1.0.5's avr-libc header emits
24 bytes and the same register operations as stock, but schedules the first
three `LDI` instructions differently. The old-style header is therefore relevant
independently of compiler version. Size differences with a newer header cannot
by themselves exclude a compiler family.

Stock CRC packet routines retain full `CALL` instructions to adjacent byte
routines, and constructors retain full tail `JMP` instructions. Global linker
relaxation that shortens such near calls is inconsistent with these sites.

## Reproduction and boundary

The temporary flag-matrix scripts, generated objects and exact-match results are
retained locally under `/private/tmp/homefab-compiler-fingerprint/`. The stock
function addresses above come from direct disassembly and identified call edges;
they are AVR byte addresses, not Ghidra word addresses.

The matching GCC 4.8.1 profile permitted meaningful source-level classification
of the remaining AVR differences. The completed whole-image comparison supplies
the separate equivalence evidence. Runtime memory qualification and physical
hardware behavior remain outside this offline comparison.

## Main-only code-layout controls

After the matching profile reproduced the rest of the image closely, the initial
large remaining byte-difference region was inside the inlined command dispatcher
in `loop()`. A controlled main-only matrix reused the exact recorded compile
arguments and unchanged source. The base object SHA-256 was
`2989a089f8c1938efbe7655ba9e6377fcaa628b6460d79bf28d7311c9f1cd976`.

- Explicit random seeds `0`, `1` and `jedi` produced that identical object.
- Disabling strict aliasing or IPA constant propagation also produced the
  identical object.
- Prefix-including `stdint.h` or `util/atomic.h` with `__STDC_LIMIT_MACROS`
  predefined produced the identical object, so these harmless header controls
  did not explain the dispatcher layout. The initial atomic-prefix attempt
  without that macro failed on missing integer-limit macros and was excluded.
- Disabling block reordering, branch guessing, tree tail merging, cross-jumping,
  dominator optimization or tree PRE changed the 12962-byte loop size to
  13036, 12928, 13050, 13110, 12974 and 13050 bytes respectively.
- Disabling top-level reordering, IPA SRA or partial inlining retained the loop
  size but changed object bytes. These were not selected as equivalence fixes.

No speculative flag change or unused declaration was added to production source.
The dispatcher source-form/CFG comparison is recorded separately in the motion
ledger. Host-word-size variation was considered but no 32-bit-host compiler
result was needed or claimed by this control matrix. Raw commands, objects,
logs and results are retained locally under `artifacts/main-flag-controls/`.

## Exact linker-generated trampoline reproduction

After restoring the command-dispatch and thermal source forms, the image had
the exact stock size, static SRAM total, code, initialized data and addresses.
Only 172 bytes differed, entirely inside `.trampolines` at flash byte range
`0x210–0x39b`. That section contained 99 four-byte `JMP` instructions with
identical destination multisets; 94 positions were permuted. This remaining
difference was traced to the actual linker algorithm rather than patched away.

The primary source was Debian's exact Atmel binutils source package:

- [Source descriptor](https://archive.debian.org/debian/pool/main/b/binutils-avr/binutils-avr_2.24+Atmel3.4.4-1.dsc).
- Original source archive SHA-256:
  `13d627ac00096c66171771005daed4df1ad202d579cc8d03013ea7993929833e`.
- Debian patch SHA-256:
  `ff5c578d694c8a76b1647f18ee299ee52c17020d37c17d8af2298f8166e5cc3b`.

The nested `avr-binutils-2.24.tar.bz2` already contains Atmel's AVR backend.
Debian's changes do not modify the stub-name or hash algorithms. Inspection of
`bfd/elf32-avr.c`, `bfd/hash.c`, `bfd/section.c` and `ld/emultempl/avrelf.em`
established the following:

1. `avr_stub_name()` uses the input BFD section ID plus the target's offset in
   that section as two eight-digit hexadecimal fields. It does not use the
   final function address or symbol name as the hash key.
2. BFD section IDs begin at `0x10` and increase for every allocated input
   section, including sections subsequently discarded by garbage collection.
3. The stub table has 4051 buckets for this invocation. BFD's hash traversal
   emits stubs in increasing bucket order, with linked-list order for a
   collision. Consequently, removing an unused function can reorder generated
   stubs while leaving every retained function and data byte unchanged.
4. This non-relaxing AVR link preallocates these stubs. All 99 destinations are
   below `0x20000`, the backend's threshold for routing these relocations through
   a stub. `--debug-stubs` confirmed the generated destinations and offsets.

A read-only model counted the actual object-file BFD sections, reconstructed
the backend's keys and hash values, and reproduced the observed order of all
98 targets originating in project/Arduino objects. The first project section ID
is 24: initial ID 16, one synthetic trampoline section and seven startup-object
sections. The remaining target,
`0x11a20`, belongs to libgcc's `_divdi3.o` rather than those objects. The model
then reproduced all 98 stock positions by subtracting exactly one section ID
after `flow.cpp` and before the thermal/Arduino objects. Main, Stop and flow
section IDs remained unchanged. This narrowed the cause to one extra input
section rather than a different SDK, host word size or optimization setting.

The extra section was the uncalled historical `movesplanned()` wrapper retained
by the reconstruction: `.text._Z12movesplannedv`, 14 bytes, discarded by the
linker. No callers exist, and the stock symbolized simulator has no
`movesplanned()` symbol. Removing that unsupported wrapper and declaration
reduced the project/core input-section count from 923 to 922. A normal, unchanged
linker invocation then reproduced all 99 stubs and the entire literal stock HEX.
No stub bytes, padding or output sections were manually rewritten.

Two independent negative controls support this explanation. The same Debian
binutils version's i386 linker, run through `qemu-i386` against the unchanged
64-bit-built objects, produced the same candidate and the same 172 differences.
Adding `-g` to both C and C++ compilation changed the permutation to 186 differing
stub bytes while retaining all other flash bytes; that profile was rejected.

The extracted source files, descriptor, section/hash model, per-target keys,
before/after predictions and checksums are retained locally in
`artifacts/stub-order-evidence/`. The model never writes firmware or object
files. The conclusive result is the ordinary production build's full addressed
and carrier comparison in `artifacts/stock-exact-a/equivalence.json`, rather
than the model's prediction alone. Hardware behavior has not been tested.

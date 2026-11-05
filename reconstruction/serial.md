# Stock serial and capacitive-driver compilation repairs

Source base: UM3 `4545818112027387f09e87f930117ab8f3804327`.
Comparison: S-line `f91240bb04e52818746a866bfe6e6af543209b79`.
Shipped evidence: official UM3 5.3.0 image's ARM `jedi-marlin-simulator`, SHA-256
`bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.

The REA adapter was already unavailable for installed Ghidra 12.2; existing
Ghidra exports and direct LLVM inspection were reused. These are direct static
analysis observations, not REA Evidence IDs or physical AVR execution.

## Serial storage and framing

The stale source's 128-byte ring and 16-bit `int` indices differ from shipped
behavior. ARM symbol `rx_buffer` is at `0x57758`, size `0x102` (258 bytes).
`MarlinSerial::read()` at `0x1ec74` reads its head/tail as single bytes at offsets
`0x100` and `0x101`, and wraps the increment through an unsigned byte.
`flush()` at `0x1ecf4` copies byte tail to byte head, discarding receive data.
The buffer is restored to 256 bytes with volatile byte indices, also matching
S-line's representation. Reserving one slot gives 255 usable queued bytes.

ARM `MarlinSerial::println()` at `0x1eea8` calls `print(char,int)` once with
character 10 and base 0. It does not emit character 13 first. The reconstructed
historical source therefore uses LF only, matching S-line and the shipped
simulator rather than the stale tree's CRLF. The modern port's earlier CRLF
fixtures were based on the public vendor source; the stock host strips line
endings, but those fixtures do not establish exact stock newline parity.

Existing AVR `int/long` overloads are retained: their widths are 16/32 bits.
No broad fixed-width rewrite was needed for the AVR baseline. This does not
claim source/ABI equivalence for rebuilding the ARM simulator.

`reconstruction/run_serial_test.py` compiles the actual `MarlinSerial.cpp` under
native ASan/UBSan with mocked UART register boundaries. It checks storage size,
255-byte capacity, overflow rejection, three wrap cycles, peek/read ordering and
empty behavior, the polling receive path, receive-discard `flush`, and exact LF
output. It passed on macOS/Clang. It does not emulate interrupt timing or a real
UART. Root's Docker build separately verifies AVR compilation/linking.

## Protocol diagnostic strings

Five missing macros in `language.h` are recovered from exact NUL-terminated
strings in the stock ELF; all bytes including trailing spaces were checked.
They are independent of LCD language selection, as binary protocol diagnostics
are host-facing rather than translated UI text.

| Macro | ARM virtual address |
| --- | --- |
| `MSG_ERR_UNEXPECTED_SEQUENCE_NUMBER` | `0x40264` |
| `MSG_ERR_PAYLOAD_SIZE_TOO_BIG` | `0x402a0` |
| `MSG_ERR_PAYLOAD_ILLEGAL_CHARACTER` | `0x402dc` |
| `MSG_ERR_CRC_MISMATCH` | `0x4031c` |
| `MSG_ERR_PACKET_UNHANDLED` | `0x40350` |

The unchanged vendor `SerialProtocol.cpp` supplies each corresponding diagnostic
call site. `MSG_ERR_STOPPED` already exists in the English language definitions,
with exact stock text; its failure was header visibility. The common-header
repair includes `language.h` and supplies the missing two-argument
`SERIAL_ECHO_F` macro, reusing the existing print overloads. No unsolicited
diagnostics or new protocol states are added.

## Capacitive I2C declarations were configuration-gated

No capacitive-driver source change was needed. Its UM3 source already implements
`i2cCapacitanceSetCAPDAC` and `i2cCapacitanceDone`, but the stale configuration did
not define `ENABLE_BED_LEVELING_PROBE`, hiding both bodies and declarations.
The configuration repair restores that feature.

The existing implementation is independently corroborated by ARM exports:
`i2cCapacitanceSetCAPDAC` at `0x2d4b0` writes register 8 at address `0xa0`, using
`0x1c,0` for disabled CAPDAC or `0x10|((value>>3)&3), value<<5` otherwise.
`i2cCapacitanceDone` at `0x2d514` implements the same first-run/status/read state
machine, waits for both queued commands, tests status bit 8, then combines the
MSB/LSB into a signed 16-bit sample. This is not a newly invented stub.

The driver's existing TODO about lack of a local measurement timeout remains;
I2C bus-level fault behavior, complete probe timing, AVR parity and physical
hardware acceptance still require validation. A successful Docker build alone
cannot close those gates.

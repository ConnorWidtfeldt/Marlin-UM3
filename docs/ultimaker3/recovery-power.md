# Recovered UM3 power-budget model

`Marlin/src/feature/ultimaker3/power_budget.{h,cpp}` implements the two stock
hotend slots and temperature-dependent bed model. It is isolated calculation
code, compiled only with `UM3_COMPATIBILITY` or the host-test flag. No heater
output, G-code handler, ADC, or shutdown path calls it yet.

## Evidence and confidence

The public vendor power-budget implementation is complete: all six
`Marlin/PowerBudgetManagement*.{h,cpp}` files are byte-identical between UM3
commit `4545818112027387f09e87f930117ab8f3804327` and S-line comparison commit
`f91240bb04e52818746a866bfe6e6af543209b79`. The port uses this source with
confirmation from the binary; these particular formulas did not require
guessing missing source.

Binary: stock UM3 `5.3.0.20211003` package's ARM executable
`usr/bin/jedi-marlin-simulator`, SHA-256
`bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee`.
The following are ELF virtual addresses, not AVR addresses or file offsets:

| Symbol | Address | Symbol size |
| --- | --- | --- |
| `PowerBudgetManagement::PowerBudgetManagement(uint8_t)` | `0x36660` | `0x58` |
| `PowerBudgetManagement::getActualBedOutput()` | `0x366b8` | `0x7a` |
| `PowerBudgetManagementBed::PowerBudgetManagementBed()` | `0x36734` | `0x44` |
| `PowerBudgetManagementBed::getActualHeaterOutput()` | `0x36778` | `0x160` |
| `PowerBudgetManagementHotendSlot::PowerBudgetManagementHotendSlot()` | `0x368d8` | `0x4a` |
| `PowerBudgetManagementHotendSlot::getActualHeaterOutput()` | `0x36924` | `0x54` |
| `pwr` | `0x589b8` | `0x54` |

LLVM symbol/disassembly inspection and direct Ghidra 12.2 decompilation confirm
the source's defaults, two-slot count, subtraction of hotend consumption, bed
60 C reference, 255-duty scale, and truncation to an integer output. These are
ordinary static-analysis observations, **not REA Evidence records**. The REA
adapter rejected the installed Ghidra version; direct Ghidra output is retained
separately in this investigation's evidence artifacts.

The checked-in `buildroot/tests/um3/stock_oracle.json` adds controlled execution
of the actual ARM bed routine under Unicorn. Twelve outputs cover temperatures
0, 20, 60, and 115 C with remaining budgets 25, 100, and 175 W. The test compares
the production implementation against those outputs. This executes a bounded
routine with a supplied object, not the simulator entry point or real hardware.

The packaged AVR HEX has SHA-256
`448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65`.
An AVR disassembly was inspected, but these power functions have not yet been
reliably mapped to AVR flash addresses. A relocation-masked match of power
objects built with the original Arduino 1.0.5 toolchain found no full-function
match. This bounded negative result does not establish a behavioral difference;
compiler/optimization differences remain possible. **Exact AVR numerical parity
is unresolved.**

## Host contract and recovered calculation

The packaged `griffin/printer/drivers/marlin/applicationLayer.py` sends each
parameter separately, so initialization must tolerate partial configuration:

| Command | Host-owned fields | New setter |
| --- | --- | --- |
| `M290 P` / `I` | total watts / idle watts | `set_total_power_budget` / `set_idle_power_consumption` |
| `M291 R` / `A` / `V` | bed ohms at 60 C / ohms per C / volts | `set_nominal_bed_resistance` / `set_bed_resistance_per_degree` / `set_bed_voltage` |
| `M292 T R` / `V` / `P` | slot ohms / volts / maximum watts | `set_nominal_hotend_resistance` / `set_hotend_voltage` / `set_max_hotend_power` |

The shipped `machines/um3.json` supplies total 221 W, idle 46 W, maximum 75 W
per slot, 24 V for bed and slots, bed resistance 3.678255 ohms at 60 C, and bed
coefficient 0.012285 ohms/C. Its shipped print-core definitions use 23 ohms.
These are **host defaults**, not constants in the new firmware module;
individual-core/machine calibration remains host-owned.

For a slot, full-on power is `V*V/R`. The slot's maximum duty is the smaller of
255 and `255*slot_max_watts/full_on_watts`, truncated down. Actual duty is the
smaller of this limit and its requested duty. `R=0` is the vendor sentinel for
a disconnected core and produces zero duty, rather than a short-circuit model.
Consumed watts are full-on watts times actual duty divided by 255.

Bed resistance is `R60 + slope*(temperature-60)`. Bed full-on power is `V*V/R`.
The original manager subtracts idle consumption and the last calculated slot
consumptions from the total. The bed gets only the remainder, with duty
`min(requested, clamp(255*remaining/full_on_watts, 0, 255))`, truncated down.
These are estimated average power limits, not instantaneous current measurements.

## Explicit safety hardening

The new code intentionally changes cases outside valid stock configuration:

- All outputs remain zero until total and explicit idle values are available;
  each slot also needs its resistance, voltage and power limit. The bed also
  needs its resistance, voltage and an explicit coefficient, which may be zero.
- Setters reject NaN, infinity, negative watts/resistance/coefficient, and
  nonpositive voltage/bed resistance. Rejection clears the affected setting so
  it cannot retain a previous heating allowance. A false return must eventually
  become a host-visible configuration error. Zero watts disables heating;
  zero hotend resistance retains the stock disconnected-core meaning.
- Derived nonfinite, zero, or negative resistance/power models give zero output.
  An invalid bed temperature inhibits the bed. This is a calculation guard;
  sensor-fault shutdown of all heaters belongs to the thermal engine.
- The original code can exceed the total when the slots alone exceed available
  power. The new allocator caps each slot by the remaining total, considering
  slot 0 before slot 1, then the bed. Normal stock budgets preserve both slots'
  priority over the bed. This overload policy is additional safety behavior,
  not a recovered vendor policy.
- One `calculate()` call returns all duties from current requests. It avoids
  the vendor getters' hidden dependency on slot getters running before the bed
  getter and uses conservative float rounding at a duty boundary.

Finite precision and conservative rounding may differ by a PWM count at an
exact mathematical boundary. Model limits do not replace thermal runaway
protection, removal interlocks, emergency shutdown, watchdogs, or power-stage
hardware checks.

## Verification and remaining integration

Run `python3 buildroot/tests/um3/run_power.py`. It builds the production module
with ASan/UBSan and checks stock binary-derived bed fixtures, stock core/slot
limits, disconnection, partial initialization, invalid settings/sensor values,
overflowed derived models, and 26,880 combinations of budgets, temperatures,
slot resistances, and requested duties. The allocation sweep independently
checks modeled total/slot watts and output bounds.

The production translation unit also compiles with AVR GCC 7.3.0 for
ATmega2560 at 16 MHz using `-std=gnu++11 -Os -DUM3_COMPATIBILITY` and warnings
as errors. This is an object-file check, not a linked UM3 firmware build.

Outstanding: map/corroborate the AVR implementation; attach M290–M292 with
host-visible validation errors; feed calibrated bed temperature; pass every
heater request through the same calculation snapshot and final safety gate;
verify real PWM/timer behavior and memory usage; and exercise failures on
hardware. The full UM3 build and flashing gates remain closed.

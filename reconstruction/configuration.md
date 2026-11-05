# Historical UM3 configuration and shared declarations

This branch reconstructs a comparison build from vendor
`4545818112027387f09e87f930117ab8f3804327`. It is separate from modern Marlin.
Configuration/header candidates come from S-line
`f91240bb04e52818746a866bfe6e6af543209b79`; values below are independently
corroborated by the pinned stock 5.3.0 package. No machine EEPROM calibration
is introduced.

| Restored setting/interface | Evidence |
| --- | --- |
| USART2, 250000 baud, two tools | 1548 mainboard host wiring, stock machine description and main/serial instructions |
| Two compiled flow slots | Stock `Flow` constructor at ARM `0x2a408` loops over two settings; stock main starts with `dummy_flow` (`FlowBase`), and the host selects sensor support |
| Hotend table21, bedtable20, oversampling8 | Exact table bytes in ARM and AVR, thermal conversion instructions; thermal ledger |
| PID range25, jump5, ambient35, K1=.99, cadence32.768ms | Stock PID/setter instructions and native replay fixtures on modern branch |
| Heating watchdog rise1/30000ms; auto fan40/35 | Stock ARM and AVR thermal instructions; thermal ledger |
| ROM steps80/80/200/369 and max230/230/240 | Exact stock AVR arrays at `0x12cbb`/`0x12c6f`; host later overrides Extended geometry and calibration |
| Max feed300/300/40/45, homing6000/6000/2400/0 | Exact stock AVR arrays at `0x12ccb`/`0x12c7f` |
| Acceleration9000/9000/100/10000, default/retract3000, jerk20/.4/5 | Named stock ARM initialized arrays/scalars at `0x5752c`, `0x5753c`–`0x5754c` |
| Extrude minimum170, segment20000us | Stock ARM `extrude_min_temp` `0x57550`, `minsegmenttime` `0x57508` |
| Fan kickstart100ms | Stock `check_axes_activity` `0x2fa34`; motion ledger |
| Position tolerance50mm | Stock `get_coordinates` `0x26578`, float literal50 at `0x26734` |
| Serial hot thresholds50C / timeout5min | Stock `check_serial_input_timeout` `0x26c84`, literal50 at `0x26d98`, comparison299999ms at `0x26d5c` |
| Probe buffers/filters/thresholds | Stock ARM instruction constants and buffer sizes; modern `recovery-motion.md` ledger |
| Scalar extrusion multiplier, integer XYZ tool offsets, Stop.h reasons | Existing UM3 main definitions and shipped ARM named objects/command handlers |

The stale UM2 LCD/SD/EEPROM headers/defaults were replaced by the smaller shared
header/configuration surface used by the UM3 main. Board detection continues to
use the published UM3 implementation. Its undefined ADC input name is corrected
to ADC14 (`ADC_BOARD_ID_PIN`), independently confirmed in the 1548 schematic.
The hardware discrepancy between the published revision-I schematic and vendor
revision detection remains unresolved. This source reconstruction does not
qualify a physical board revision or authorize the PA6 workaround on hardware.

Stock step/direction/enable/endstop/heater/current pins are mapped as documented
in the modern branch's hardware audit. PA2 and PA6 explicitly use Arduino digital
24 and28, avoiding ambiguous `A2`/`A6` names. Physical F/D/J tokens used by existing
vendor drivers now have fast-I/O aliases for the corresponding MCU PIN/PORT/DDR
registers. The stock A4988 current range is 2000 mA; initialization remains runtime
board-dependent. Existing non-stock 2621B paths are retained for comparison with
the shipped binary, without adding S-line BOARD_V4 support.

The flow-settings initializer changes only from designated to positional syntax
in the exact struct member order, making it accepted by AVR GCC 4.3.2. Compiled
flow support remains present as in stock; this does not add physical sensors to
UM3 machines. Host-supplied current, direction, temperature compensation, PID,
geometry, nozzle offsets and calibration remain runtime settings.

Not every inherited planner/driver configuration branch has instruction-level
AVR equivalence. Those are explicit comparison candidates. No release inventory
entry is closed merely by obtaining a successful compile/link.

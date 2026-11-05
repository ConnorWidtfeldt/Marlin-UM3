# UM3 stock compatibility implementation

Imported from the byte-identical stock reconstruction at
`7a80a3bb615362f513e1608447a6eb92efa35098`, based on UltimakerMarlin
`4545818112027387f09e87f930117ab8f3804327` and binary-verified repairs.
Original GPL notices are retained.

The stock command frontend, protocol, peripherals, probing and thermal behavior
are isolated in `um3_stock`. The legacy planner, stepper engine and arc engine
are excluded; adapters use modern Marlin's implementations. UART, timers,
watchdog and thermal safety have explicit integration hooks. This directory
is built only for UM3_COMPATIBILITY. See the integration ledger before use.

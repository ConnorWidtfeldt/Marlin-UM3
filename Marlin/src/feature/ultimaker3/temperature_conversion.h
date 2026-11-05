/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * ADC tables recovered byte-identically from ARM simulator and stock AVR flash.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

// raw_sum uses the stock factor 8; no dependence on modern Marlin OVERSAMPLENR.
// Hotends use table 21; bed below uses internal-ADC/INA826 table 20. The later
// board-ID-4 external bed ADC uses table 21 and must select the hotend converter.
float hotend_temperature_from_raw_sum(int16_t raw_sum, float gain = 1, float offset = 0);
float bed_temperature_from_raw_sum(int16_t raw_sum, float gain = 1, float offset = 0);

// One signed 12-bit ADS101X sample, scaled internally; invalid samples return NaN.
float hotend_temperature_from_adc(int16_t sample, float gain = 1, float offset = 0);

} // namespace ultimaker3

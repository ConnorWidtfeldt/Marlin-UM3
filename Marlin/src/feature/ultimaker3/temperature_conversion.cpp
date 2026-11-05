/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * S-line f91240bb04e52818746a866bfe6e6af543209b79 table 20/21 definitions.
 * Exact bytes corroborated at stock AVR flash 0x1e78/0x1e44 and ARM 0x403f8/0x4046c.
 * Interpolation corroborated by ARM analog2temp_sub 0x34a80.
 */
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "temperature_conversion.h"
#include <math.h>
#ifdef __AVR__
  #include <avr/pgmspace.h>
  #define UM3_PROGMEM PROGMEM
#else
  #define UM3_PROGMEM
#endif

namespace ultimaker3 {

static const int16_t BED_TABLE[][2] UM3_PROGMEM = {
  {0, 0}, {1816, 1}, {1960, 20}, {2096, 40}, {2232, 60}, {2360, 80},
  {2496, 100}, {2632, 120}, {2760, 140}, {2888, 160}, {3016, 180},
  {3144, 200}, {3272, 220}, {3392, 240}, {3520, 260}, {3640, 280},
  {3760, 300}, {3880, 320}, {4000, 340}, {4120, 360}, {4232, 380},
  {4352, 400}, {4912, 500}, {5448, 600}, {5952, 700}, {6440, 800},
  {6896, 900}, {7336, 1000}, {7744, 1100}
};

static const int16_t HOTEND_TABLE[][2] UM3_PROGMEM = {
  {-17448, -50}, {-14472, 0}, {-11632, 50}, {-8904, 100}, {-6296, 150},
  {-3800, 200}, {-1400, 250}, {896, 300}, {3104, 350}, {5216, 400},
  {7248, 450}, {9200, 500}, {16288, 700}
};

static int16_t read_table(const int16_t *value) {
  #ifdef __AVR__
    return int16_t(pgm_read_word(value));
  #else
    return *value;
  #endif
}

static float interpolate(const int16_t raw, const int16_t table[][2], const uint8_t length) {
  for (uint8_t i = 1; i < length; ++i) {
    const int16_t upper_raw = read_table(&table[i][0]);
    if (upper_raw > raw) {
      const int16_t lower_raw = read_table(&table[i - 1][0]);
      const int16_t lower_temperature = read_table(&table[i - 1][1]);
      return lower_temperature + (raw - lower_raw)
        * float(read_table(&table[i][1]) - lower_temperature) / float(upper_raw - lower_raw);
    }
  }
  return read_table(&table[length - 1][1]);
}

static float compensate(const float temperature, const float gain, const float offset) {
  return isfinite(gain) && isfinite(offset) ? temperature * gain + offset : NAN;
}

float hotend_temperature_from_raw_sum(const int16_t raw, const float gain, const float offset) {
  return compensate(interpolate(raw, HOTEND_TABLE, sizeof(HOTEND_TABLE) / sizeof(*HOTEND_TABLE)), gain, offset);
}

float bed_temperature_from_raw_sum(const int16_t raw, const float gain, const float offset) {
  return compensate(interpolate(raw, BED_TABLE, sizeof(BED_TABLE) / sizeof(*BED_TABLE)), gain, offset);
}

float hotend_temperature_from_adc(const int16_t sample, const float gain, const float offset) {
  return sample >= -2048 && sample <= 2047 ? hotend_temperature_from_raw_sum(sample * 8, gain, offset) : NAN;
}

} // namespace ultimaker3

#undef UM3_PROGMEM
#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST

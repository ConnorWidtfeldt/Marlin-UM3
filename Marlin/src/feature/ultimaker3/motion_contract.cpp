// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "motion_contract.h"

#include <math.h>
#include <string.h>

namespace ultimaker3 {

// Compiled stock defaults only: Griffin later supplies Extended geometry,
// Z=400 steps/mm, and the individual printer's calibration.
const float STOCK_STEPS_PER_MM[4] = {80.0f, 80.0f, 200.0f, 369.0f};
const float STOCK_HOMING_FEEDRATE_MM_MIN[4] = {6000.0f, 6000.0f, 2400.0f, 0.0f};
const float STOCK_MAX_FEEDRATE_MM_S[4] = {300.0f, 300.0f, 40.0f, 45.0f};
const float STOCK_MAX_POS_MM[3] = {230.0f, 230.0f, 240.0f};

constexpr uint8_t ProbeSettings::SAMPLE_COUNT_1;
constexpr uint8_t ProbeSettings::SAMPLE_COUNT_2;
constexpr uint8_t ProbeSettings::RESIDUAL_BUFFER_SIZE;
constexpr uint16_t ProbeSettings::SAMPLE_FREQUENCY_HZ;
constexpr uint16_t ProbeSettings::RESIDUAL_MULTIPLIER;
constexpr float ProbeSettings::ALPHA;
constexpr float ProbeSettings::BETA;
constexpr int32_t ProbeSettings::PEAK_THRESHOLD;
constexpr uint8_t ProbeSettings::PEAK_DROP_NUMERATOR;
constexpr uint8_t ProbeSettings::PEAK_DROP_DENOMINATOR;
constexpr uint8_t ProbeSettings::MAX_NOISE_STD;
constexpr float ProbeSettings::DEFAULT_MOVE_MM;

uint8_t planner_free_slots(const uint8_t head, const uint8_t tail) {
  return 15 - (uint8_t(head - tail) & 15);
}

bool tool_offset_steps(const float millimeters, const float steps_per_mm, int32_t &offset) {
  const float scaled = millimeters * steps_per_mm;
  // The negative rounded result must fit in int32_t on both AVR and the host.
  if (!isfinite(millimeters) || !isfinite(steps_per_mm) || steps_per_mm <= 0
      || !isfinite(scaled) || scaled <= -2147483648.0f || scaled >= 2147483648.0f)
    return false;
  offset = -int32_t(lround(scaled));
  return true;
}

bool logical_position_from_steps(const int32_t physical_steps[4], const int32_t tool_offset[3],
                                 const float steps_per_mm[4], float logical_position[4]) {
  float result[4];
  for (uint8_t axis = 0; axis < 4; ++axis) {
    if (!isfinite(steps_per_mm[axis]) || steps_per_mm[axis] <= 0) return false;
    const int64_t corrected = int64_t(physical_steps[axis]) + (axis < 3 ? tool_offset[axis] : 0);
    if (corrected < INT32_MIN || corrected > INT32_MAX) return false;
    result[axis] = float(corrected) / steps_per_mm[axis];
    if (!isfinite(result[axis])) return false;
  }
  memcpy(logical_position, result, sizeof(result));
  return true;
}

} // namespace ultimaker3

#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST

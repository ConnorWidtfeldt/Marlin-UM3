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

float junction_speed(const float current[4],const float previous[4],float nominal,
                     float previous_nominal,uint8_t queued,float xyjerk,float zjerk,float ejerk) {
  float speed=xyjerk/2;
  if(fabsf(current[2])>zjerk/2 && zjerk/2<speed) speed=zjerk/2;
  if(fabsf(current[3])>ejerk/2 && ejerk/2<speed) speed=ejerk/2;
  if(nominal<speed) speed=nominal;
  if(queued>1 && previous_nominal>0.0001f) {
    float factor=1;
    const float xy=hypotf(current[0]-previous[0],current[1]-previous[1]);
    const float z=fabsf(current[2]-previous[2]), e=fabsf(current[3]-previous[3]);
    if(xy>xyjerk) factor=xyjerk/xy;
    if(z>zjerk && zjerk/z<factor) factor=zjerk/z;
    if(e>ejerk && ejerk/e<factor) factor=ejerk/e;
    speed=nominal*factor;
    if(previous_nominal<speed) speed=previous_nominal;
  }
  return speed;
}

bool physical_target_steps(const float logical[4],const int32_t tool_offset[3],
                           const float steps_per_mm[4],int32_t physical[4]) {
  int32_t result[4];
  for(uint8_t a=0;a<4;a++) {
    const float scaled=logical[a]*steps_per_mm[a];
    if(!isfinite(logical[a]) || !isfinite(steps_per_mm[a]) || steps_per_mm[a]<=0
       || !isfinite(scaled) || scaled < -2147483648.0f || scaled >= 2147483648.0f) return false;
    const int64_t value=(a<3 ? int64_t(lround(scaled)) : int64_t(scaled))-(a<3 ? tool_offset[a] : 0);
    if(value<INT32_MIN || value>INT32_MAX) return false;
    result[a]=int32_t(value);
  }
  memcpy(physical,result,sizeof(result));
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

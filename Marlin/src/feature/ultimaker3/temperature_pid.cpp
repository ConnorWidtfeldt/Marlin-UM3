/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Stock ARM update 0x35dac; sample interval literals 0x24b80/0x24f08;
 * K1/FF constants 0x36470/0x36478/0x36488. This does not drive heater outputs.
 */
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "temperature_pid.h"
#include <math.h>

namespace ultimaker3 {

static float clamp(const float value, const float minimum, const float maximum) {
  return value < minimum ? minimum : value > maximum ? maximum : value;
}

TemperaturePID::TemperaturePID()
  : Kff_(0), Kp_(0), Ki_(0), Kd_(0), Ki_max_(255), Kpcf_(0),
    functional_range_(25), min_input_(1), max_input_(0), target_temperature_(0),
    i_state_(0), d_state_(0), previous_sample_(0), debug_dump_(false) {}

uint8_t TemperaturePID::update(const float current_temperature, const uint8_t cooling_fan_speed) {
  // The stock constructor leaves bounds unset; reject until explicitly configured.
  if (!isfinite(current_temperature) || !isfinite(target_temperature_)
      || !isfinite(min_input_) || !isfinite(max_input_) || min_input_ > max_input_
      || !isfinite(Kff_) || !isfinite(Kp_) || !isfinite(Ki_) || !isfinite(Kd_)
      || !isfinite(Ki_max_) || Ki_max_ < 0 || !isfinite(Kpcf_)
      || !isfinite(functional_range_) || functional_range_ < 0) {
    resetState();
    previous_sample_ = 0;
    return 0;
  }

  const float error = target_temperature_ - current_temperature;
  const float temperature_delta = current_temperature - previous_sample_;
  if (fabsf(temperature_delta) > 5.0f) {
    previous_sample_ = current_temperature;
    return 0;
  }

  uint8_t result;
  if (error > functional_range_) {
    resetState();
    result = 255;
  }
  else if (error < -functional_range_ || target_temperature_ == 0) {
    result = 0;
  }
  else {
    const float p_term = Kp_ * error;
    i_state_ = clamp(i_state_ + error * Ki_, -Ki_max_, Ki_max_);
    // double is 32-bit on AVR, matching the vendor's expression and rounding.
    d_state_ = (1.0 - 0.99) * (Kd_ * temperature_delta) + 0.99 * d_state_;
    const float ff_term = Kff_ * (target_temperature_ - 35);
    const float fan_term = Kpcf_ * cooling_fan_speed;
    const float output = ff_term + fan_term + p_term + i_state_ - d_state_;
    // Invalid gains/state must never reach an undefined float-to-integer cast.
    if (!isfinite(output)) {
      resetState();
      previous_sample_ = current_temperature;
      return 0;
    }
    result = uint8_t(clamp(output, 0, 255));
  }

  if (current_temperature < min_input_ || current_temperature > max_input_)
    result = 0;
  previous_sample_ = current_temperature;
  return result;
}

void TemperaturePID::resetState() {
  i_state_ = 0;
  d_state_ = 0;
}

} // namespace ultimaker3

#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST

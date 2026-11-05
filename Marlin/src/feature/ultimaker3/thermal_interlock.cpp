/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Stock ARM removal request 0x3443c, auto-revoke 0x34844, watchdog 0x344cc.
 * Valid finite behavior is recovered from shipped instructions, not S-line guesses.
 */
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "thermal_interlock.h"
#include <math.h>

namespace ultimaker3 {

bool HotendRemovalInterlock::request(const bool enable, const float temperature, const float target) {
  if (enable && (!isfinite(temperature) || !isfinite(target) || temperature > 70 || target != 0))
    return false;
  allowed_ = enable;
  return true;
}

void HotendRemovalInterlock::observe(const bool present, const float temperature) {
  // Unknown temperature cannot authorize physically removing a potentially hot core.
  if (!isfinite(temperature) || (present && temperature >= 62))
    allowed_ = false;
}

bool HeatingProgressWatchdog::update(const uint8_t delivered_pwm, const float temperature, const uint32_t now_millis) {
  // Explicit defensive addition: invalid input is a fault even when output is off.
  if (!isfinite(temperature)) {
    start_millis_ = 0;
    start_temperature_ = 0;
    return true;
  }
  if (delivered_pwm != 255) {
    start_millis_ = 0;
    return false;
  }
  if (start_millis_ == 0 || temperature - start_temperature_ > 1) {
    start_millis_ = now_millis;
    start_temperature_ = temperature;
    return false;
  }
  return uint32_t(now_millis - start_millis_) > uint32_t(30000);
}

} // namespace ultimaker3

#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST

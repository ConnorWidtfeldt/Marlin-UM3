/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Reconstructed from stock ARM functions 0x3443c, 0x344cc and 0x345c8.
 * See docs/ultimaker3/recovery-thermal.md. This module performs no hardware I/O.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

// One instance per hotend. The caller must forbid heating whenever removal is
// allowed, and separately reject invalid sensors before applying any output.
class HotendRemovalInterlock {
public:
  HotendRemovalInterlock() : allowed_(true) {}

  // Return whether the request was accepted. Rejected enable requests preserve
  // the previous permission; disabling permission is unconditional.
  bool request(bool enable, float temperature, float target);
  void observe(bool present, float temperature);
  bool removalAllowed() const { return allowed_; }

private:
  bool allowed_;
};

class HeatingProgressWatchdog {
public:
  HeatingProgressWatchdog() : start_millis_(0), start_temperature_(0) {}

  // Use the LAST ACTUALLY DELIVERED output, not the requested/budgeted PWM.
  // Returns a fault; the caller owns immediate shutdown and fault latching.
  bool update(uint8_t delivered_pwm, float temperature, uint32_t now_millis);

  uint32_t startMillis() const { return start_millis_; }
  float startTemperature() const { return start_temperature_; }

private:
  uint32_t start_millis_;
  float start_temperature_;
};

} // namespace ultimaker3

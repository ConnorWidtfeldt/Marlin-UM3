/**
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from UltimakerMarlin 4545818112027387f09e87f930117ab8f3804327.
 * Corroborated by stock ARM TemperaturePID::{constructor,update} at 0x35d10/0x35dac.
 * See docs/ultimaker3/recovery-thermal.md for evidence and integration limits.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

class TemperaturePID {
public:
  static constexpr double SAMPLE_INTERVAL_SECONDS = 0.032768;

  TemperaturePID();
  uint8_t update(float current_temperature, uint8_t cooling_fan_speed);

  void setTargetTemperature(float value) { target_temperature_ = value; }
  float getTargetTemperature() const { return target_temperature_; }
  void setKff(float value) { Kff_ = value; }
  void setKp(float value) { Kp_ = value; }
  void setKi(float value) { Ki_ = value * SAMPLE_INTERVAL_SECONDS; }
  void setKd(float value) { Kd_ = value / SAMPLE_INTERVAL_SECONDS; }
  void setKiMax(float value) { Ki_max_ = value; }
  void setKpcf(float value) { Kpcf_ = value; }
  void setFunctionalRange(float value) { functional_range_ = value; }
  void setControlRange(float minimum, float maximum) { min_input_ = minimum; max_input_ = maximum; }
  // The future serial handler owns the diagnostic output; this module has no I/O.
  void setDebugDump(bool enabled) { debug_dump_ = enabled; }
  bool debugDumpEnabled() const { return debug_dump_; }

private:
  void resetState();

  float Kff_, Kp_, Ki_, Kd_, Ki_max_, Kpcf_;
  float functional_range_, min_input_, max_input_, target_temperature_;
  float i_state_, d_state_, previous_sample_;
  bool debug_dump_;
};

} // namespace ultimaker3

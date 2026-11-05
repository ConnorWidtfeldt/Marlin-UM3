/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * See power_budget.h for source and shipped-binary provenance.
 */
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "power_budget.h"
#include <math.h>

namespace ultimaker3 {

static bool set_nonnegative(float &destination, const float value) {
  const bool valid = isfinite(value) && value >= 0;
  destination = valid ? value : 0;
  return valid;
}

static bool set_positive(float &destination, const float value) {
  const bool valid = isfinite(value) && value > 0;
  destination = valid ? value : 0;
  return valid;
}

static float full_power(const float voltage, const float resistance) {
  if (!(voltage > 0) || !(resistance > 0) || !isfinite(resistance)) return 0;
  const float watts = voltage * voltage / resistance;
  return isfinite(watts) && watts > 0 ? watts : 0;
}

static float used_power(const float full, const uint8_t duty) {
  return full * (duty / 255.0f);
}

static uint8_t limit_output(const uint8_t requested, const float full, const float available) {
  if (!(full > 0) || !(available > 0)) return 0;
  if (available >= full) return requested;
  const float cap = 255.0f * available / full;
  if (!isfinite(cap)) return 0;
  uint8_t duty = cap < requested ? uint8_t(cap) : requested;
  // Round conservatively when float division lands just above a PWM boundary.
  while (duty && used_power(full, duty) > available) --duty;
  return duty;
}

PowerBudget::PowerBudget()
  : slots_{}, total_(0), idle_(0), bed_resistance_(0), bed_slope_(0), bed_voltage_(0),
    idle_configured_(false), bed_slope_configured_(false) {}

bool PowerBudget::set_total_power_budget(const float watts) {
  return set_nonnegative(total_, watts);
}

bool PowerBudget::set_idle_power_consumption(const float watts) {
  idle_configured_ = set_nonnegative(idle_, watts);
  return idle_configured_;
}

bool PowerBudget::set_nominal_bed_resistance(const float ohms) {
  return set_positive(bed_resistance_, ohms);
}

bool PowerBudget::set_bed_resistance_per_degree(const float ohms_per_celsius) {
  bed_slope_configured_ = set_nonnegative(bed_slope_, ohms_per_celsius);
  return bed_slope_configured_;
}

bool PowerBudget::set_bed_voltage(const float volts) {
  return set_positive(bed_voltage_, volts);
}

bool PowerBudget::set_nominal_hotend_resistance(const uint8_t slot, const float ohms) {
  return slot < HOTENDS && set_nonnegative(slots_[slot].resistance, ohms);
}

bool PowerBudget::set_hotend_voltage(const uint8_t slot, const float volts) {
  return slot < HOTENDS && set_positive(slots_[slot].voltage, volts);
}

bool PowerBudget::set_max_hotend_power(const uint8_t slot, const float watts) {
  return slot < HOTENDS && set_nonnegative(slots_[slot].max_power, watts);
}

PowerBudget::Outputs PowerBudget::calculate(const uint8_t (&requested_hotends)[HOTENDS],
                                           const uint8_t requested_bed,
                                           const float bed_temperature) const {
  Outputs result = {};
  if (!idle_configured_ || total_ <= idle_) return result;
  float remaining = total_ - idle_;
  for (uint8_t slot = 0; slot < HOTENDS; ++slot) {
    const float full = full_power(slots_[slot].voltage, slots_[slot].resistance);
    const float available = slots_[slot].max_power < remaining ? slots_[slot].max_power : remaining;
    result.hotend[slot] = limit_output(requested_hotends[slot], full, available);
    remaining -= used_power(full, result.hotend[slot]);
  }
  if (bed_slope_configured_ && isfinite(bed_temperature)) {
    const float resistance = bed_resistance_ + bed_slope_ * (bed_temperature - 60.0f);
    // An invalid nominal resistance must not become usable through the slope.
    if (bed_resistance_ > 0)
      result.bed = limit_output(requested_bed, full_power(bed_voltage_, resistance), remaining);
  }
  return result;
}

} // namespace ultimaker3

#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST

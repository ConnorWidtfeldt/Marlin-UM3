/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2016-2021 Ultimaker B.V.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Derived from UltimakerMarlin 4545818112027387f09e87f930117ab8f3804327,
 * Marlin/PowerBudgetManagement*.{h,cpp}, corroborated by the shipped UM3
 * 5.3.0 ARM simulator. See docs/ultimaker3/recovery-power.md for hardening
 * differences. This calculates duty limits; it does not drive heaters.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

class PowerBudget {
public:
  static constexpr uint8_t HOTENDS = 2;
  struct Outputs { uint8_t hotend[HOTENDS], bed; };

  PowerBudget();

  // M290. Zero budget disables heaters; idle must be explicitly supplied.
  bool set_total_power_budget(float watts);
  bool set_idle_power_consumption(float watts);

  // M291. Bed resistance is measured at 60 C; slope is ohms per degree C.
  bool set_nominal_bed_resistance(float ohms);
  bool set_bed_resistance_per_degree(float ohms_per_celsius);
  bool set_bed_voltage(float volts);

  // M292. Zero hotend resistance is the vendor's disconnected-core sentinel.
  bool set_nominal_hotend_resistance(uint8_t slot, float ohms);
  bool set_hotend_voltage(uint8_t slot, float volts);
  bool set_max_hotend_power(uint8_t slot, float watts);

  // One snapshot avoids the vendor getters' dependency on call order.
  Outputs calculate(const uint8_t (&requested_hotends)[HOTENDS], uint8_t requested_bed,
                    float bed_temperature) const;

private:
  struct Slot { float resistance, voltage, max_power; } slots_[HOTENDS];
  float total_, idle_, bed_resistance_, bed_slope_, bed_voltage_;
  bool idle_configured_, bed_slope_configured_;
};

} // namespace ultimaker3

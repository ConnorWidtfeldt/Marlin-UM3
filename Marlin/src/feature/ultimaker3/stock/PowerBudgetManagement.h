/*
    Copyright (c) 2016-2021 Ultimaker B.V. All rights reserved.

    Marlin is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Marlin is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Marlin.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include "../power_budget.h"
namespace um3_stock {
class PowerBudgetManagement {
  ultimaker3::PowerBudget budget;
  uint8_t requested[2]={0,0}, bed_output=0;
  float bed_temperature=0;
  ultimaker3::PowerBudget::Outputs outputs() const { return budget.calculate(requested,bed_output,bed_temperature); }
public:
  explicit PowerBudgetManagement(uint8_t) {}
  bool setTotalPowerBudget(float v) { return budget.set_total_power_budget(v); }
  bool setIdlePowerConsumption(float v) { return budget.set_idle_power_consumption(v); }
  bool setNominalHotendResistance(uint8_t e,float v) { return budget.set_nominal_hotend_resistance(e,v); }
  bool setHotendVoltage(uint8_t e,float v) { return budget.set_hotend_voltage(e,v); }
  bool setMaxPowerUsageForHeater(uint8_t e,float v) { return budget.set_max_hotend_power(e,v); }
  bool setBedVoltage(float v) { return budget.set_bed_voltage(v); }
  bool setNominalBedResistance(float v) { return budget.set_nominal_bed_resistance(v); }
  bool setBedResistancePerDegree(float v) { return budget.set_bed_resistance_per_degree(v); }
  void setRequestedHeaterOutput(uint8_t e,uint8_t v) { if(e<2) requested[e]=v; }
  uint8_t getRequestedHeaterOutput(uint8_t e) { return e<2 ? requested[e] : 0; }
  void setRequestedBedOutput(uint8_t v) { bed_output=v; }
  void setCurrentBedTemperature(float v) { bed_temperature=v; }
  uint8_t getActualHeaterOutput(uint8_t e) const { return e<2 ? outputs().hotend[e] : 0; }
  uint8_t getActualBedOutput() const { return outputs().bed; }
};
}

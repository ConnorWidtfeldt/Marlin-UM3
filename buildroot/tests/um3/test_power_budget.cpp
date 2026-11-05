/** SPDX-License-Identifier: GPL-3.0-or-later */
#include "power_budget.h"
#include "power_fixtures.h"

#include <assert.h>
#include <float.h>
#include <initializer_list>
#include <limits>
#include <stdio.h>

using ultimaker3::PowerBudget;

static PowerBudget configured(const float total = 221, const float idle = 46,
                              const float slot_resistance = 23) {
  PowerBudget budget;
  assert(budget.set_total_power_budget(total));
  assert(budget.set_idle_power_consumption(idle));
  assert(budget.set_nominal_bed_resistance(3.678255f));
  assert(budget.set_bed_resistance_per_degree(0.012285f));
  assert(budget.set_bed_voltage(24));
  for (uint8_t slot = 0; slot < PowerBudget::HOTENDS; ++slot) {
    assert(budget.set_nominal_hotend_resistance(slot, slot_resistance));
    assert(budget.set_hotend_voltage(slot, 24));
    assert(budget.set_max_hotend_power(slot, 75));
  }
  return budget;
}

static void assert_off(const PowerBudget::Outputs &output) {
  assert(output.hotend[0] == 0 && output.hotend[1] == 0 && output.bed == 0);
}

static void startup() {
  const uint8_t requested[] = {255, 255};
  PowerBudget budget;
  assert_off(budget.calculate(requested, 255, 20));
  assert(budget.set_total_power_budget(221));
  assert(budget.set_hotend_voltage(0, 24));
  assert(budget.set_max_hotend_power(0, 75));
  assert_off(budget.calculate(requested, 255, 20));
  assert(budget.set_nominal_hotend_resistance(0, 23));
  assert_off(budget.calculate(requested, 255, 20)); // No host idle-power value yet.
  assert(budget.set_idle_power_consumption(46));
  PowerBudget::Outputs result = budget.calculate(requested, 255, 20);
  assert(result.hotend[0] == 255 && result.hotend[1] == 0 && result.bed == 0);
  assert(budget.set_nominal_bed_resistance(3.678255f));
  assert(budget.set_bed_voltage(24));
  assert(budget.calculate(requested, 255, 20).bed == 0); // No slope yet.
  assert(budget.set_bed_resistance_per_degree(0));
  assert(budget.calculate(requested, 255, 20).bed > 0);
  assert(budget.set_total_power_budget(0));
  assert_off(budget.calculate(requested, 255, 20));
}

static void stock_fixtures() {
  const uint8_t off[] = {0, 0};
  for (const StockBedFixture &fixture : STOCK_BED_FIXTURES) {
    const PowerBudget budget = configured(fixture.remaining, 0);
    assert(budget.calculate(off, 255, fixture.temperature).bed == fixture.expected);
    assert(budget.calculate(off, 17, fixture.temperature).bed == 17);
  }

  const uint8_t full[] = {255, 255};
  PowerBudget budget = configured();
  PowerBudget::Outputs result = budget.calculate(full, 255, 60);
  assert(result.hotend[0] == 255 && result.hotend[1] == 255 && result.bed == 203);
  budget = configured(221, 46, 6);
  result = budget.calculate(full, 255, 60);
  assert(result.hotend[0] == 199 && result.hotend[1] == 199 && result.bed == 40);
  assert(budget.set_nominal_hotend_resistance(0, 0));
  result = budget.calculate(full, 255, 60);
  assert(result.hotend[0] == 0 && result.hotend[1] == 199 && result.bed == 162);
  assert(budget.set_max_hotend_power(1, 0));
  result = budget.calculate(full, 255, 60);
  assert(result.hotend[1] == 0 && result.bed == 255);
}

static void invalid_inputs() {
  const uint8_t requested[] = {255, 255};
  const float bad[] = {-1, std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity()};
  for (const float value : bad) {
    PowerBudget budget = configured();
    assert(!budget.set_total_power_budget(value));
    assert_off(budget.calculate(requested, 255, 20));
    budget = configured();
    assert(!budget.set_idle_power_consumption(value));
    assert_off(budget.calculate(requested, 255, 20));
    budget = configured();
    assert(!budget.set_nominal_hotend_resistance(0, value));
    assert(budget.calculate(requested, 255, 20).hotend[0] == 0);
    budget = configured();
    assert(!budget.set_hotend_voltage(1, value));
    assert(budget.calculate(requested, 255, 20).hotend[1] == 0);
    budget = configured();
    assert(!budget.set_max_hotend_power(1, value));
    assert(budget.calculate(requested, 255, 20).hotend[1] == 0);
    budget = configured();
    assert(!budget.set_nominal_bed_resistance(value));
    assert(budget.calculate(requested, 255, 115).bed == 0);
    budget = configured();
    assert(!budget.set_bed_resistance_per_degree(value));
    assert(budget.calculate(requested, 255, 20).bed == 0);
    budget = configured();
    assert(!budget.set_bed_voltage(value));
    assert(budget.calculate(requested, 255, 20).bed == 0);
  }
  PowerBudget budget = configured();
  const PowerBudget::Outputs before = budget.calculate(requested, 255, 20);
  for (const uint8_t slot : {uint8_t(2), uint8_t(255)}) {
    assert(!budget.set_hotend_voltage(slot, 24));
    assert(!budget.set_nominal_hotend_resistance(slot, 23));
    assert(!budget.set_max_hotend_power(slot, 75));
  }
  const PowerBudget::Outputs after = budget.calculate(requested, 255, 20);
  assert(after.hotend[0] == before.hotend[0] && after.hotend[1] == before.hotend[1]
         && after.bed == before.bed);
  assert(!budget.set_nominal_bed_resistance(0));
  assert(!budget.set_bed_voltage(0));
  assert(!budget.set_hotend_voltage(0, 0));
  assert(budget.calculate(requested, 255, 20).hotend[0] == 0);

  budget = configured();
  assert(budget.calculate(requested, 255, std::numeric_limits<float>::quiet_NaN()).bed == 0);
  assert(budget.calculate(requested, 255, std::numeric_limits<float>::infinity()).bed == 0);
  assert(budget.calculate(requested, 255, -1000).bed == 0); // Nonpositive effective R.
  assert(budget.set_bed_voltage(FLT_MAX));
  assert(budget.calculate(requested, 255, 20).bed == 0); // Overflowed power model.
  assert(budget.set_hotend_voltage(0, FLT_MAX));
  assert(budget.calculate(requested, 255, 20).hotend[0] == 0);
  budget = configured(40, 46);
  assert_off(budget.calculate(requested, 255, 20));
}

static void global_budget() {
  // This deliberately undersized budget exercises the documented safety hardening.
  const uint8_t full[] = {255, 255};
  PowerBudget budget = configured(100, 46, 6);
  PowerBudget::Outputs result = budget.calculate(full, 255, 20);
  assert(result.hotend[0] == 143 && result.hotend[1] == 0 && result.bed == 0);

  unsigned cases = 0;
  for (const float total : {46.0f, 47.0f, 100.0f, 175.0f, 221.0f})
    for (const float resistance : {6.0f, 8.0f, 23.0f})
      for (int temperature = 0; temperature <= 120; temperature += 20)
        for (int a = 0; a <= 255; a += 17)
          for (int b = 0; b <= 255; b += 17) {
            budget = configured(total, 46, resistance);
            const uint8_t requested[] = {uint8_t(a), uint8_t(b)};
            result = budget.calculate(requested, 255, float(temperature));
            assert(result.hotend[0] <= a && result.hotend[1] <= b);
            const double full_hotend = 576.0 / resistance;
            const double power0 = full_hotend * result.hotend[0] / 255;
            const double power1 = full_hotend * result.hotend[1] / 255;
            const double bed_power = 576.0 / (3.678255 + 0.012285 * (temperature - 60))
                                     * result.bed / 255;
            assert(power0 <= 75.0001 && power1 <= 75.0001);
            assert(power0 + power1 + bed_power <= total - 46 + 0.0001);
            ++cases;
          }
  printf("UM3 power budget: stock fixtures, fail-closed settings, %u allocation cases passed\n", cases);
}

int main() {
  startup();
  stock_fixtures();
  invalid_inputs();
  global_budget();
}

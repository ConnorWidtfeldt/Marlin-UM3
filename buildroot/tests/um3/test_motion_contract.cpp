// SPDX-License-Identifier: GPL-3.0-or-later
#include "motion_contract.h"

#include <assert.h>
#include <initializer_list>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using namespace ultimaker3;

int main() {
  for (uint8_t tail = 0; tail < 16; ++tail)
    for (uint8_t used = 0; used < 16; ++used)
      assert(planner_free_slots((tail + used) % 16, tail) == 15 - used);
  assert(planner_free_slots(0, 255) == 14);

  int32_t offset = 123;
  assert(tool_offset_steps(18.0f, 80.0f, offset) && offset == -1440);
  assert(tool_offset_steps(0.5f, 1.0f, offset) && offset == -1);
  assert(tool_offset_steps(-0.5f, 1.0f, offset) && offset == 1);
  assert(tool_offset_steps(0.0f, 400.0f, offset) && offset == 0);
  assert(tool_offset_steps(1.5f, 1.0f, offset) && offset == -2);
  assert(tool_offset_steps(-1.5f, 1.0f, offset) && offset == 2);
  assert(tool_offset_steps(2147483520.0f, 1.0f, offset) && offset == -2147483520);
  assert(tool_offset_steps(-2147483520.0f, 1.0f, offset) && offset == 2147483520);
  offset = 123;
  for (const float invalid : {NAN, INFINITY, -INFINITY, 2147483648.0f, -2147483648.0f}) {
    assert(!tool_offset_steps(invalid, 1.0f, offset));
    assert(offset == 123);
  }
  for (const float invalid : {NAN, INFINITY, 0.0f, -1.0f}) {
    assert(!tool_offset_steps(1.0f, invalid, offset));
    assert(offset == 123);
  }

  const int32_t executed[4] = {9440, 8000, 4000, 3690};
  const int32_t tool1[3] = {-1440, 0, -600};
  const float calibrated[4] = {80, 80, 400, 369};
  float logical[4] = {};
  assert(logical_position_from_steps(executed, tool1, calibrated, logical));
  assert(logical[0] == 100 && logical[1] == 100 && logical[2] == 8.5f && logical[3] == 10);
  const int32_t tool0[3] = {};
  assert(logical_position_from_steps(executed, tool0, calibrated, logical));
  assert(logical[0] == 118 && logical[2] == 10 && logical[3] == 10);

  const float sentinel[4] = {1, 2, 3, 4};
  memcpy(logical, sentinel, sizeof(logical));
  const float invalid_steps[4] = {80, 80, 0, 369};
  assert(!logical_position_from_steps(executed, tool1, invalid_steps, logical));
  assert(!memcmp(logical, sentinel, sizeof(logical)));
  const int32_t overflow[4] = {INT32_MIN, 0, 0, 0};
  assert(!logical_position_from_steps(overflow, tool1, calibrated, logical));
  assert(!memcmp(logical, sentinel, sizeof(logical)));

  assert(STOCK_STEPS_PER_MM[2] == 200 && calibrated[2] == 400);
  assert(STOCK_MAX_POS_MM[2] == 240); // The host later supplies Extended Z=310.
  static_assert(ProbeSettings::SAMPLE_COUNT_1 + ProbeSettings::SAMPLE_COUNT_2 == 100, "sample ring");
  static_assert(ProbeSettings::RESIDUAL_BUFFER_SIZE == 25, "residual ring");
  static_assert(ProbeSettings::PEAK_THRESHOLD == 150000, "scaled peak threshold");
  assert(ProbeSettings::ALPHA == 0.85f && ProbeSettings::BETA == 0.04f);
  puts("UM3 recovered planner credits, tool offsets and stopped positions passed");
}

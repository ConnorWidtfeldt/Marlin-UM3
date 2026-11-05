/**
 * Marlin 3D Printer Firmware
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Recovered UM3 contracts, corroborated by the stock 2019.01.27 simulator and
 * jedi.hex. See docs/ultimaker3/recovery-motion.md for addresses and limitations.
 * These helpers do not operate motors or implement the capacitive probe.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

extern const float STOCK_STEPS_PER_MM[4];
extern const float STOCK_HOMING_FEEDRATE_MM_MIN[4];
extern const float STOCK_MAX_FEEDRATE_MM_S[4];
extern const float STOCK_MAX_POS_MM[3];

struct ProbeSettings {
  static constexpr uint8_t SAMPLE_COUNT_1 = 50;
  static constexpr uint8_t SAMPLE_COUNT_2 = 50;
  static constexpr uint8_t RESIDUAL_BUFFER_SIZE = 25;
  static constexpr uint16_t SAMPLE_FREQUENCY_HZ = 100;
  static constexpr uint16_t RESIDUAL_MULTIPLIER = 1000;
  static constexpr float ALPHA = 0.85f;
  static constexpr float BETA = 0.04f;
  static constexpr int32_t PEAK_THRESHOLD = 150000;
  static constexpr uint8_t PEAK_DROP_NUMERATOR = 3;
  static constexpr uint8_t PEAK_DROP_DENOMINATOR = 4;
  static constexpr uint8_t MAX_NOISE_STD = 20;
  static constexpr float DEFAULT_MOVE_MM = 10.0f;
};

// The 16-entry ring reserves one slot so an empty queue differs from a full one.
uint8_t planner_free_slots(uint8_t head, uint8_t tail);

// M218 stores negative, half-away-from-zero rounded steps, not millimeters.
// Reject invalid calibration or overflow without modifying the output.
bool tool_offset_steps(float millimeters, float steps_per_mm, int32_t &offset);

// Recover logical XYZE after stopping at executed physical steps. The selected
// tool contributes XYZ offsets only. Host calibration supplies steps_per_mm.
// Reject invalid input without partially updating logical_position.
bool logical_position_from_steps(const int32_t physical_steps[4], const int32_t tool_offset[3],
                                 const float steps_per_mm[4], float logical_position[4]);

} // namespace ultimaker3

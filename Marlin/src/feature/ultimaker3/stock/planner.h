/** GPL-3.0-or-later. UM3 API adapted to the modern Marlin planner. */
#pragma once
#include "Marlin.h"
namespace um3_stock {
void plan_init();

// Add a new linear movement to the buffer. x, y and z is the signed, absolute target position in
// millimeters. Feed rate specifies the speed of the motion.
void plan_buffer_line(const float &x, const float &y, const float &z, const float &e, float feed_rate, const uint8_t &extruder);

// Set position. Used for G92 instructions.
void plan_set_position(const float &x, const float &y, const float &z, const float &e);
void plan_set_e_position(const float &e);



void check_axes_activity();
uint8_t plan_buf_free_positions();  // return the number of free positions in the planner buffer.

extern uint32_t minsegmenttime;
extern float max_feedrate[NUM_AXIS]; // set the max speeds
extern float axis_steps_per_unit[NUM_AXIS];
extern uint32_t max_acceleration_units_per_sq_second[4]; // Use M201 to override by software
extern float minimumfeedrate;
extern float acceleration;         // Normal acceleration mm/s^2  THIS IS THE DEFAULT ACCELERATION for all moves. M204 SXXXX
extern float retract_acceleration; //  mm/s^2   filament pull-pack and push-forward  while standing still in the other axis M204 TXXXX
extern float max_xy_jerk;          //speed that can be stopped at once, if I understand correctly.
extern float max_z_jerk;
extern float max_e_jerk;
extern float mintravelfeedrate;
extern uint32_t axis_steps_per_sqr_second[NUM_AXIS];



bool blocks_queued();
void set_extrude_min_temp(float temp);
void reset_acceleration_rates();
}

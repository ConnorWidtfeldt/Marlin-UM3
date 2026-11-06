/** GPL-3.0-or-later. Boundary between the verified UM3 frontend and modern Marlin. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int um3_native_serial_read();
int um3_native_serial_peek();
int um3_native_serial_available();
void um3_native_serial_write(uint8_t);
void um3_native_serial_begin(uint32_t);
void um3_native_serial_end();
void um3_native_serial_clear_rx();
void um3_native_stepper_init();
void um3_native_synchronize();
void um3_native_set_steps(int32_t x,int32_t y,int32_t z,int32_t e);
int32_t um3_native_get_steps(uint8_t axis);
void um3_native_set_e_steps(int32_t e);
void um3_native_wake();
void um3_native_endstops(bool enable);
void um3_native_clear_endstop_hits();
bool um3_native_endstop_hit();
uint8_t um3_native_endstop_hits();
int32_t um3_native_triggered_steps(uint8_t axis);
void um3_native_disable_steppers();
void um3_native_enable_axis(uint8_t,bool);
void um3_native_quickstop();
void um3_native_abort_motion();
uint8_t um3_native_planner_free();
bool um3_native_blocks_queued();
void um3_native_activity();
uint8_t um3_stock_target_fan_speed();
void um3_stock_apply_cooling_fan(uint8_t);
void um3_native_buffer(float x,float y,float z,float e,float speed,uint8_t tool);
void um3_native_position(float x,float y,float z,float e);
void um3_native_e_position(float e);
bool um3_native_calibrate(const float *steps,const float *feeds,const uint32_t *accels,
                         float accel,float retract,float xyjerk,float zjerk,float ejerk,
                         float minfeed,float mintravel,uint32_t minsegment,int flow,const bool *directions);
void um3_native_select_tool(uint8_t);
bool um3_stock_is_stopped();
void um3_native_tool_offsets(const int32_t *offsets);
bool um3_native_target_steps(const float *logical,uint8_t tool,int32_t *steps);
void um3_native_min_extrude(float temp);
void um3_native_arc(const float *start,const float *end,const float *offset,float speed,bool clockwise,uint8_t tool);
void um3_native_watchdog_init();
void um3_native_watchdog_reset();
void um3_native_temperature_tick();
void um3_native_thermal_sample(const float *hotends,const float *targets,const bool *allow_removal,float bed,float bed_target);
void um3_stock_thermal_task();
void um3_stock_disable_heaters();
void um3_stock_stop(uint8_t reason);
void um3_stock_read_commands();
#ifdef __cplusplus
}
namespace um3_stock { void setup(); void loop(); void manage_inactivity(); }
#endif

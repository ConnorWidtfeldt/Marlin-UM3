/** GPL-3.0-or-later. Verified UM3 interfaces backed by Marlin 2.1.2.9 motion. */
#include "Marlin.h"
#include "planner.h"
#include "stepper.h"
#include "motion_control.h"
#include "watchdog.h"
#include "temperature.h"
#include "SerialProtocol.h"
#include "fan_driver.h"
#include "../motion_contract.h"
namespace um3_stock {
uint32_t minsegmenttime=DEFAULT_MINSEGMENTTIME;
float max_feedrate[NUM_AXIS]=DEFAULT_MAX_FEEDRATE;
float axis_steps_per_unit[NUM_AXIS]=DEFAULT_AXIS_STEPS_PER_UNIT;
uint32_t max_acceleration_units_per_sq_second[4]=DEFAULT_MAX_ACCELERATION;
uint32_t axis_steps_per_sqr_second[4];
float minimumfeedrate=0, mintravelfeedrate=0;
float acceleration=DEFAULT_ACCELERATION, retract_acceleration=DEFAULT_RETRACT_ACCELERATION;
float max_xy_jerk=DEFAULT_XYJERK, max_z_jerk=DEFAULT_ZJERK, max_e_jerk=DEFAULT_EJERK;
bool invert_direction[5]={DEFAULT_INVERT_X_DIR,DEFAULT_INVERT_Y_DIR,DEFAULT_INVERT_Z_DIR,DEFAULT_INVERT_E0_DIR,DEFAULT_INVERT_E1_DIR};
void sync_motion_settings() {
  if(!um3_native_calibrate(axis_steps_per_unit,max_feedrate,max_acceleration_units_per_sq_second,
    acceleration,retract_acceleration,max_xy_jerk,max_z_jerk,max_e_jerk,
    minimumfeedrate,mintravelfeedrate,minsegmenttime,extrudemultiply,invert_direction)) stop(STOP_REASON_POSITION_ERROR);
  um3_native_tool_offsets(&extruder_offset[0][0]);
  um3_native_select_tool(active_extruder);
}
void plan_init() { sync_motion_settings(); plan_set_position(current_position[0],current_position[1],current_position[2],current_position[3]); }
void plan_buffer_line(const float &x,const float &y,const float &z,const float &e,float speed,const uint8_t &tool) {
  sync_motion_settings();
  if (!isStopped()) um3_native_buffer(x,y,z,e,speed,tool);
}
void plan_set_position(const float &x,const float &y,const float &z,const float &e) {
  sync_motion_settings();
  um3_native_position(x,y,z,e);
}
void plan_set_e_position(const float &e) { sync_motion_settings(); um3_native_e_position(e); }
void reset_acceleration_rates() {
  for(uint8_t a=0;a<4;a++) axis_steps_per_sqr_second[a]=max_acceleration_units_per_sq_second[a]*axis_steps_per_unit[a];
  sync_motion_settings();
}
void set_extrude_min_temp(float temp) { um3_native_min_extrude(temp); }
void check_axes_activity() { um3_native_activity(); }
uint8_t plan_buf_free_positions() { return um3_native_planner_free(); }
bool blocks_queued() { return um3_native_blocks_queued(); }
void st_init() { um3_native_stepper_init(); }
void st_enable_interrupt() { um3_native_wake(); }
void st_synchronize() { um3_native_synchronize(); }
void st_set_position(const long &x,const long &y,const long &z,const long &e) { um3_native_set_steps(x,y,z,e); }
void st_set_e_position(const long &e) { um3_native_set_e_steps(e); }
long st_get_position(uint8_t axis) { return um3_native_get_steps(axis); }
void st_wake_up() { um3_native_wake(); }
void enable_endstops(bool check) { um3_native_endstops(check); }
void endstops_hit_on_purpose() { um3_native_clear_endstop_hits(); }
bool isEndstopHit() { return um3_native_endstop_hit(); }
void checkHitEndstops() { um3_native_clear_endstop_hits(); }
void finishAndDisableSteppers() { st_synchronize(); um3_native_disable_steppers(); }
void quickStop() {
  um3_native_quickstop();
  int32_t physical[4],offset[3];
  for(uint8_t a=0;a<4;a++) physical[a]=st_get_position(a);
  for(uint8_t a=0;a<3;a++) offset[a]=extruder_offset[a][active_extruder];
  if(!ultimaker3::logical_position_from_steps(physical,offset,axis_steps_per_unit,current_position)) {
    stop(STOP_REASON_POSITION_ERROR); return;
  }
  plan_set_position(current_position[0],current_position[1],current_position[2],current_position[3]);
}
void mc_arc(float *start,float *end,float *offset,unsigned char,unsigned char,unsigned char,float speed,float,unsigned char clockwise,uint8_t tool) {
  sync_motion_settings();
  um3_native_arc(start,end,offset,speed,clockwise,tool);
  for(uint8_t a=0;a<4;a++) start[a]=end[a];
}
void watchdog_init() { um3_native_watchdog_init(); }
void watchdog_reset() { um3_native_watchdog_reset(); }
}
extern "C" void um3_stock_thermal_task() { um3_stock::manage_heater(); }
extern "C" void um3_stock_disable_heaters() { um3_stock::disable_all_heaters(); }
extern "C" void um3_stock_stop(uint8_t reason) { um3_stock::stop(reason); }
extern "C" void um3_stock_read_commands();

extern "C" bool um3_stock_is_stopped() { return um3_stock::isStopped(); }

extern "C" uint8_t um3_stock_target_fan_speed() { return um3_stock::target_fan_speed; }
extern "C" void um3_stock_apply_cooling_fan(uint8_t speed) {
  static uint32_t kick_end=0;
  static bool running=false;
  const uint32_t now=millis();
  if(speed) {
    if(!running) { kick_end=now+FAN_KICKSTART_TIME; running=true; }
    if(int32_t(kick_end-now)>0) speed=255;
  }
  else running=false;
  um3_stock::setCoolingFanSpeed(speed);
}

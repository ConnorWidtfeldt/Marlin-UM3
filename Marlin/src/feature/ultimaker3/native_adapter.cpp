/** GPL-3.0-or-later. UM3 compatibility calls into current Marlin facilities. */
#include "../../inc/MarlinConfig.h"
#if ENABLED(UM3_COMPATIBILITY)
#include "native_api.h"
#include "motion_contract.h"
#include "../../MarlinCore.h"
#include "../../module/planner.h"
#include "../../module/stepper.h"
#include "../../module/endstops.h"
#include "../../module/motion.h"
#include "../../module/temperature.h"
#include "../../gcode/gcode.h"
static int32_t um3_step_offsets[3][2];
static_assert(sizeof(xyze_pos_t)==4*sizeof(float),"UM3 requires Cartesian XYZE storage");
bool um3_runtime_direction[5]={true,false,true,true,false};
void plan_arc(const xyze_pos_t &,const ab_float_t &,bool,uint8_t);
extern "C" {
int um3_native_serial_read() { return MYSERIAL1.read(); }
int um3_native_serial_peek() { return MYSERIAL1.peek(); }
int um3_native_serial_available() { return MYSERIAL1.available(); }
void um3_native_serial_write(uint8_t b) { MYSERIAL1.write(b); }
void um3_native_serial_begin(uint32_t baud) { MYSERIAL1.begin(baud); }
void um3_native_serial_end() { MYSERIAL1.end(); }
void um3_native_serial_clear_rx() { while(MYSERIAL1.available()) MYSERIAL1.read(); }
void um3_native_stepper_init() { endstops.init(); stepper.init(); stepper.suspend(); }
void um3_native_synchronize() { planner.synchronize(); }
void um3_native_set_steps(int32_t x,int32_t y,int32_t z,int32_t e) { stepper.set_position(xyze_long_t{x,y,z,e}); }
int32_t um3_native_get_steps(uint8_t a) { return stepper.position(AxisEnum(a)); }
void um3_native_set_e_steps(int32_t e) { stepper.set_axis_position(E_AXIS,e); }
void um3_native_wake() { stepper.wake_up(); }
void um3_native_endstops(bool on) { endstops.enable(on); }
void um3_native_clear_endstop_hits() { endstops.hit_on_purpose(); }
bool um3_native_endstop_hit() { return endstops.trigger_state()!=0; }
void um3_native_enable_axis(uint8_t axis,bool enable) {
  if(axis<3) { if(enable) stepper.enable_axis(AxisEnum(axis)); else stepper.disable_axis(AxisEnum(axis)); }
  else { if(enable) stepper.enable_extruder(axis-3); else stepper.disable_extruder(axis-3); }
}
void um3_native_disable_steppers() { stepper.disable_all_steppers(); }
void um3_native_quickstop() { planner.quick_stop(); planner.synchronize(); }
void um3_native_abort_motion() { planner.quick_stop(); thermalManager.um3_reset(); }
uint8_t um3_native_planner_free() { return planner.moves_free(); }
bool um3_native_blocks_queued() { return planner.has_blocks_queued(); }
void um3_native_activity() { planner.check_axes_activity(); }
void um3_native_buffer(float x,float y,float z,float e,float speed,uint8_t tool) {
  if(planner.cleaning_buffer_counter) planner.synchronize();
  if(um3_stock_is_stopped()) return;
  active_extruder=tool;
  current_position.set(x,y,z,e);
  planner.buffer_line(current_position,speed,tool);
}
void um3_native_position(float x,float y,float z,float e) {
  current_position.set(x,y,z,e); planner.set_position_mm(current_position);
}
void um3_native_e_position(float e) {
  const float scaled=e*planner.settings.axis_steps_per_mm[E_AXIS];
  if(!isfinite(scaled) || scaled < -2147483648.0f || scaled >= 2147483648.0f) { um3_stock_stop(14); return; }
  current_position.e=e; planner.set_e_position_mm(e);
}
bool um3_native_calibrate(const float *steps,const float *feeds,const uint32_t *accels,
                         float accel,float retract,float xyjerk,float zjerk,float ejerk,
                         float minfeed,float mintravel,uint32_t minsegment,int flow,const bool *directions) {
  for(uint8_t a=0;a<4;a++) {
    if(!isfinite(steps[a]) || steps[a]<=0 || !isfinite(feeds[a]) || feeds[a]<=0
       || uint64_t(accels[a])*steps[a]>UINT32_MAX) return false;
  }
  const float positive[]={accel,retract,xyjerk,zjerk,ejerk,minfeed,mintravel};
  for(float value:positive) if(!isfinite(value) || value<0) return false;
  bool changed=false;
  for(uint8_t a=0;a<4;a++) {
    changed |= planner.settings.axis_steps_per_mm[a]!=steps[a] || planner.settings.max_acceleration_mm_per_s2[a]!=accels[a];
    planner.settings.axis_steps_per_mm[a]=steps[a];
    planner.settings.max_feedrate_mm_s[a]=feeds[a];
    planner.settings.max_acceleration_mm_per_s2[a]=accels[a];
  }
  planner.settings.acceleration=accel;
  planner.settings.travel_acceleration=accel;
  planner.settings.retract_acceleration=retract;
  planner.settings.min_feedrate_mm_s=minfeed;
  planner.settings.min_travel_feedrate_mm_s=mintravel;
  planner.settings.min_segment_time_us=minsegment;
  planner.max_jerk.set(xyjerk,xyjerk,zjerk,ejerk);
  for(uint8_t e=0;e<2;e++) planner.set_flow(e,flow);
  bool direction_changed=false;
  for(uint8_t a=0;a<5;a++) direction_changed |= um3_runtime_direction[a]!=directions[a];
  if(direction_changed) {
    CRITICAL_SECTION_START();
    for(uint8_t a=0;a<5;a++) um3_runtime_direction[a]=directions[a];
    stepper.apply_directions();
    CRITICAL_SECTION_END();
  }
  if(changed) {
    for(uint8_t a=0;a<4;a++) planner.mm_per_step[a]=1.0f/steps[a];
    planner.refresh_acceleration_rates();
  }
  return true;
}
void um3_native_select_tool(uint8_t tool) { if(tool<2) active_extruder=tool; }
void um3_native_tool_offsets(const int32_t *offsets) {
  for(uint8_t a=0;a<3;a++) for(uint8_t e=0;e<2;e++) um3_step_offsets[a][e]=offsets[a*2+e];
}
bool um3_native_target_steps(const float *logical,uint8_t tool,int32_t *steps) {
  if(tool>=2) return false;
  const int32_t offset[3]={um3_step_offsets[0][tool],um3_step_offsets[1][tool],um3_step_offsets[2][tool]};
  if(!ultimaker3::physical_target_steps(logical,offset,planner.settings.axis_steps_per_mm,steps)) {
    um3_stock_stop(14);
    return false;
  }
  return true;
}
void um3_native_min_extrude(float temp) { thermalManager.extrude_min_temp=temp; }
void um3_native_arc(const float *from,const float *to,const float *offset,float speed,bool cw,uint8_t tool) {
  active_extruder=tool;
  current_position.set(from[0],from[1],from[2],from[3]);
  feedrate_mm_s=speed;
  feedrate_percentage=100;
  plan_arc(xyze_pos_t{to[0],to[1],to[2],to[3]},ab_float_t{offset[0],offset[1]},cw,0);
}
void um3_native_watchdog_init() { hal.watchdog_init(); }
void um3_native_watchdog_reset() { hal.watchdog_refresh(); }
void um3_native_temperature_tick() { endstops.poll(); planner.isr(); }
}
#endif

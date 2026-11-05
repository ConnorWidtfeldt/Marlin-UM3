/** GPL-3.0-or-later. UM3 stock configuration, reconstruction 7a80a3bb6.
 * Compiled defaults are retained; Griffin supplies Extended geometry/calibration.
 */
#pragma once
#define CONFIGURATION_H_VERSION 02010206
#define STRING_CONFIG_H_AUTHOR "HomeFab UM3"
#define MOTHERBOARD BOARD_ULTIMAKER_3
#define CUSTOM_MACHINE_NAME "Ultimaker 3 Extended"
#define SERIAL_PORT 2
#define BAUDRATE 250000
#define EXTRUDERS 2
#define DEFAULT_NOMINAL_FILAMENT_DIA 2.85
#define X_DRIVER_TYPE A4988
#define Y_DRIVER_TYPE A4988
#define Z_DRIVER_TYPE A4988
#define E0_DRIVER_TYPE A4988
#define E1_DRIVER_TYPE A4988
// The UM3 acquisition engine owns the external ADS1015 and calibrated bed ADC.
#define TEMP_SENSOR_0 -8
#define TEMP_SENSOR_1 -8
#define TEMP_SENSOR_BED 20
#define TEMP_SENSOR_2 0
#define TEMP_SENSOR_3 0
#define TEMP_SENSOR_4 0
#define TEMP_SENSOR_5 0
#define TEMP_SENSOR_6 0
#define TEMP_SENSOR_7 0
#define TEMP_SENSOR_PROBE 0
#define TEMP_SENSOR_CHAMBER 0
#define TEMP_SENSOR_COOLER 0
#define TEMP_SENSOR_BOARD 0
#define TEMP_SENSOR_REDUNDANT 0
#define HEATER_0_MINTEMP 5
#define HEATER_1_MINTEMP 5
#define BED_MINTEMP 5
#define HEATER_0_MAXTEMP 365
#define HEATER_1_MAXTEMP 365
#define BED_MAXTEMP 200
#define HOTEND_OVERSHOOT 15
#define BED_OVERSHOOT 10
#define BANG_MAX 255
#define PID_MAX 255
#define MAX_BED_POWER 255
#define PIDTEMP
#define PIDTEMPBED
#define PID_K1 0.99
#define PID_FUNCTIONAL_RANGE 25
#define DEFAULT_KP 0
#define DEFAULT_KI 0
#define DEFAULT_KD 0
#define DEFAULT_BED_KP 0
#define DEFAULT_BED_KI 0
#define DEFAULT_BED_KD 0
#define PREVENT_COLD_EXTRUSION
#define EXTRUDE_MINTEMP 170
#define PREVENT_LENGTHY_EXTRUDE
#define EXTRUDE_MAXLENGTH 1000
#define THERMAL_PROTECTION_HOTENDS
#define THERMAL_PROTECTION_BED
#define USE_XMIN_PLUG
#define USE_YMAX_PLUG
#define USE_ZMAX_PLUG
#define ENDSTOPPULLUPS
#define X_MIN_ENDSTOP_INVERTING true
#define Y_MAX_ENDSTOP_INVERTING true
#define Z_MAX_ENDSTOP_INVERTING true
#define X_MAX_ENDSTOP_INVERTING true
#define Y_MIN_ENDSTOP_INVERTING true
#define Z_MIN_ENDSTOP_INVERTING true
#define Z_MIN_PROBE_ENDSTOP_INVERTING true
#define X_ENABLE_ON 0
#define Y_ENABLE_ON 0
#define Z_ENABLE_ON 0
#define E_ENABLE_ON 0
#define DISABLE_X false
#define DISABLE_Y false
#define DISABLE_Z false
#define DISABLE_E false
#define DISABLE_OTHER_EXTRUDERS false
#define INVERT_X_DIR true
#define INVERT_Y_DIR false
#define INVERT_Z_DIR true
#define INVERT_E0_DIR true
#define INVERT_E1_DIR false
#define X_HOME_DIR -1
#define Y_HOME_DIR 1
#define Z_HOME_DIR 1
#define X_BED_SIZE 230
#define Y_BED_SIZE 230
#define X_MIN_POS 0
#define Y_MIN_POS 0
#define Z_MIN_POS 0
#define X_MAX_POS 230
#define Y_MAX_POS 230
#define Z_MAX_POS 240
// Bounds/homing are owned by the stock frontend's runtime geometry.
#define DEFAULT_AXIS_STEPS_PER_UNIT {80,80,200,369}
#define DEFAULT_MAX_FEEDRATE {300,300,40,45}
#define DEFAULT_MAX_ACCELERATION {9000,9000,100,10000}
#define DEFAULT_ACCELERATION 3000
#define DEFAULT_RETRACT_ACCELERATION 3000
#define DEFAULT_TRAVEL_ACCELERATION 3000
#define CLASSIC_JERK
#define DEFAULT_XJERK 20
#define DEFAULT_YJERK 20
#define DEFAULT_ZJERK 0.4
#define DEFAULT_EJERK 5
#define HOMING_FEEDRATE_MM_M {6000,6000,2400}
#define HOMING_BUMP_MM {7,7,7}
#define HOMING_BUMP_DIVISOR {3,3,3}
#define DEFAULT_MINIMUMFEEDRATE 0
#define DEFAULT_MINTRAVELFEEDRATE 0
#define PRINTCOUNTER_SAVE_INTERVAL 0
#define NO_VOLUMETRICS
#define NUM_M106_FANS 0
#define LCD_LANGUAGE en
#define DISPLAY_CHARSET_HD44780 JAPANESE

#define AXIS_RELATIVE_MODES {false,false,false,false}
#define TEMP_HYSTERESIS 3
#define TEMP_RESIDENCY_TIME 0
#define TEMP_WINDOW 1
#define TEMP_BED_HYSTERESIS 3
#define TEMP_BED_RESIDENCY_TIME 0
#define TEMP_BED_WINDOW 1

#define PROPORTIONAL_FONT_RATIO 1.0

// Native autotune is not exposed by the stock command frontend.
#define PREHEAT_1_TEMP_HOTEND 0
#define PREHEAT_1_TEMP_BED 0

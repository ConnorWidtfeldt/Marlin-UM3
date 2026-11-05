/** GPL-3.0-or-later. UM3 1548 board; verified stock reconstruction 7a80a3bb6.
 * Timer0: clock/thermal sampling; Timer1: Marlin stepper; Timer3: hotend PWM;
 * Timer5: A4988 reference PWM; TWI: stock priority driver; USART2: host.
 */
#pragma once
#define REQUIRE_MEGA2560
#include "env_validate.h"
#define BOARD_INFO_NAME "Ultimaker 3"
#define X_MIN_PIN 22
#define Y_MAX_PIN 26
#define Z_MAX_PIN 29
#define X_STEP_PIN 25
#define X_DIR_PIN 23
#define X_ENABLE_PIN 27
#define Y_STEP_PIN 32
#define Y_DIR_PIN 33
#define Y_ENABLE_PIN 31
#define Z_STEP_PIN 35
#define Z_DIR_PIN 36
#define Z_ENABLE_PIN 34
#define E0_STEP_PIN 42
#define E0_DIR_PIN 43
#define E0_ENABLE_PIN 37
#define E1_STEP_PIN 49
#define E1_DIR_PIN 47
#define E1_ENABLE_PIN 48
#define HEATER_0_PIN 2
#define HEATER_1_PIN 3
#define HEATER_BED_PIN 4
#define TEMP_0_PIN -1
#define TEMP_1_PIN -1
#define TEMP_BED_PIN 10
#define PS_ON_PIN 24
#define UM3_SAFETY_PIN 28
#define UM3_BOARD_ID_PIN 14
#define UM3_MOTOR_CURRENT_PWM_XY_PIN 44
#define UM3_MOTOR_CURRENT_PWM_Z_PIN 45
#define UM3_MOTOR_CURRENT_PWM_E_PIN 46

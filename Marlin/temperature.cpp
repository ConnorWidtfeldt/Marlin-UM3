// Stock ARM thermal reconstruction; see reconstruction/thermal.md for provenance and gaps.
/*
  temperature.cpp - temperature control
  Part of Marlin

 Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 This firmware is a mashup between Sprinter and grbl.
  (https://github.com/kliment/Sprinter)
  (https://github.com/simen/grbl/tree)
 */


#include "Marlin.h"
#include <util/atomic.h>
#include "temperature.h"
#include "watchdog.h"
#include "temperature_ADS101X.h"
#include "fan_driver.h"
#include "i2c_onewire_ds2482.h"
#include "Board.h"
#include "PowerBudgetManagement.h"
#include "GainCompensatorFactory.h"


// This magic value is the raw temperature value when no PT100 element is connected!
// The maximum ADC value is 0x7FF, however we need to take into account some noise.
#define MAGIC_NO_PT100_CONNECTED_RAW_VALUE (0x7F0 * OVERSAMPLENR)

//===========================================================================
//============================= public variables ============================
//===========================================================================
TemperaturePID hotend_pid[EXTRUDERS];
TemperaturePID heated_bed_pid;
#if defined(HEATER_0_USES_ADS101X) || defined(HEATER_1_USES_ADS101X) || defined(HEATER_2_USES_ADS101X)
TemperatureADS101X hotend_temperature_adc(ADS101X_ADC_ADDRESS_HOTEND);
#endif

/** For the XL the bed is measured using the same ADC as the hotends using I2C */
TemperatureADS101X bed_temperature_adc(ADS101X_ADC_ADDRESS_BED);


PowerBudgetManagement pwr(EXTRUDERS);

int16_t current_temperature_raw[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);
float current_temperature[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0.0);
int16_t current_temperature_bed_raw = 0;
float current_temperature_bed = 0.0;
bool allow_temperature_sensor_errors[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(true);
bool hotend_present[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(false);
//Heater output accumulators, this is used to have M105 report the average heating output since last report instead of just the "current" output which fluctuates too fast.
uint16_t heater_output_accumulator[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);
uint16_t heater_output_accumulator_counter[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);
uint16_t bed_heater_output_accumulator = 0;
uint16_t bed_heater_output_accumulator_counter = 0;

bool stop_heaters_pwm = false;

//===========================================================================
//============================ private variables ============================
//===========================================================================
static volatile bool temp_meas_ready = false;

// 60 degC is hot but usually not too hot to handle and it is also not an expected ambient temperature.
#define HOTEND_HUMAN_TOUCHABLE_TEMPERATURE 60

// The minimum temperature defines the temperature below which the heater will not be enabled It is used
// to check that the wiring to the thermistor is not broken.
// Otherwise this would lead to the heater being powered on all the time.
static const float HEATER_MINTEMP[] = {5.0, 5.0, 5.0};
#define BED_MINTEMP 5

// When temperature exceeds max temp, your heater will be switched off.
// This feature exists to protect your hotend from overheating accidentally, but *NOT* from thermistor short/failure!
// You should use MINTEMP for thermistor short/failure protection.
static const float HEATER_MAXTEMP[] = {365.0, 365.0, 365.0};
#define BED_MAXTEMP 200

static unsigned char soft_pwm_bed = 0;

// Init min and max temp with extreme values to prevent false errors during startup
static uint8_t temp_error_cnt[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);

static uint8_t temp_error_bed_cnt = 0;

static void min_temp_error(uint8_t e);
static void max_temp_error(uint8_t e);
static void bed_max_temp_error();

static uint32_t max_heating_start_millis[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);
static float max_heating_start_temperature[EXTRUDERS] = ARRAY_BY_EXTRUDERS_INIT(0);

static const int16_t (*heater_ttbl_map[EXTRUDERS])[2] = ARRAY_BY_EXTRUDERS( HEATER_0_TEMPTABLE, HEATER_1_TEMPTABLE, HEATER_2_TEMPTABLE );
static uint8_t heater_ttbllen_map[EXTRUDERS] = ARRAY_BY_EXTRUDERS( HEATER_0_TEMPTABLE_LEN, HEATER_1_TEMPTABLE_LEN, HEATER_2_TEMPTABLE_LEN );

static float analog2temp(int16_t raw, uint8_t e);
static float analog2tempBed(int16_t raw);
static void updateTemperaturesFromRawValues();

#define ERROR_SETTLING_TIME_MILLIS 10
#define ERROR_COUNT_INCREASE 10
#define ERROR_COUNT_INCREASE_NOT_PRESENT 4
#define ERROR_COUNT_DECREASE 1
#define ERROR_COUNT_BED_TEMP_OVERFLOW_TRIGGER   20          // The number of bed temperature errors before a bed temperature overflow will be triggered

// Save the last pwm level of the heaters as the bed has to be adjusted (possibly) the moment another heater is set.
// Because manage_heaters() is not guaranteed to be called fast enough, save the last PWM level of the heaters ...
static uint8_t last_pwm_output[EXTRUDERS] = { 0 };

static void setHeaterOutput(int8_t heater, uint8_t output);

//===========================================================================
//================================ Functions ================================
//===========================================================================

void setAllowHotendRemoval(uint8_t extruder, bool allow_removal)
{
    if (extruder >= EXTRUDERS)
        return;
    if (allow_removal)
    {
        if (current_temperature[extruder] > 70)
            return;
        if (hotend_pid[extruder].getTargetTemperature() != 0.0)
            return;
    }
    allow_temperature_sensor_errors[extruder] = allow_removal;
}

/// Safety feature for nozzle temperature.
/// When the output has been set to maximum for too long a time without the temperature increasing,
/// then most likely we have a hardware problem and should disable the heaters.
void nozzle_temperature_watchdog(uint8_t e)
{
    if (last_pwm_output[e] == PID_MAX)
    {
        if ((current_temperature[e] - max_heating_start_temperature[e] > MAX_HEATING_TEMPERATURE_INCREASE)
            || max_heating_start_millis[e] == 0)
        {
            max_heating_start_millis[e] = millis();
            max_heating_start_temperature[e] = current_temperature[e];
        }
        else if (millis() - max_heating_start_millis[e] > MAX_HEATING_CHECK_MILLIS)
        {
            //Did not heat up MAX_HEATING_TEMPERATURE_INCREASE in MAX_HEATING_CHECK_MILLIS while the PID was at the maximum.
            //Potential problems could be that the heater is not working, or the temperature sensor is not measuring what the heater is heating.
            disable_all_heaters();
            stop(STOP_REASON_HEATER_ERROR(e));
        }
    }
    else
    {
        max_heating_start_millis[e] = 0;
    }
}

void manage_heater()
{
#if defined(HEATER_0_USES_ADS101X) || defined(HEATER_1_USES_ADS101X) || defined(HEATER_2_USES_ADS101X)
  hotend_temperature_adc.updateState();
#endif
  if (Board::getId() == Board::BOARD_2621B)
  {
    bed_temperature_adc.updateState();
  }

  if (!temp_meas_ready)
  {
    return;
  }

  updateTemperaturesFromRawValues();

  for (uint8_t e = 0; e < EXTRUDERS; e++)
  {
      // Stock ARM manage_heater 0x3462c..0x346f8: no S-line error-count logging gate.
      if (stop_heaters_pwm)
          pwr.setRequestedHeaterOutput(e, 0);
      else
          pwr.setRequestedHeaterOutput(e, hotend_pid[e].update(current_temperature[e]));
      nozzle_temperature_watchdog(e);
  }
  pwr.setRequestedBedOutput(heated_bed_pid.update(current_temperature_bed));
  pwr.setCurrentBedTemperature(current_temperature_bed);
  for (uint8_t e = 0; e < EXTRUDERS; e++)
  {
      setHeaterOutput(e, pwr.getActualHeaterOutput(e));
  }
  setHeaterOutput(-1, pwr.getActualBedOutput());

  // which fan pins need to be turned on?
  uint8_t fanState = 0;
  for (uint8_t e = 0; e < EXTRUDERS; e++)
  {
    // Mark the nozzle as present when both the EEPROM are working AND the temperature reading makes sense.
    hotend_present[e] = i2cOneWireReset(e) && (current_temperature_raw[e] < MAGIC_NO_PT100_CONNECTED_RAW_VALUE);

    // On reboot a hotend cartridge may be present and hot, if so enable it so the fan is turned on!
    // The rationale for not using EXTRUDER_AUTO_FAN_TEMPERATURE is that it is reasonable to expect 40 degC ambient temperature.
    // It would also be annoying to have temperature error warnings(reboot to clear) if you remove your cold hotends at ambient temp > 40 degC.
    if (hotend_present[e] && current_temperature[e] >= HOTEND_HUMAN_TOUCHABLE_TEMPERATURE + 2)
        allow_temperature_sensor_errors[e] = false;

    if ((current_temperature[e] > EXTRUDER_AUTO_FAN_TEMPERATURE) && !allow_temperature_sensor_errors[e])
      fanState |= 1;
    if ((current_temperature[e] < EXTRUDER_AUTO_FAN_TEMPERATURE - 5) || allow_temperature_sensor_errors[e])
      fanState |= 2;
  }

  if (fanState & 1)
    setHotendCoolingFanSpeed(255);
  else if (fanState & 2)
    setHotendCoolingFanSpeed(0);
}

// Reconstructed original combined setter, stock ARM 0x34938.
static void setHeaterOutput(int8_t heater, uint8_t output)
{
    if (heater < 0)
    {
        if (stop_heaters_pwm && output != PID_MAX)
            output = 0;
        // Preserve the stock AVR standard-bed-first branch order (0x102f4).
        if (Board::getId() != Board::BOARD_2621B)
            analogWrite(HEATER_BED_PIN, output);
        else
            soft_pwm_bed = output / 2;
        bed_heater_output_accumulator += output;
        bed_heater_output_accumulator_counter += 1;
    }
    else
    {
        if (allow_temperature_sensor_errors[heater])
            output = 0;
        if (stop_heaters_pwm)
            output = 0;
        last_pwm_output[heater] = output;
        heater_output_accumulator[heater] += output;
        heater_output_accumulator_counter[heater] += 1;
        if (heater == 0)
            analogWrite(HEATER_0_PIN, output);
#if EXTRUDERS > 1
        else if (heater == 1)
            analogWrite(HEATER_1_PIN, output);
#endif
    }
}

#define PGM_RD_W(x)   ((short)pgm_read_word(&x))
static float analog2temp_sub(int16_t raw, const int16_t tt[][2], uint8_t table_len)
{
    float celsius = 0;
    uint8_t i;

    for (i = 1; i < table_len; i++)
    {
        if (PGM_RD_W(tt[i][0]) > raw)
        {
            // Calculate temperature by linear interpolating the temperature conversion table.
            celsius = PGM_RD_W(tt[i - 1][1]) +
                (raw - PGM_RD_W(tt[i - 1][0])) *
                (float)(PGM_RD_W(tt[i][1]) - PGM_RD_W(tt[i - 1][1])) /
                (float)(PGM_RD_W(tt[i][0]) - PGM_RD_W(tt[i - 1][0]));
            // Break out of loop since value was found.
            break;
        }
    }

    // Overflow: Set to last value in the table
    if (i == table_len)
        celsius = PGM_RD_W(tt[i - 1][1]);

    return celsius;
}

static float analog2temp(int16_t raw, uint8_t e)
{
    return analog2temp_sub(raw, heater_ttbl_map[e], heater_ttbllen_map[e]);
}

static void temperature_error_checking()
{
    for (uint8_t e = 0; e < EXTRUDERS; e++)
    {
        if (!allow_temperature_sensor_errors[e])
        {
            GainCompensator compensator = GainCompensatorFactory::getInstance(GainCompensatorFactory::EXTRUDER, e);
            if (current_temperature[e] >= compensator.compensate(HEATER_MAXTEMP[e]))
            {
                if (hotend_present[e])
                {
                    temp_error_cnt[e] += ERROR_COUNT_INCREASE;
                }
                else
                {
                    temp_error_cnt[e] += ERROR_COUNT_INCREASE_NOT_PRESENT;
                }

                if (temp_error_cnt[e] > ERROR_SETTLING_TIME_MILLIS * ERROR_COUNT_INCREASE)
                {
                    max_temp_error(e);
                }
            }
            else if (current_temperature[e] <= compensator.compensate(HEATER_MINTEMP[e]))
            {
                temp_error_cnt[e] += ERROR_COUNT_INCREASE;
                if (temp_error_cnt[e] > ERROR_SETTLING_TIME_MILLIS * ERROR_COUNT_INCREASE)
                {
                    min_temp_error(e);
                }
            }
            else if (temp_error_cnt[e] > 0)
            {
                temp_error_cnt[e] -= ERROR_COUNT_DECREASE;
            }
        }
        else
        {
            temp_error_cnt[e] = 0;
        }
    }

#ifdef BED_MAXTEMP
    if (current_temperature_bed > BED_MAXTEMP)
    {
        temp_error_bed_cnt++;
        if (temp_error_bed_cnt > ERROR_COUNT_BED_TEMP_OVERFLOW_TRIGGER)
        {
            heated_bed_pid.setTargetTemperature(0);
            bed_max_temp_error();
        }
    }
    else if (temp_error_bed_cnt > 0)
    {
        temp_error_bed_cnt--;
    }
#endif
}

static float analog2tempBed(int16_t raw)
{
    if (Board::getId() == Board::BOARD_2621B)
    {
        return analog2temp_sub(raw, temptable_21, TEMPTABLE_21_LEN);
    }
    else
    {
        return analog2temp_sub(raw, temptable_20, TEMPTABLE_20_LEN);
    }
}

/* Called to get the raw values into the the actual temperatures. The raw values are
 * created in interrupt context, and this function is called from normal context as
 * it is too slow to run in interrupts and will block the stepper routine otherwise
 * */
static void updateTemperaturesFromRawValues()
{
#if defined(HEATER_0_USES_ADS101X) || defined(HEATER_1_USES_ADS101X) || defined(HEATER_2_USES_ADS101X)
    current_temperature_raw[0] = hotend_temperature_adc.getResult(0) * OVERSAMPLENR;
    current_temperature_raw[1] = hotend_temperature_adc.getResult(1) * OVERSAMPLENR;
#endif

    if (Board::getId() == Board::BOARD_2621B)
    {
        current_temperature_bed_raw = bed_temperature_adc.getResult(0) * OVERSAMPLENR;
    }

    GainCompensator compensator=GainCompensatorFactory::getInstance(GainCompensatorFactory::BUILDPLATE, 0);
    current_temperature_bed = compensator.compensate(analog2tempBed(current_temperature_bed_raw));

    for (uint8_t e = 0; e < EXTRUDERS; e++)
    {
        compensator = GainCompensatorFactory::getInstance(GainCompensatorFactory::EXTRUDER, e);
        current_temperature[e] = compensator.compensate(analog2temp(current_temperature_raw[e], e));
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        temp_meas_ready = false;
    }

    temperature_error_checking();

    // Reset the watchdog after we know we have a temperature measurement.
    watchdog_reset();
}

/**
 * Initialize the temperature manager
 * The manager is implemented by periodic calls to manage_heater()
 */
void temperatureInit()
{
#if defined(HEATER_0_USES_ADS101X) || defined(HEATER_1_USES_ADS101X) || defined(HEATER_2_USES_ADS101X)
    hotend_temperature_adc.init();
#endif
    if (Board::getId() == Board::BOARD_2621B)
    {
        bed_temperature_adc.init();
    }

    for (uint8_t e = 0; e < EXTRUDERS; e++)
    {
        hotend_pid[e].setControlRange(HEATER_MINTEMP[e], HEATER_MAXTEMP[e]);
    }

    heated_bed_pid.setControlRange(BED_MINTEMP, BED_MAXTEMP);

  #if defined(HEATER_0_PIN) && (HEATER_0_PIN > -1)
    SET_OUTPUT(HEATER_0_PIN);
  #endif
  #if defined(HEATER_1_PIN) && (HEATER_1_PIN > -1)
    SET_OUTPUT(HEATER_1_PIN);
  #endif
  #if defined(HEATER_2_PIN) && (HEATER_2_PIN > -1)
    SET_OUTPUT(HEATER_2_PIN);
  #endif
  #if defined(HEATER_BED_PIN) && (HEATER_BED_PIN > -1)
    // Stock AVR 0x10438 branches to board 4; the standard bed path is first.
    if (Board::getId() != Board::BOARD_2621B)
        SET_OUTPUT(HEATER_BED_PIN);
    else
        SET_OUTPUT(HEATER_BED_XL_PIN);
  #endif

  // Set analog inputs
  ADCSRA = 1<<ADEN | 1<<ADSC | 1<<ADIF | 0x07;
  // Disable the digital inputs that are shared with the ADC. This will reduce current consumption.
  DIDR0 = 0;
  #ifdef DIDR2
    DIDR2 = 0;
  #endif
  #if defined(TEMP_0_PIN) && (TEMP_0_PIN > -1)
    #if TEMP_0_PIN < 8
       DIDR0 |= 1 << TEMP_0_PIN;
    #else
       DIDR2 |= 1<<(TEMP_0_PIN - 8);
    #endif
  #endif
  #if defined(TEMP_1_PIN) && (TEMP_1_PIN > -1)
    #if TEMP_1_PIN < 8
       DIDR0 |= 1<<TEMP_1_PIN;
    #else
       DIDR2 |= 1<<(TEMP_1_PIN - 8);
    #endif
  #endif
  #if defined(TEMP_2_PIN) && (TEMP_2_PIN > -1)
    #if TEMP_2_PIN < 8
       DIDR0 |= 1 << TEMP_2_PIN;
    #else
       DIDR2 |= 1<<(TEMP_2_PIN - 8);
    #endif
  #endif
  #if defined(TEMP_BED_PIN) && (TEMP_BED_PIN > -1)
       if (Board::getId() != Board::BOARD_2621B)
       {
            #if TEMP_BED_PIN < 8
            DIDR0 |= 1<<TEMP_BED_PIN;
            #else
            DIDR2 |= 1<<(TEMP_BED_PIN - 8);
            #endif
       }
       else
       {
           // nothing to do here for the UM3.1 board
       }
  #endif

  // Use timer0 for temperature measurement
  // Interleave temperature interrupt with millies interrupt
  OCR0B = 128;
  TIMSK0 |= (1<<OCIE0B);
}

void disable_bed()
{
    heated_bed_pid.setTargetTemperature(0);

    switch (Board::getId()) {
    case Board::BOARD_2621B:
        WRITE(HEATER_BED_XL_PIN, LOW);
        soft_pwm_bed = 0;
        break;
    default:
#if defined(HEATER_BED_PIN) && HEATER_BED_PIN > -1
        WRITE(HEATER_BED_PIN, LOW);
#endif
        break;
    }
}

void disable_all_heaters()
{
    for(uint8_t e=0; e<EXTRUDERS; e++)
        hotend_pid[e].setTargetTemperature(0);

    #if defined(HEATER_0_PIN) && HEATER_0_PIN > -1
    WRITE(HEATER_0_PIN, LOW);
    #endif
    #if defined(HEATER_1_PIN) && HEATER_1_PIN > -1
    WRITE(HEATER_1_PIN,LOW);
    #endif
    #if defined(HEATER_2_PIN) && HEATER_2_PIN > -1
    WRITE(HEATER_2_PIN,LOW);
    #endif

    disable_bed();
}

void max_temp_error(uint8_t e) {
  disable_all_heaters();
  stop(STOP_REASON_MAXTEMP(e));
}

void min_temp_error(uint8_t e) {
  disable_all_heaters();
  stop(STOP_REASON_MINTEMP(e));
}

void bed_max_temp_error(void) {
  disable_bed();
  stop(STOP_REASON_MAXTEMP_BED);
}

// Stock ARM soft_pwm 0x353c8. Division belongs in setHeaterOutput,
// and the 7-bit cycle wraps after 128 ticks, unlike later S-line code.
void soft_pwm(void)
{
    static uint8_t pwm_count = 1;
    static uint8_t soft_pwm_b = 0;
    if (Board::getId() != Board::BOARD_2621B)
        return;
    if (pwm_count == 0)
    {
        soft_pwm_b = soft_pwm_bed;
        if (soft_pwm_b > 0)
            WRITE(HEATER_BED_XL_PIN, HIGH);
    }
    if (soft_pwm_b <= pwm_count)
        WRITE(HEATER_BED_XL_PIN, LOW);
    pwm_count = (pwm_count + 1) & 0x7f;
}

/**
 * Timer 0 is shared with millies
 *  - Manage PWM to all the heaters and fan
 *  - Update the raw temperature values
 *  - Check new temperature values for MIN/MAX errors
 */
ISR(TIMER0_COMPB_vect)
{
  // These variables are only accessible from the ISR, but static, so they don't lose their value
  static unsigned char temp_count = 0;
  static uint32_t raw_temp_0_value = 0;
#if EXTRUDERS > 1
  static uint32_t raw_temp_1_value = 0;
#endif
#if defined(TEMP_2_PIN) && (TEMP_2_PIN > -1) && EXTRUDERS > 2
  static uint32_t raw_temp_2_value = 0;
#endif
  static uint32_t raw_temp_bed_value = 0;
  static unsigned char temp_state = 5;

  soft_pwm();

  switch (temp_state) {
    case 1: // Measure TEMP_0
      #ifdef HEATER_0_USES_ADS101X
      #elif defined(TEMP_0_PIN) && (TEMP_0_PIN > -1)
        raw_temp_0_value += ADC;
      #endif
      // Prepare TEMP_BED
      #if defined(TEMP_BED_PIN) && (TEMP_BED_PIN > -1)
        #if TEMP_BED_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_BED_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      temp_state = 2;
      break;
    case 2: // Measure TEMP_BED
      if (Board::getId() != Board::BOARD_2621B)
      {
        #if defined(TEMP_BED_PIN) && (TEMP_BED_PIN > -1)
            raw_temp_bed_value += ADC;
        #endif
      }
      // Prepare TEMP_1
      #if defined(TEMP_1_PIN) && (TEMP_1_PIN > -1) && EXTRUDERS > 1
        #if TEMP_1_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_1_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      temp_state = 3;
      break;
    case 3: // Measure TEMP_1
      #ifdef HEATER_1_USES_ADS101X
      #elif defined(TEMP_1_PIN) && (TEMP_1_PIN > -1) && EXTRUDERS > 1
        raw_temp_1_value += ADC;
      #endif
      // Prepare TEMP_2
      #if defined(TEMP_2_PIN) && (TEMP_2_PIN > -1) && EXTRUDERS > 2
        #if TEMP_2_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_2_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      temp_state = 4;
      break;
    case 4: // Measure TEMP_2
      #if defined(TEMP_2_PIN) && (TEMP_2_PIN > -1) && EXTRUDERS > 2
        raw_temp_2_value += ADC;
      #endif
      temp_count++;
      // Fall through to state 0
    case 0: // Prepare TEMP_0
      #if defined(TEMP_0_PIN) && (TEMP_0_PIN > -1)
        #if TEMP_0_PIN > 7
          ADCSRB = 1<<MUX5;
        #else
          ADCSRB = 0;
        #endif
        ADMUX = ((1 << REFS0) | (TEMP_0_PIN & 0x07));
        ADCSRA |= 1<<ADSC; // Start conversion
      #endif
      temp_state = 1;
      break;
    case 5: //Startup, delay initial temp reading a tiny bit so the hardware can settle.
      temp_state = 0;
      break;
  }


  if (temp_count >= OVERSAMPLENR) // 4 timer ticks per sample * 8 = 32.768ms at 16MHz.
  {
    if (!temp_meas_ready) //Only update the raw values if they have been read. Else we could be updating them during reading.
    {
      current_temperature_raw[0] = raw_temp_0_value;
#if EXTRUDERS > 1
      current_temperature_raw[1] = raw_temp_1_value;
#endif
#if EXTRUDERS > 2
      current_temperature_raw[2] = raw_temp_2_value;
#endif
      current_temperature_bed_raw = raw_temp_bed_value;
    }

    temp_meas_ready = true;
    temp_count = 0;
    raw_temp_0_value = 0;
#if EXTRUDERS > 1
    raw_temp_1_value = 0;
#endif
#if defined(TEMP_2_PIN) && (TEMP_2_PIN > -1) && EXTRUDERS > 2
    raw_temp_2_value = 0;
#endif
    raw_temp_bed_value = 0;
  }
}


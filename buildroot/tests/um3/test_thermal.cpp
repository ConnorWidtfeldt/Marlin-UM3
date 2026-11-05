/** Exercise the production UM3 thermal math; this does not drive hardware. */
#include "temperature_pid.h"
#include "temperature_conversion.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using ultimaker3::TemperaturePID;
using ultimaker3::hotend_temperature_from_adc;
using ultimaker3::hotend_temperature_from_raw_sum;
using ultimaker3::bed_temperature_from_raw_sum;

static unsigned checks = 0;

static void require(const bool condition, const char *description) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    std::exit(1);
  }
}

static void expect_float(const float actual, const float expected, const char *description) {
  ++checks;
  if (!std::isfinite(actual) || std::fabs(actual - expected) > 0.0001f) {
    std::cerr << "FAIL: " << description << ": expected " << expected << ", got " << actual << '\n';
    std::exit(1);
  }
}

static void expect_pwm(const uint8_t actual, const uint8_t expected, const char *description) {
  ++checks;
  if (actual != expected) {
    std::cerr << "FAIL: " << description << ": expected " << unsigned(expected)
              << ", got " << unsigned(actual) << '\n';
    std::exit(1);
  }
}

static void test_conversion_boundaries() {
  expect_float(hotend_temperature_from_raw_sum(-17448), -50, "hotend first knot");
  expect_float(hotend_temperature_from_raw_sum(-14472), 0, "hotend zero knot");
  expect_float(hotend_temperature_from_raw_sum(-13052), 25, "hotend interior interpolation");
  expect_float(hotend_temperature_from_raw_sum(-15960), -25, "signed hotend interpolation");
  expect_float(hotend_temperature_from_raw_sum(16288), 700, "hotend last knot");
  expect_float(hotend_temperature_from_raw_sum(32767), 700, "hotend high clamp");
  expect_float(hotend_temperature_from_raw_sum(-13052, 1.25f, -3), 28.25f, "hotend compensation");
  expect_float(hotend_temperature_from_adc(-1809), 0, "ADS sample scales by eight");
  expect_float(hotend_temperature_from_adc(-1454), 50, "ADS sample positive Celsius knot");
  expect_float(hotend_temperature_from_adc(2047), 700, "ADS maximum signed sample");
  require(std::isfinite(hotend_temperature_from_adc(-2048)), "ADS minimum signed sample accepted");
  require(std::isnan(hotend_temperature_from_adc(-2049)), "ADS under-range rejected");
  require(std::isnan(hotend_temperature_from_adc(2048)), "ADS over-range rejected");
  expect_float(bed_temperature_from_raw_sum(0), 0, "bed first knot");
  expect_float(bed_temperature_from_raw_sum(1816), 1, "bed low knot");
  expect_float(bed_temperature_from_raw_sum(1888), 10.5f, "bed interior interpolation");
  expect_float(bed_temperature_from_raw_sum(-1816), -1, "bed low extrapolation");
  expect_float(bed_temperature_from_raw_sum(7744), 1100, "bed last knot");
  expect_float(bed_temperature_from_raw_sum(32767), 1100, "bed high clamp");
  expect_float(bed_temperature_from_raw_sum(2496, 0.95f, 2), 97, "bed compensation");
  require(std::isnan(hotend_temperature_from_raw_sum(0, NAN)), "invalid hotend gain rejected");
  require(std::isnan(bed_temperature_from_raw_sum(0, 1, INFINITY)), "invalid bed offset rejected");
}

static void test_control_branches() {
  TemperaturePID pid;
  pid.setTargetTemperature(100);
  expect_pwm(pid.update(0, 0), 0, "unconfigured bounds fail closed");
  pid.setControlRange(5, 365);
  expect_pwm(pid.update(20, 0), 0, "startup sample jump rejected");
  expect_pwm(pid.update(20, 0), 255, "stable low sample uses full power");
  expect_pwm(pid.update(25, 0), 255, "five-degree step is accepted");
  expect_pwm(pid.update(30.01f, 0), 0, "step above five degrees rejected");
  expect_pwm(pid.update(30.01f, 0), 255, "stable sample recovers after rejection");
  expect_pwm(pid.update(4, 0), 0, "low bound jump rejected");
  expect_pwm(pid.update(4, 0), 0, "temperature below bounds cannot heat");
  pid.setTargetTemperature(500);
  expect_pwm(pid.update(366, 0), 0, "high bound jump rejected");
  expect_pwm(pid.update(366, 0), 0, "temperature above bounds cannot heat");
  expect_pwm(pid.update(365, 0), 255, "upper bound is inclusive");
  pid.setTargetTemperature(0);
  expect_pwm(pid.update(365, 255), 0, "zero target turns output off");
  pid.setDebugDump(true);
  require(pid.debugDumpEnabled(), "diagnostic flag retained without serial side effects");
  pid.setDebugDump(false);
  require(!pid.debugDumpEnabled(), "diagnostic flag cleared");

  TemperaturePID feedforward;
  feedforward.setControlRange(0, 365);
  feedforward.setTargetTemperature(50);
  feedforward.setKff(2);
  feedforward.setKpcf(0.25f);
  expect_pwm(feedforward.update(50, 0), 0, "feedforward first jump rejected");
  expect_pwm(feedforward.update(50, 100), 55, "ambient and fan feedforward summed");
  feedforward.setTargetTemperature(10);
  expect_pwm(feedforward.update(50, 255), 0, "error below functional range turns off");

  TemperaturePID integral;
  integral.setControlRange(0, 365);
  integral.setTargetTemperature(20);
  integral.setKi(1000);
  integral.setKiMax(7);
  expect_pwm(integral.update(19, 0), 0, "integral first jump rejected");
  expect_pwm(integral.update(19, 0), 7, "positive integral clamps");
  expect_pwm(integral.update(21, 0), 0, "negative integral clamps and output clips");
  integral.setTargetTemperature(100);
  expect_pwm(integral.update(21, 0), 255, "full-power branch resets integral");
  integral.setTargetTemperature(21);
  expect_pwm(integral.update(21, 0), 0, "integral reset survives return to control range");
}

static void test_invalid_inputs() {
  TemperaturePID pid;
  pid.setControlRange(0, 365);
  pid.setTargetTemperature(100);
  expect_pwm(pid.update(NAN, 255), 0, "NaN sensor fails closed");
  expect_pwm(pid.update(INFINITY, 255), 0, "infinite sensor fails closed");
  pid.setTargetTemperature(NAN);
  expect_pwm(pid.update(20, 255), 0, "NaN target fails closed");
  pid.setTargetTemperature(20);
  pid.setKp(INFINITY);
  expect_pwm(pid.update(20, 0), 0, "infinite gain fails closed");
  pid.setTargetTemperature(100);
  pid.setKp(NAN);
  expect_pwm(pid.update(0, 0), 0, "NaN gain prevents full-power branch");
  pid.setKp(0);
  pid.setKiMax(-1);
  expect_pwm(pid.update(0, 0), 0, "negative integral limit prevents full-power branch");
  pid.setKiMax(255);
  pid.setFunctionalRange(-1);
  expect_pwm(pid.update(0, 0), 0, "negative functional range prevents full-power branch");
  pid.setFunctionalRange(INFINITY);
  expect_pwm(pid.update(0, 0), 0, "infinite functional range rejected");
  pid.setFunctionalRange(25);
  pid.setKff(1);
  pid.setTargetTemperature(50);
  expect_pwm(pid.update(50, 0), 0, "valid sample after invalid state jump rejected");
  expect_pwm(pid.update(50, 0), 15, "valid gains recover after invalid state");
  pid.setTargetTemperature(50);
  pid.setKp(std::numeric_limits<float>::max());
  expect_pwm(pid.update(45, 0), 0, "overflowed computed output fails closed");
  pid.setControlRange(100, 50);
  expect_pwm(pid.update(50, 0), 0, "inverted bounds fail closed");
  pid.setControlRange(0, INFINITY);
  expect_pwm(pid.update(50, 0), 0, "nonfinite bounds fail closed");
}

// The runner generates this from the independently replayed stock ARM fixture.
#include "thermal_oracle.h"

int main() {
  test_conversion_boundaries();
  test_control_branches();
  test_invalid_inputs();
  test_stock_oracle();
  std::cout << "UM3 thermal math: " << checks << " checks passed\n";
}

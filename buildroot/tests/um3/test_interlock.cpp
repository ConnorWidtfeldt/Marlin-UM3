/** Exercise recovered production safety state; no GPIO or shutdown is invoked. */
#include "thermal_interlock.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using ultimaker3::HeatingProgressWatchdog;
using ultimaker3::HotendRemovalInterlock;

static unsigned checks = 0;

static void require(const bool condition, const char *description) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    std::exit(1);
  }
}

static void test_removal() {
  HotendRemovalInterlock core;
  require(core.removalAllowed(), "stock cold-start removal permission defaults true");
  require(!core.request(true, 71, 0), "enable over 70C rejected");
  require(core.removalAllowed(), "rejected enable preserves previous true state");
  require(core.request(false, 200, 200), "disable accepted regardless of temperature/target");
  require(!core.removalAllowed(), "disable clears permission");
  require(!core.request(true, 70.01f, 0), "enable just above 70C rejected");
  require(!core.removalAllowed(), "rejected enable preserves previous false state");
  require(!core.request(true, 20, 0.001f), "nonzero target blocks removal");
  require(!core.request(true, 20, -0.001f), "negative nonzero target also blocks removal");
  require(core.request(true, 70, 0), "exact 70C with target zero accepted");
  core.observe(false, 100);
  require(core.removalAllowed(), "absent core does not auto-revoke finite-temperature permission");
  core.observe(true, 61.999f);
  require(core.removalAllowed(), "present core below 62C keeps permission");
  core.observe(true, 62);
  require(!core.removalAllowed(), "present core at 62C auto-revokes permission");
  core.observe(true, 20);
  require(!core.removalAllowed(), "cooling does not automatically restore permission");
  require(core.request(true, 20, -0.0f), "negative zero target is zero");
  require(!core.request(true, NAN, 0), "NaN temperature cannot enable removal");
  require(core.removalAllowed(), "invalid enable preserves state");
  require(!core.request(true, 20, INFINITY), "infinite target cannot enable removal");
  core.observe(false, NAN);
  require(!core.removalAllowed(), "unknown temperature revokes physical removal permission");
  require(core.request(false, NAN, NAN), "disable remains possible with invalid readings");

  HotendRemovalInterlock independent;
  require(independent.removalAllowed(), "one hotend does not change another slot");
}

static void test_watchdog() {
  HeatingProgressWatchdog watchdog;
  require(watchdog.startMillis() == 0, "watchdog starts without timer");
  require(!watchdog.update(255, 20, 100), "first full-power sample starts timer");
  require(watchdog.startMillis() == 100 && watchdog.startTemperature() == 20, "initial baseline recorded");
  require(!watchdog.update(255, 21, 30100), "exactly 1C rise and 30000ms does not fault");
  require(watchdog.startMillis() == 100, "exactly 1C rise does not refresh baseline");
  require(watchdog.update(255, 21, 30101), "strictly over 30000ms without over-1C rise faults");
  require(watchdog.update(255, 19, 30102), "temperature decline continues to fault");
  require(!watchdog.update(255, 21.001f, 30103), "over-1C rise refreshes baseline before time check");
  require(watchdog.startMillis() == 30103, "progress resets timestamp");
  require(!watchdog.update(254, 10, 90000), "budget-limited output stops full-power timing");
  require(watchdog.startMillis() == 0, "non-full output clears timestamp");
  require(watchdog.startTemperature() == 21.001f, "non-full output retains unused temperature baseline");
  require(!watchdog.update(255, 10, 90001), "next full output begins a new window");
  require(watchdog.startTemperature() == 10, "restart captures fresh baseline");
  require(!watchdog.update(0, 10, 200000), "off output cannot cause progress timeout");

  HeatingProgressWatchdog wrapping;
  require(!wrapping.update(255, 20, UINT32_MAX - 1000), "pre-wrap baseline accepted");
  require(!wrapping.update(255, 20, 28999), "wraparound exactly 30000ms accepted");
  require(wrapping.update(255, 20, 29000), "wraparound 30001ms faults");

  HeatingProgressWatchdog zero_clock;
  require(!zero_clock.update(255, 20, 0), "zero clock records zero sentinel");
  require(!zero_clock.update(255, 20, 30001), "stock zero-sentinel corner restarts window");
  require(zero_clock.startMillis() == 30001, "zero sentinel was replaced on next update");
  require(!zero_clock.update(255, 20, 60001), "restarted zero-clock window exact timeout accepted");
  require(zero_clock.update(255, 20, 60002), "restarted zero-clock window eventually faults");

  HeatingProgressWatchdog invalid;
  require(invalid.update(0, NAN, 0), "invalid sensor faults even with heater off");
  require(invalid.update(255, INFINITY, 100), "infinite sensor faults");
  require(invalid.startMillis() == 0 && invalid.startTemperature() == 0, "invalid sensor clears baseline");
  require(!invalid.update(255, 20, 200), "valid input can restart helper; caller must latch faults");
}

#include "interlock_oracle.h"

int main() {
  test_removal();
  test_watchdog();
  test_stock_oracle();
  std::cout << "UM3 thermal interlocks: " << checks << " checks passed\n";
}

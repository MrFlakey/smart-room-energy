#include <unity.h>
#include <math.h>
#include "room_logic.h"

static RoomInputs in(uint32_t t, bool pir, float temp = 22, float light = 20, float knob = 25) {
  return RoomInputs{t, pir, -1, temp, light, knob};
}

void setUp() {}
void tearDown() {}

void test_everything_off_before_anyone_arrives() {
  RoomController r;
  r.update(in(0, false, 30, 0));
  TEST_ASSERT_FALSE(r.occupied());
  TEST_ASSERT_EQUAL(0, r.outputs().led1);
  TEST_ASSERT_EQUAL(0, r.outputs().fan);
}

void test_dark_room_lights_on_when_occupied() {
  RoomController r;
  r.update(in(0, true, 22, 0));
  TEST_ASSERT_TRUE(r.occupied());
  TEST_ASSERT_EQUAL(255, r.outputs().led1);
  TEST_ASSERT_EQUAL(255, r.outputs().led2);
}

void test_bright_room_keeps_lights_off() {
  RoomController r;
  r.update(in(0, true, 22, 80));
  TEST_ASSERT_EQUAL(0, r.outputs().led1);
}

void test_lights_dim_with_daylight() {
  RoomController r;
  r.update(in(0, true, 22, 30));
  uint8_t v = r.outputs().led1;
  TEST_ASSERT_TRUE(v > 60 && v < 255);
}

void test_ultrasonic_counts_as_presence() {
  RoomController r;
  r.update(RoomInputs{0, false, 50, 22, 0, 25});
  TEST_ASSERT_TRUE(r.occupied());
}

void test_loads_off_after_vacancy_timeout() {
  RoomController r;
  r.update(in(0, true, 22, 0));
  r.update(in(61000, false, 22, 0));
  TEST_ASSERT_FALSE(r.occupied());
  TEST_ASSERT_EQUAL(255, r.outputs().led1);  // still within the 5 min grace period
  r.update(in(300001, false, 22, 0));
  TEST_ASSERT_EQUAL(0, r.outputs().led1);
}

void test_fan_follows_temperature_with_hysteresis() {
  RoomController r;
  r.update(in(0, true, 26, 80, 25));
  TEST_ASSERT_TRUE(r.outputs().fan >= 102);
  r.update(in(1000, true, 24.5, 80, 25));  // below setpoint but inside hysteresis
  TEST_ASSERT_TRUE(r.outputs().fan > 0);
  r.update(in(2000, true, 23.9, 80, 25));
  TEST_ASSERT_EQUAL(0, r.outputs().fan);
  r.update(in(3000, true, 29, 80, 25));
  TEST_ASSERT_EQUAL(255, r.outputs().fan);
}

void test_missing_temperature_turns_fan_off() {
  RoomController r;
  r.update(in(0, true, NAN, 80));
  TEST_ASSERT_EQUAL(0, r.outputs().fan);
}

void test_knob_sets_setpoint_but_remote_wins_until_knob_moves() {
  RoomController r;
  r.update(in(0, true, 22, 80, 24));
  TEST_ASSERT_EQUAL_FLOAT(24, r.setpoint());
  r.setSetpoint(21);
  r.update(in(200, true, 22, 80, 24.2));  // knob jitter inside the deadband
  TEST_ASSERT_EQUAL_FLOAT(21, r.setpoint());
  r.update(in(400, true, 22, 80, 27));
  TEST_ASSERT_EQUAL_FLOAT(27, r.setpoint());
}

void test_override_holds_until_room_is_vacant() {
  RoomController r;
  r.update(in(0, true, 22, 0));
  r.toggleLights();  // turn off by hand
  r.update(in(1000, true, 22, 0));
  TEST_ASSERT_EQUAL(0, r.outputs().led1);
  TEST_ASSERT_TRUE(r.lightsOverridden());
  r.update(in(302000, false, 22, 0));  // vacant long enough: override cleared
  TEST_ASSERT_FALSE(r.lightsOverridden());
  r.update(in(303000, true, 22, 0));   // someone comes back: auto again
  TEST_ASSERT_EQUAL(255, r.outputs().led1);
}

void test_manual_mode_ignores_rules_and_keeps_levels() {
  RoomController r;
  r.update(in(0, true, 22, 0));
  r.setMode(RoomMode::Manual);
  r.update(in(400000, false, 30, 0));  // vacant and hot: auto would change things
  TEST_ASSERT_EQUAL(255, r.outputs().led1);
  TEST_ASSERT_EQUAL(0, r.outputs().fan);
  r.setFan(200);
  r.update(in(401000, false, 30, 0));
  TEST_ASSERT_EQUAL(200, r.outputs().fan);
}

void test_state_change_reported_once() {
  RoomController r;
  r.update(in(0, true, 22, 0));
  TEST_ASSERT_TRUE(r.consumeStateChanged());
  TEST_ASSERT_FALSE(r.consumeStateChanged());
  r.update(in(200, true, 22, 5));  // brightness shifts slightly: not a state change
  TEST_ASSERT_FALSE(r.consumeStateChanged());
  r.toggleMode();
  TEST_ASSERT_TRUE(r.consumeStateChanged());
}

void test_energy_meter_integrates_power() {
  EnergyMeter e;
  e.add(10, 0);
  e.add(10, 3600000UL / 2);  // 10 W for half an hour
  TEST_ASSERT_FLOAT_WITHIN(0.001, 5.0, e.wh());
  e.add(NAN, 3600000UL);     // wattmeter missing: nothing added
  TEST_ASSERT_FLOAT_WITHIN(0.001, 5.0, e.wh());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_everything_off_before_anyone_arrives);
  RUN_TEST(test_dark_room_lights_on_when_occupied);
  RUN_TEST(test_bright_room_keeps_lights_off);
  RUN_TEST(test_lights_dim_with_daylight);
  RUN_TEST(test_ultrasonic_counts_as_presence);
  RUN_TEST(test_loads_off_after_vacancy_timeout);
  RUN_TEST(test_fan_follows_temperature_with_hysteresis);
  RUN_TEST(test_missing_temperature_turns_fan_off);
  RUN_TEST(test_knob_sets_setpoint_but_remote_wins_until_knob_moves);
  RUN_TEST(test_override_holds_until_room_is_vacant);
  RUN_TEST(test_manual_mode_ignores_rules_and_keeps_levels);
  RUN_TEST(test_state_change_reported_once);
  RUN_TEST(test_energy_meter_integrates_power);
  return UNITY_END();
}

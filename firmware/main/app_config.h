// Room behaviour and timing. Everything you might want to tune is here.
// Wi-Fi, broker and room id are set in menuconfig ("Smart Room" menu).
#pragma once

// Occupancy and door
#define EMPTY_AFTER_S           15.0f   // room is empty this long after the last PIR motion
#define DOOR_OPEN_CM            40.0f   // farther than this = door open (sensor faces the closed door;
                                        // the ESP32 reads anything under ~20 cm as ~20 cm)
#define DOOR_DEBOUNCE_READS     3       // readings in a row before the door state changes
#define DOOR_PAUSE_S            10.0f   // door open this long pauses the AC (fan) in auto mode

// Setpoint
#define SET_MIN_C               18.0f
#define SET_MAX_C               30.0f
#define SET_DEFAULT_C           25.0f   // used when the knob can't be read at boot
#define KNOB_DEADBAND_C         0.5f    // the knob only counts once it moves more than this
// The DFR0058 is a 10-turn pot (about 330 mV per turn on 3V3). 18..30 C uses
// the middle 3 turns, about 4 C per turn; past either end it holds 18 or 30.
#define KNOB_MIN_MV             1150
#define KNOB_MAX_MV             2150

// Fan ("AC") in auto mode
#define FAN_ON_ABOVE_C          0.3f    // starts when the room is this much over the setpoint
#define FAN_OFF_BELOW_C         0.0f    // stops when it is back at or under the setpoint
#define FAN_FULL_AT_C           3.0f    // full speed at this many degrees over
#define FAN_MIN_PCT             30      // slowest auto speed, so the fan doesn't stall

// DFR0026 light sensor: output in mV that counts as 0 % (dark) and 100 % (bright
// room). It only gives a few hundred mV in room light; set these from the
// "sensors:" line on the serial monitor with the room lights off and on.
#define LIGHT_DARK_MV           0
#define LIGHT_BRIGHT_MV         1000

// Daylight dimming in auto mode (ambient light %, 0 dark .. 100 bright)
#define LIGHT_FULL_BELOW_PCT    10.0f   // darker than this: lights at 100 %
#define LIGHT_OFF_ABOVE_PCT     60.0f   // brighter than this: lights off
#define LIGHT_MIN_ON_PCT        10      // auto levels under this are switched off
#define LIGHT_STEP_PCT          5       // auto levels are rounded to this step
#define LIGHT_RAMP_PCT_PER_S    40      // how fast auto dimming moves

// Energy. The wattmeter only meters the fan supply; LED power is estimated.
#define LED_FULL_W              0.02f   // one LED at full brightness (3.3 V, 220 ohm)
#define FAN_FULL_W_DEFAULT      2.0f    // fan at 100 % until a real reading replaces it

// Sensor fault detection
#define FAULT_AFTER_BAD_READS   5       // bad readings in a row before a sensor counts as failed
#define RECOVER_AFTER_GOOD_READS 15     // good readings in a row before it counts as recovered
#define LM35_MIN_C              1.0f    // readings outside this range are implausible
#define LM35_MAX_C              60.0f
#define LM35_MAX_JUMP_C         5.0f    // between two readings 200 ms apart
#define ANALOG_JUMP_PCT         30.0f   // light/knob: a jump this big counts as noise...
#define ANALOG_JUMPS_FOR_FAULT  4       // ...and this many within ANALOG_JUMP_WINDOW reads is a fault
#define ANALOG_JUMP_WINDOW      15
#define PIR_PROBE_ENABLE        1       // check that something drives the PIR pin (see sensors.c)
#define PIR_PROBE_EVERY_MS      2000
#define POWER_RETRY_MS          10000   // try to find a missing wattmeter this often

// Task periods
#define CONTROL_PERIOD_MS       100
#define SENSOR_PERIOD_MS        200
#define POWER_PERIOD_MS         500
#define TELEMETRY_PERIOD_MS     2000
#define STATE_MIN_INTERVAL_MS   500     // state is re-published at most this often

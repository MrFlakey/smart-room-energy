// Data shared between the tasks. Each struct has one writer task; readers take
// a copy with the get function, so nobody holds a lock while doing real work.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SENSOR_PIR,
    SENSOR_ULTRASONIC,
    SENSOR_LM35,
    SENSOR_LIGHT,
    SENSOR_ROTATION,
    SENSOR_SEN0291,
    SENSOR_COUNT,
} sensor_id_t;

// Keys used on the faults topic and in fault events
extern const char *const SENSOR_KEYS[SENSOR_COUNT];

// Written by the sensor and power tasks
typedef struct {
    bool failed[SENSOR_COUNT];
    const char *reason[SENSOR_COUNT];   // short reason while failed, static strings

    bool pir;              // raw PIR output now
    float since_motion_s;  // -1 if no motion seen since boot
    float dist_cm;         // valid unless the ultrasonic failed
    bool door_open;        // debounced, false while the ultrasonic failed
    float temp_c;          // valid unless lm35 failed
    float light_pct;       // valid unless light failed
    float knob_pct;        // valid unless rotation failed
    float fan_w;           // metered fan supply power, valid unless sen0291 failed
} sensors_t;

typedef enum { MODE_AUTO, MODE_MANUAL } room_mode_t;

// Written by the control task
typedef struct {
    room_mode_t mode;
    bool occ;
    float set_c;
    int light;           // 0..100, both LEDs
    int fan;             // 0..100
    bool lights_ovr;
    bool ac_ovr;
    bool ac_paused;
    bool door_open;
    float power_w;       // fan (metered) + LEDs (estimated); valid unless sen0291 failed
    double energy_wh;
    float saved_pct;
} room_t;

void room_data_init(void);

void sensors_get(sensors_t *out);
void sensors_put(const sensors_t *in);   // sensor task fields only, see sensors.c
void sensors_put_power(bool failed, const char *reason, float fan_w);

void room_get(room_t *out);
void room_put(const room_t *in);

// Room control: occupancy, door, setpoint, lights, fan, overrides and energy.
// Runs in its own task every 100 ms from the latest sensor snapshot, so it
// never waits on a sensor or the network.
#pragma once

#include <stdbool.h>
#include "room_data.h"

// One command from the twin; only the has_* fields that are set apply
typedef struct {
    bool has_mode;
    room_mode_t mode;
    bool has_light;
    bool light_on;
    bool has_fan;
    bool fan_on;
    bool light_auto;     // {"light":"auto"}: end the lights override (auto mode)
    bool fan_auto;       // {"fan":"auto"}: end the fan override (auto mode)
    bool has_set;
    float set_c;
    bool reset_energy;
} room_cmd_t;

void control_start(void);

// Never blocks; returns false if the command queue is full
bool control_send_cmd(const room_cmd_t *cmd);

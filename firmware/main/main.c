// Smart Room firmware: occupancy, door, temperature and daylight control of
// the lights and the fan ("AC"), with energy tracking and an MQTT link to the
// digital twin.
//
// Tasks (higher number = higher priority):
//   control  6  room rules and outputs, every 100 ms
//   sensors  5  PIR, ultrasonic, LM35, light, knob, every 200 ms
//   power    4  SEN0291 wattmeter, every 500 ms
//   publish  3  telemetry, state, faults and events to MQTT and serial
// Wi-Fi and MQTT run in their own ESP-IDF tasks and reconnect on their own.
// Control never waits for a sensor or the network.

#include "esp_log.h"
#include "room_data.h"
#include "sensors.h"
#include "control.h"
#include "net.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Smart Room starting. The PIR may trigger on its own for up to a minute while it settles.");
    room_data_init();
    sensors_start();
    net_start();
    control_start();
}

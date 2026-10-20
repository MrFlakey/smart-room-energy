#include "room_data.h"

#include "freertos/FreeRTOS.h"

const char *const SENSOR_KEYS[SENSOR_COUNT] = {
    [SENSOR_PIR] = "pir",
    [SENSOR_ULTRASONIC] = "hcsr04",   // key from the MQTT contract
    [SENSOR_LM35] = "lm35",
    [SENSOR_LIGHT] = "light",
    [SENSOR_ROTATION] = "rotation",
    [SENSOR_SEN0291] = "sen0291",
};

// The structs are small, so a spinlock around a copy is enough
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static sensors_t s_sensors;
static room_t s_room;

void room_data_init(void)
{
    s_sensors.since_motion_s = -1.0f;
    s_room.mode = MODE_AUTO;
}

void sensors_get(sensors_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    *out = s_sensors;
    taskEXIT_CRITICAL(&s_lock);
}

void sensors_put(const sensors_t *in)
{
    taskENTER_CRITICAL(&s_lock);
    // The wattmeter fields belong to the power task
    bool p_failed = s_sensors.failed[SENSOR_SEN0291];
    const char *p_reason = s_sensors.reason[SENSOR_SEN0291];
    float fan_w = s_sensors.fan_w;
    s_sensors = *in;
    s_sensors.failed[SENSOR_SEN0291] = p_failed;
    s_sensors.reason[SENSOR_SEN0291] = p_reason;
    s_sensors.fan_w = fan_w;
    taskEXIT_CRITICAL(&s_lock);
}

void sensors_put_power(bool failed, const char *reason, float fan_w)
{
    taskENTER_CRITICAL(&s_lock);
    s_sensors.failed[SENSOR_SEN0291] = failed;
    s_sensors.reason[SENSOR_SEN0291] = reason;
    s_sensors.fan_w = fan_w;
    taskEXIT_CRITICAL(&s_lock);
}

void room_get(room_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    *out = s_room;
    taskEXIT_CRITICAL(&s_lock);
}

void room_put(const room_t *in)
{
    taskENTER_CRITICAL(&s_lock);
    s_room = *in;
    taskEXIT_CRITICAL(&s_lock);
}

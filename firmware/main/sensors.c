#include "sensors.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "app_config.h"
#include "board_pins.h"
#include "room_data.h"
#include "pir.h"
#include "lm35.h"
#include "light_sensor.h"
#include "rotation_sensor.h"
#include "sen0291.h"
#include "sen0307.h"

static const char *TAG = "sensors";

// Counts good and bad readings so one glitch doesn't flip a sensor's state
typedef struct {
    int bad;
    int good;
    bool failed;
    const char *reason;
} health_t;

static void health_fail_now(health_t *h, sensor_id_t id, const char *why)
{
    if (!h->failed) {
        ESP_LOGW(TAG, "%s failed: %s", SENSOR_KEYS[id], why);
    }
    h->failed = true;
    h->reason = why;
    h->bad = 0;
    h->good = 0;
}

static void health_update(health_t *h, sensor_id_t id, bool ok, const char *why,
                          int fail_after, int recover_after)
{
    if (ok) {
        h->bad = 0;
        if (h->failed && ++h->good >= recover_after) {
            h->failed = false;
            h->reason = NULL;
            ESP_LOGI(TAG, "%s recovered", SENSOR_KEYS[id]);
        }
    } else {
        h->good = 0;
        if (h->failed) {
            h->reason = why;
        } else if (++h->bad >= fail_after) {
            health_fail_now(h, id, why);
        }
    }
}

// Light and knob: a disconnected analog pin floats and jumps around, which a
// real sensor doesn't do several times within a few seconds.
typedef struct {
    float last;
    bool has_last;
    bool jumps[ANALOG_JUMP_WINDOW];
    int pos;
} jump_watch_t;

static bool jump_watch_ok(jump_watch_t *w, float value)
{
    bool jump = w->has_last && fabsf(value - w->last) > ANALOG_JUMP_PCT;
    w->last = value;
    w->has_last = true;
    w->jumps[w->pos] = jump;
    w->pos = (w->pos + 1) % ANALOG_JUMP_WINDOW;
    int count = 0;
    for (int i = 0; i < ANALOG_JUMP_WINDOW; i++) {
        count += w->jumps[i];
    }
    return count < ANALOG_JUMPS_FOR_FAULT;
}

static pir_handle_t s_pir;
static sen0307_handle_t s_us;
static lm35_handle_t s_lm35;
static light_sensor_handle_t s_light;
static rotation_sensor_handle_t s_knob;

static health_t s_health[SENSOR_COUNT];
static sensors_t s_data;   // the sensor task's working copy
static jump_watch_t s_light_jumps, s_knob_jumps;
static int s_door_count;   // readings in a row on the other side of the threshold
static float s_last_temp;
static bool s_has_temp;
static int64_t s_next_pir_probe_us;

// The PIR module drives its output both ways. If nothing is connected, the pin
// only sits low because of our pull-down. Switching briefly to a pull-up tells
// the two apart: a driven pin stays low, a loose one goes high.
static bool pir_pin_driven(void)
{
    gpio_intr_disable(PIN_PIR);
    gpio_set_pull_mode(PIN_PIR, GPIO_PULLUP_ONLY);
    esp_rom_delay_us(200);
    bool high = gpio_get_level(PIN_PIR);
    gpio_set_pull_mode(PIN_PIR, GPIO_PULLDOWN_ONLY);
    esp_rom_delay_us(200);
    gpio_intr_enable(PIN_PIR);
    return !high;
}

static void read_pir(void)
{
    health_t *h = &s_health[SENSOR_PIR];
    if (!s_pir) {
        return;
    }
    s_data.pir = pir_is_motion(s_pir);
    s_data.since_motion_s = pir_seconds_since_motion(s_pir);
#if PIR_PROBE_ENABLE
    int64_t now = esp_timer_get_time();
    if (now >= s_next_pir_probe_us) {
        s_next_pir_probe_us = now + PIR_PROBE_EVERY_MS * 1000LL;
        bool ok = s_data.pir || pir_pin_driven();
        health_update(h, SENSOR_PIR, ok, "not connected", 3, 3);
    }
#else
    (void)h;
#endif
}

// The SEN0307 sits on a pin with internal pulls, so every reading is followed
// by a check that the sensor still drives the pin. Without that, an unplugged
// sensor would just read as some random distance.
static void read_ultrasonic(void)
{
    health_t *h = &s_health[SENSOR_ULTRASONIC];
    if (!s_us) {
        return;
    }
    float cm = 0;
    const char *why = NULL;
    bool connected = true;   // stays true on a pin that can't be checked
    if (sen0307_read_cm(s_us, &cm) != ESP_OK) {
        why = "no reading";
    } else if (sen0307_check_connected(s_us, &connected) == ESP_OK && !connected) {
        why = "not connected";
    }
    health_update(h, SENSOR_ULTRASONIC, why == NULL, why, FAULT_AFTER_BAD_READS, RECOVER_AFTER_GOOD_READS);
    if (h->failed) {
        s_data.door_open = false;
        s_door_count = 0;
        return;
    }
    if (why != NULL) {
        return;
    }
    s_data.dist_cm = cm;
    bool open_now = cm > DOOR_OPEN_CM;
    if (open_now != s_data.door_open) {
        if (++s_door_count >= DOOR_DEBOUNCE_READS) {
            s_data.door_open = open_now;
            s_door_count = 0;
        }
    } else {
        s_door_count = 0;
    }
}

static void read_temperature(void)
{
    health_t *h = &s_health[SENSOR_LM35];
    if (!s_lm35) {
        return;
    }
    float c = 0;
    bool read_ok = lm35_read_celsius(s_lm35, &c) == ESP_OK;
    const char *why = NULL;
    if (!read_ok) {
        why = "no reading";
    } else if (c < LM35_MIN_C || c > LM35_MAX_C) {
        why = "out of range";
    } else if (s_has_temp && fabsf(c - s_last_temp) > LM35_MAX_JUMP_C) {
        why = "unstable";
    }
    if (read_ok) {
        // Implausible values count too, so a steady value isn't seen as a
        // jump once it settles
        s_last_temp = c;
        s_has_temp = true;
    }
    health_update(h, SENSOR_LM35, why == NULL, why, FAULT_AFTER_BAD_READS, RECOVER_AFTER_GOOD_READS);
    if (why == NULL) {
        s_data.temp_c = c;
    }
}

static void read_analog_pct(sensor_id_t id, esp_err_t err, float pct, jump_watch_t *w, float *out)
{
    const char *why = NULL;
    if (err != ESP_OK) {
        why = "no reading";
    } else if (!jump_watch_ok(w, pct)) {
        why = "unstable";
    }
    health_update(&s_health[id], id, why == NULL, why, FAULT_AFTER_BAD_READS, RECOVER_AFTER_GOOD_READS);
    if (why == NULL) {
        *out = pct;
    }
}

static void poll_once(void)
{
    read_pir();
    read_ultrasonic();
    read_temperature();

    float pct = 0;
    if (s_light) {
        esp_err_t err = light_sensor_read_percent(s_light, &pct);
        read_analog_pct(SENSOR_LIGHT, err, pct, &s_light_jumps, &s_data.light_pct);
    }
    if (s_knob) {
        esp_err_t err = rotation_sensor_read_percent(s_knob, &pct);
        read_analog_pct(SENSOR_ROTATION, err, pct, &s_knob_jumps, &s_data.knob_pct);
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i == SENSOR_SEN0291) {
            continue;
        }
        s_data.failed[i] = s_health[i].failed;
        s_data.reason[i] = s_health[i].reason;
    }
    sensors_put(&s_data);
}

static void sensor_task(void *arg)
{
    TickType_t wake = xTaskGetTickCount();
    int count = 0;
    for (;;) {
        poll_once();
        // Raw voltages for calibrating the light and knob ranges in app_config.h
        if (++count >= TELEMETRY_PERIOD_MS / SENSOR_PERIOD_MS) {
            count = 0;
            int light_mv = -1, knob_mv = -1;
            if (s_light) {
                light_sensor_read_mv(s_light, &light_mv);
            }
            if (s_knob) {
                rotation_sensor_read_mv(s_knob, &knob_mv);
            }
            printf("sensors: light %d mV, knob %d mV\n", light_mv, knob_mv);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

static void power_task(void *arg)
{
    sen0291_handle_t dev = NULL;
    health_t h = { 0 };
    int64_t next_try_us = 0;
    float fan_w = 0;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        int64_t now = esp_timer_get_time();
        if (!dev && now >= next_try_us) {
            sen0291_config_t cfg = SEN0291_DEFAULT_CONFIG(PIN_I2C_SDA, PIN_I2C_SCL);
            cfg.address = SEN0291_ADDR;
            if (sen0291_create(&cfg, &dev) != ESP_OK) {
                dev = NULL;
                next_try_us = now + POWER_RETRY_MS * 1000LL;
                health_fail_now(&h, SENSOR_SEN0291, "not found");
            }
        }
        if (dev) {
            sen0291_reading_t r;
            bool ok = sen0291_read(dev, &r) == ESP_OK;
            health_update(&h, SENSOR_SEN0291, ok, "no reading", FAULT_AFTER_BAD_READS, RECOVER_AFTER_GOOD_READS);
            if (ok) {
                // Negative only if IN+ and IN- are swapped; the power is the same
                fan_w = fabsf(r.power_mw) / 1000.0f;
            } else if (h.failed) {
                // Start over, in case it was unplugged and comes back
                sen0291_delete(dev);
                dev = NULL;
                next_try_us = now + POWER_RETRY_MS * 1000LL;
            }
        }
        sensors_put_power(h.failed, h.reason, fan_w);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(POWER_PERIOD_MS));
    }
}

static void init_failed(sensor_id_t id, esp_err_t err)
{
    ESP_LOGE(TAG, "%s init failed: %s", SENSOR_KEYS[id], esp_err_to_name(err));
    health_fail_now(&s_health[id], id, "init failed");
}

void sensors_start(void)
{
    esp_err_t err;

    pir_config_t pir_cfg = PIR_DEFAULT_CONFIG(PIN_PIR);
    if ((err = pir_create(&pir_cfg, &s_pir)) != ESP_OK) {
        s_pir = NULL;
        init_failed(SENSOR_PIR, err);
    }

    sen0307_config_t us_cfg = SEN0307_DEFAULT_CONFIG(PIN_ULTRASONIC);
    if ((err = sen0307_create(&us_cfg, &s_us)) != ESP_OK) {
        s_us = NULL;
        init_failed(SENSOR_ULTRASONIC, err);
    } else {
        bool connected;
        if (sen0307_check_connected(s_us, &connected) == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "GPIO%d has no internal pulls, so an unplugged ultrasonic can't be detected",
                     PIN_ULTRASONIC);
        }
    }

    lm35_config_t lm35_cfg = LM35_DEFAULT_CONFIG(PIN_LM35);
    if ((err = lm35_create(&lm35_cfg, &s_lm35)) != ESP_OK) {
        s_lm35 = NULL;
        init_failed(SENSOR_LM35, err);
    }

    light_sensor_config_t light_cfg = LIGHT_SENSOR_DEFAULT_CONFIG(PIN_LIGHT);
    light_cfg.min_mv = LIGHT_DARK_MV;
    light_cfg.max_mv = LIGHT_BRIGHT_MV;
    if ((err = light_sensor_create(&light_cfg, &s_light)) != ESP_OK) {
        s_light = NULL;
        init_failed(SENSOR_LIGHT, err);
    }

    rotation_sensor_config_t knob_cfg = ROTATION_SENSOR_DEFAULT_CONFIG(PIN_ROTATION);
    knob_cfg.min_mv = KNOB_MIN_MV;
    knob_cfg.max_mv = KNOB_MAX_MV;
    if ((err = rotation_sensor_create(&knob_cfg, &s_knob)) != ESP_OK) {
        s_knob = NULL;
        init_failed(SENSOR_ROTATION, err);
    }

    // One pass now so the control task starts from real readings
    s_data.since_motion_s = -1.0f;
    poll_once();

    xTaskCreate(sensor_task, "sensors", 4096, NULL, 5, NULL);
    xTaskCreate(power_task, "power", 4096, NULL, 4, NULL);
}

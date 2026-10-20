#include "control.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "app_config.h"
#include "board_pins.h"
#include "net.h"
#include "button.h"
#include "fan.h"
#include "led.h"

static const char *TAG = "control";

static QueueHandle_t s_cmds;
static led_handle_t s_led1, s_led2;
static fan_handle_t s_fan;
static button_handle_t s_btn[3];   // B1 lights, B2 fan, B3 mode

// Everything the control task remembers between ticks
static struct {
    room_t r;                // what gets published
    bool pir_ok;             // this tick's PIR health
    bool lights_ovr_on;      // override target while lights_ovr is set
    bool ac_ovr_on;
    bool manual_light_on;
    bool manual_fan_on;
    bool fan_running;        // auto fan hysteresis
    float light_level;       // smoothed auto light level
    float knob_ref_c;        // knob reading that last changed the setpoint
    bool knob_ref_valid;
    int64_t door_open_since_us;
    bool pir_recheck;        // PIR just recovered: end overrides if the room stays empty
    int64_t pir_recovered_us;
    bool prev_failed[SENSOR_COUNT];
    double baseline_wh;      // what an always-on room would have used
    float fan_full_w;        // learned fan power at 100 %
    int64_t fan_full_since_us;
    int out_light, out_fan;  // levels last written to the hardware
} c;

bool control_send_cmd(const room_cmd_t *cmd)
{
    return s_cmds && xQueueSend(s_cmds, cmd, 0) == pdTRUE;
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static float knob_to_c(float pct)
{
    return SET_MIN_C + clampf(pct, 0, 100) / 100.0f * (SET_MAX_C - SET_MIN_C);
}

static float round_half(float v)
{
    return roundf(v * 2.0f) / 2.0f;
}

static void post_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void post_event(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    net_post_event(buf, false);
}

static void end_overrides(const char *why)
{
    if (c.r.lights_ovr || c.r.ac_ovr) {
        ESP_LOGI(TAG, "overrides end: %s", why);
    }
    c.r.lights_ovr = false;
    c.r.ac_ovr = false;
}

static void set_mode(room_mode_t mode)
{
    if (mode == c.r.mode) {
        return;
    }
    c.r.mode = mode;
    end_overrides("mode changed");
    c.pir_recheck = false;
    if (mode == MODE_MANUAL) {
        // Manual starts from whatever is on now
        c.manual_light_on = c.r.light > 0;
        c.manual_fan_on = c.r.fan > 0;
    }
    ESP_LOGI(TAG, "mode %s", mode == MODE_AUTO ? "auto" : "manual");
}

static bool lights_wanted_on(void)
{
    if (c.r.mode == MODE_MANUAL) {
        return c.manual_light_on;
    }
    return c.r.lights_ovr ? c.lights_ovr_on : c.r.light > 0;
}

static bool fan_wanted_on(void)
{
    if (c.r.mode == MODE_MANUAL) {
        return c.manual_fan_on;
    }
    // While the door pauses it, the fan still counts as on if it would run
    return c.r.ac_ovr ? c.ac_ovr_on : (c.r.fan > 0 || c.fan_running);
}

// B1 or {"light":..}. In auto it becomes an override; with the PIR failed,
// switching off ends the override instead (the lights are off then anyway).
static void request_light(bool on)
{
    if (c.r.mode == MODE_MANUAL) {
        c.manual_light_on = on;
    } else if (!c.pir_ok && !on) {
        c.r.lights_ovr = false;
    } else {
        c.r.lights_ovr = true;
        c.lights_ovr_on = on;
    }
}

static void request_fan(bool on)
{
    if (c.r.mode == MODE_MANUAL) {
        c.manual_fan_on = on;
    } else if (!c.pir_ok && !on) {
        c.r.ac_ovr = false;
    } else {
        c.r.ac_ovr = true;
        c.ac_ovr_on = on;
    }
}

static void handle_cmd(const room_cmd_t *cmd)
{
    if (cmd->has_mode) {
        set_mode(cmd->mode);
    }
    if (cmd->has_light) {
        request_light(cmd->light_on);
    }
    if (cmd->has_fan) {
        request_fan(cmd->fan_on);
    }
    // "auto" hands an overridden device back to auto control; ignored otherwise
    if (cmd->light_auto && c.r.mode == MODE_AUTO) {
        c.r.lights_ovr = false;
    }
    if (cmd->fan_auto && c.r.mode == MODE_AUTO) {
        c.r.ac_ovr = false;
    }
    if (cmd->has_set) {
        c.r.set_c = roundf(clampf(cmd->set_c, SET_MIN_C, SET_MAX_C) * 10.0f) / 10.0f;
    }
    if (cmd->reset_energy) {
        c.r.energy_wh = 0;
        c.baseline_wh = 0;
        ESP_LOGI(TAG, "energy reset");
    }
}

static void handle_buttons(void)
{
    for (int i = 0; i < 3; i++) {
        if (!s_btn[i] || !button_was_pressed(s_btn[i])) {
            continue;
        }
        post_event("{\"type\":\"button\",\"id\":%d}", i + 1);
        switch (i) {
        // In auto, a press while that device is overridden ends the override
        case 0:
            if (c.r.mode == MODE_AUTO && c.r.lights_ovr) {
                c.r.lights_ovr = false;
            } else {
                request_light(!lights_wanted_on());
            }
            break;
        case 1:
            if (c.r.mode == MODE_AUTO && c.r.ac_ovr) {
                c.r.ac_ovr = false;
            } else {
                request_fan(!fan_wanted_on());
            }
            break;
        case 2: set_mode(c.r.mode == MODE_AUTO ? MODE_MANUAL : MODE_AUTO); break;
        }
    }
}

// What a sensor fault switches off in auto mode, as the twin expects it
static void post_fault(sensor_id_t id)
{
    bool lights = false, fan = false;
    if (c.r.mode == MODE_AUTO) {
        lights = (id == SENSOR_LIGHT || id == SENSOR_PIR) && !c.r.lights_ovr;
        fan = (id == SENSOR_LM35 || id == SENSOR_PIR) && !c.r.ac_ovr;
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"type\":\"fault\",\"sensor\":\"%s\",\"off\":[%s%s%s]}",
             SENSOR_KEYS[id], lights ? "\"lights\"" : "", lights && fan ? "," : "", fan ? "\"fan\"" : "");
    net_post_event(buf, true);
}

static void check_faults(const sensors_t *s, int64_t now)
{
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (s->failed[i] == c.prev_failed[i]) {
            continue;
        }
        c.prev_failed[i] = s->failed[i];
        if (s->failed[i]) {
            post_fault(i);
        } else {
            char buf[80];
            snprintf(buf, sizeof(buf), "{\"type\":\"recovered\",\"sensor\":\"%s\"}", SENSOR_KEYS[i]);
            net_post_event(buf, true);
            if (i == SENSOR_PIR) {
                c.pir_recheck = true;
                c.pir_recovered_us = now;
            }
        }
    }
}

static void update_occupancy(const sensors_t *s, int64_t now)
{
    bool occ = c.pir_ok && s->since_motion_s >= 0 && s->since_motion_s < EMPTY_AFTER_S;
    if (occ != c.r.occ) {
        c.r.occ = occ;
        // A failing PIR is reported as a fault, not as the room emptying
        if (c.pir_ok) {
            post_event("{\"type\":\"occupancy\",\"value\":%d}", occ);
            if (!occ) {
                end_overrides("room empty");
            }
        }
    }
    // Overrides made while the PIR was failed follow the normal rule again once
    // it is back: they end when the room is (or becomes) empty
    if (c.pir_recheck && c.pir_ok) {
        if (occ) {
            c.pir_recheck = false;   // the normal "room becomes empty" rule takes over
        } else if (now - c.pir_recovered_us >= (int64_t)(EMPTY_AFTER_S * 1e6f)) {
            end_overrides("room empty after PIR recovered");
            c.pir_recheck = false;
        }
    }
}

static void update_door(const sensors_t *s, int64_t now)
{
    if (s->door_open != c.r.door_open) {
        c.r.door_open = s->door_open;
        c.door_open_since_us = now;
        // A failing sensor closes the door quietly; the fault event says why
        if (!s->failed[SENSOR_ULTRASONIC]) {
            post_event("{\"type\":\"door\",\"value\":%d}", c.r.door_open);
        }
    }
}

static void update_setpoint(const sensors_t *s)
{
    if (s->failed[SENSOR_ROTATION]) {
        c.knob_ref_valid = false;   // take a new reference once it's back
        return;
    }
    float knob = knob_to_c(s->knob_pct);
    if (!c.knob_ref_valid) {
        c.knob_ref_c = knob;
        c.knob_ref_valid = true;
    } else if (fabsf(knob - c.knob_ref_c) > KNOB_DEADBAND_C) {
        c.knob_ref_c = knob;
        c.r.set_c = round_half(knob);
    }
}

static int daylight_level(float ambient_pct)
{
    float level;
    if (ambient_pct <= LIGHT_FULL_BELOW_PCT) {
        level = 100;
    } else if (ambient_pct >= LIGHT_OFF_ABOVE_PCT) {
        level = 0;
    } else {
        level = 100.0f * (LIGHT_OFF_ABOVE_PCT - ambient_pct) / (LIGHT_OFF_ABOVE_PCT - LIGHT_FULL_BELOW_PCT);
    }
    int stepped = (int)lroundf(level / LIGHT_STEP_PCT) * LIGHT_STEP_PCT;
    return stepped < LIGHT_MIN_ON_PCT ? 0 : stepped;
}

static void update_lights(const sensors_t *s, float dt_s)
{
    float target;
    bool smooth = false;
    if (c.r.mode == MODE_MANUAL) {
        target = c.manual_light_on ? 100 : 0;
    } else if (c.r.lights_ovr) {
        target = c.lights_ovr_on ? 100 : 0;
    } else if (!c.pir_ok || s->failed[SENSOR_LIGHT] || !c.r.occ) {
        target = 0;
    } else {
        target = daylight_level(s->light_pct);
        smooth = c.light_level > 0;   // switch on at once, then dim gently
    }

    if (smooth) {
        float step = LIGHT_RAMP_PCT_PER_S * dt_s;
        float diff = target - c.light_level;
        c.light_level += clampf(diff, -step, step);
    } else {
        c.light_level = target;
    }
    c.r.light = (int)lroundf(c.light_level);
}

static void update_fan(const sensors_t *s, int64_t now)
{
    int target;
    // An open door pauses the AC in auto mode, unless the fan is overridden
    c.r.ac_paused = c.r.mode == MODE_AUTO && !c.r.ac_ovr && c.r.door_open &&
                    now - c.door_open_since_us >= (int64_t)(DOOR_PAUSE_S * 1e6f);
    if (c.r.mode == MODE_MANUAL) {
        target = c.manual_fan_on ? 100 : 0;
    } else {
        if (c.r.ac_ovr) {
            target = c.ac_ovr_on ? 100 : 0;
        } else if (!c.pir_ok || s->failed[SENSOR_LM35] || !c.r.occ) {
            c.fan_running = false;
            target = 0;
        } else {
            float over = s->temp_c - c.r.set_c;
            if (!c.fan_running && over >= FAN_ON_ABOVE_C) {
                c.fan_running = true;
            } else if (c.fan_running && over <= FAN_OFF_BELOW_C) {
                c.fan_running = false;
            }
            target = 0;
            if (c.fan_running) {
                float pct = FAN_MIN_PCT + (100 - FAN_MIN_PCT) * over / FAN_FULL_AT_C;
                target = (int)lroundf(clampf(pct, FAN_MIN_PCT, 100) / 5.0f) * 5;
            }
        }
        if (c.r.ac_paused) {
            target = 0;
        }
    }
    c.r.fan = target;
}

static void update_energy(const sensors_t *s, float dt_s, int64_t now)
{
    bool metered = !s->failed[SENSOR_SEN0291];
    float led_w = 2 * LED_FULL_W * c.r.light / 100.0f;
    float fan_w;

    // Learn what the fan really draws at full speed once it has spun up
    if (c.out_fan == 100) {
        if (c.fan_full_since_us == 0) {
            c.fan_full_since_us = now;
        } else if (metered && now - c.fan_full_since_us > 3000000 && s->fan_w > 0.05f) {
            c.fan_full_w = 0.9f * c.fan_full_w + 0.1f * s->fan_w;
        }
    } else {
        c.fan_full_since_us = 0;
    }

    if (metered) {
        fan_w = s->fan_w;
        c.r.power_w = led_w + fan_w;
    } else {
        // Keep counting energy with an estimate; power_w is reported as null
        fan_w = c.fan_full_w * c.out_fan / 100.0f;
        c.r.power_w = NAN;
    }

    double dt_h = dt_s / 3600.0;
    c.r.energy_wh += (led_w + fan_w) * dt_h;
    c.baseline_wh += (2 * LED_FULL_W + c.fan_full_w) * dt_h;
    c.r.saved_pct = c.baseline_wh > 0 ? clampf(100.0f * (1.0f - c.r.energy_wh / c.baseline_wh), 0, 100) : 0;
}

static void apply_outputs(void)
{
    if (c.r.light != c.out_light) {
        uint8_t level = (uint8_t)(c.r.light * 255 / 100);
        if (s_led1) {
            led_set_brightness(s_led1, level);
        }
        if (s_led2) {
            led_set_brightness(s_led2, level);
        }
        c.out_light = c.r.light;
    }
    if (c.r.fan != c.out_fan) {
        if (s_fan) {
            fan_set_percent(s_fan, (uint8_t)c.r.fan);
        }
        c.out_fan = c.r.fan;
    }
}

static void control_task(void *arg)
{
    esp_task_wdt_add(NULL);
    TickType_t wake = xTaskGetTickCount();
    int64_t last_us = esp_timer_get_time();

    for (;;) {
        esp_task_wdt_reset();
        int64_t now = esp_timer_get_time();
        float dt_s = (now - last_us) / 1e6f;
        last_us = now;

        sensors_t s;
        sensors_get(&s);
        c.pir_ok = !s.failed[SENSOR_PIR];

        check_faults(&s, now);
        update_occupancy(&s, now);
        update_door(&s, now);
        update_setpoint(&s);
        handle_buttons();
        room_cmd_t cmd;
        while (xQueueReceive(s_cmds, &cmd, 0) == pdTRUE) {
            handle_cmd(&cmd);
        }
        update_lights(&s, dt_s);
        update_fan(&s, now);
        apply_outputs();
        update_energy(&s, dt_s, now);
        room_put(&c.r);

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

void control_start(void)
{
    s_cmds = xQueueCreate(8, sizeof(room_cmd_t));

    // A missing output is logged and skipped; the rest of the room keeps working
    led_config_t led1_cfg = LED_DEFAULT_CONFIG(PIN_LED1, LEDC_CHANNEL_0);
    if (led_create(&led1_cfg, &s_led1) != ESP_OK) {
        ESP_LOGE(TAG, "LED1 init failed");
        s_led1 = NULL;
    }
    led_config_t led2_cfg = LED_DEFAULT_CONFIG(PIN_LED2, LEDC_CHANNEL_1);
    if (led_create(&led2_cfg, &s_led2) != ESP_OK) {
        ESP_LOGE(TAG, "LED2 init failed");
        s_led2 = NULL;
    }
    fan_config_t fan_cfg = FAN_DEFAULT_CONFIG(PIN_FAN);
    if (fan_create(&fan_cfg, &s_fan) != ESP_OK) {
        ESP_LOGE(TAG, "fan init failed");
        s_fan = NULL;
    }
    const int btn_pins[3] = { PIN_BTN1, PIN_BTN2, PIN_BTN3 };
    for (int i = 0; i < 3; i++) {
        button_config_t cfg = BUTTON_DEFAULT_CONFIG(btn_pins[i]);
        if (button_create(&cfg, &s_btn[i]) != ESP_OK) {
            ESP_LOGE(TAG, "B%d init failed", i + 1);
            s_btn[i] = NULL;
        }
    }

    // Start in auto, with the setpoint from the knob if it can be read
    sensors_t s;
    sensors_get(&s);
    c.r.mode = MODE_AUTO;
    c.r.set_c = s.failed[SENSOR_ROTATION] ? SET_DEFAULT_C : round_half(knob_to_c(s.knob_pct));
    c.r.power_w = NAN;
    c.fan_full_w = FAN_FULL_W_DEFAULT;
    room_put(&c.r);

    xTaskCreate(control_task, "control", 4096, NULL, 6, NULL);
}

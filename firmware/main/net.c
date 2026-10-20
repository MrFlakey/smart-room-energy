#include "net.h"

#include <inttypes.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "app_config.h"
#include "control.h"
#include "room_data.h"

static const char *TAG = "net";

#define TOPIC_BASE   "smartroom/" CONFIG_SMARTROOM_ROOM_ID "/"
#define EVENT_LEN    160
#define MAX_PENDING  8      // fault events kept while offline

typedef struct {
    char json[EVENT_LEN];
    bool keep;
} event_msg_t;

static QueueHandle_t s_events;
static esp_mqtt_client_handle_t s_mqtt;
static bool s_mqtt_started;
static atomic_bool s_connected;
static atomic_bool s_resync;       // publish state and faults again after a (re)connect
static TimerHandle_t s_wifi_retry;
static int s_wifi_retry_ms = 1000;

void net_post_event(const char *json, bool keep)
{
    if (!s_events) {
        return;
    }
    event_msg_t msg = { .keep = keep };
    strlcpy(msg.json, json, sizeof(msg.json));
    if (xQueueSend(s_events, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped %s", json);
    }
}

// ---- Commands from the twin ----

static bool json_on(const cJSON *item, bool *on)
{
    if (cJSON_IsNumber(item)) {
        *on = item->valuedouble > 0;
        return true;
    }
    if (cJSON_IsBool(item)) {
        *on = cJSON_IsTrue(item);
        return true;
    }
    return false;
}

static void handle_cmd_json(const char *data, int len)
{
    ESP_LOGI(TAG, "cmd %.*s", len, data);
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!cJSON_IsObject(root)) {
        ESP_LOGW(TAG, "cmd is not a JSON object, ignored");
        cJSON_Delete(root);
        return;
    }

    room_cmd_t cmd = { 0 };
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (cJSON_IsString(item)) {
        if (strcmp(item->valuestring, "auto") == 0) {
            cmd.has_mode = true;
            cmd.mode = MODE_AUTO;
        } else if (strcmp(item->valuestring, "manual") == 0) {
            cmd.has_mode = true;
            cmd.mode = MODE_MANUAL;
        }
    }
    item = cJSON_GetObjectItemCaseSensitive(root, "light");
    cmd.light_auto = cJSON_IsString(item) && strcmp(item->valuestring, "auto") == 0;
    cmd.has_light = json_on(item, &cmd.light_on);
    item = cJSON_GetObjectItemCaseSensitive(root, "fan");
    cmd.fan_auto = cJSON_IsString(item) && strcmp(item->valuestring, "auto") == 0;
    cmd.has_fan = json_on(item, &cmd.fan_on);
    item = cJSON_GetObjectItemCaseSensitive(root, "set_c");
    if (cJSON_IsNumber(item)) {
        cmd.has_set = true;
        cmd.set_c = (float)item->valuedouble;
    }
    bool reset = false;
    if (json_on(cJSON_GetObjectItemCaseSensitive(root, "reset_energy"), &reset)) {
        cmd.reset_energy = reset;
    }
    cJSON_Delete(root);

    if (cmd.has_mode || cmd.has_light || cmd.has_fan || cmd.light_auto || cmd.fan_auto ||
        cmd.has_set || cmd.reset_energy) {
        if (!control_send_cmd(&cmd)) {
            ESP_LOGW(TAG, "command queue full, cmd dropped");
        }
    } else {
        ESP_LOGW(TAG, "cmd has no known keys, ignored");
    }
}

// ---- MQTT ----

static void publish(const char *subtopic, const char *payload, int qos, bool retain)
{
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_BASE "%s", subtopic);
    if (esp_mqtt_client_publish(s_mqtt, topic, payload, 0, qos, retain) < 0) {
        ESP_LOGW(TAG, "publish to %s failed", topic);
    }
}

static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        publish("status", "online", 1, true);
        esp_mqtt_client_subscribe(s_mqtt, TOPIC_BASE "cmd", 1);
        atomic_store(&s_resync, true);
        atomic_store(&s_connected, true);
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (atomic_exchange(&s_connected, false)) {
            ESP_LOGW(TAG, "MQTT disconnected, retrying in the background");
        }
        break;
    case MQTT_EVENT_DATA:
        // Commands are small; anything split over several chunks is ignored
        if (e->current_data_offset == 0 && e->data_len == e->total_data_len &&
            e->topic_len == (int)strlen(TOPIC_BASE "cmd") &&
            memcmp(e->topic, TOPIC_BASE "cmd", e->topic_len) == 0) {
            handle_cmd_json(e->data, e->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

// ---- Wi-Fi ----

static void wifi_retry(TimerHandle_t timer)
{
    esp_wifi_connect();
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        ESP_LOGW(TAG, "Wi-Fi disconnected (reason %d), retry in %d s", d->reason, s_wifi_retry_ms / 1000);
        xTimerChangePeriod(s_wifi_retry, pdMS_TO_TICKS(s_wifi_retry_ms), 0);
        s_wifi_retry_ms = s_wifi_retry_ms * 2 > 30000 ? 30000 : s_wifi_retry_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip = data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP " IPSTR, IP2STR(&ip->ip_info.ip));
        s_wifi_retry_ms = 1000;
        if (s_mqtt && !s_mqtt_started) {
            // From here the MQTT client reconnects on its own
            s_mqtt_started = esp_mqtt_client_start(s_mqtt) == ESP_OK;
        }
    }
}

static bool wifi_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }

    if ((err = esp_netif_init()) != ESP_OK ||
        (err = esp_event_loop_create_default()) != ESP_OK) {
        ESP_LOGE(TAG, "network init failed: %s", esp_err_to_name(err));
        return false;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&init_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        return false;
    }
    s_wifi_retry = xTimerCreate("wifi_retry", pdMS_TO_TICKS(1000), pdFALSE, NULL, wifi_retry);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);

    wifi_config_t cfg = { 0 };
    strlcpy((char *)cfg.sta.ssid, CONFIG_SMARTROOM_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, CONFIG_SMARTROOM_WIFI_PASSWORD, sizeof(cfg.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if ((err = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void mqtt_create(void)
{
    static char client_id[40];
    uint8_t mac[6] = { 0 };
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(client_id, sizeof(client_id), "smartroom-%s-%02x%02x%02x",
             CONFIG_SMARTROOM_ROOM_ID, mac[3], mac[4], mac[5]);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_SMARTROOM_MQTT_URI,
        .credentials.client_id = client_id,
        .session.keepalive = 15,
        .session.last_will = {
            .topic = TOPIC_BASE "status",
            .msg = "offline",
            .qos = 1,
            .retain = 1,
        },
        .network.reconnect_timeout_ms = 5000,
        .network.timeout_ms = 5000,
    };
    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event, NULL);
}

// ---- Publishing ----

// Fixed-point number, or null when v is NaN
static const char *num(char *buf, size_t size, float v, int decimals)
{
    if (isnan(v)) {
        return "null";
    }
    long long scale = decimals == 1 ? 10 : decimals == 2 ? 100 : 1000;
    long long x = llroundf(v * scale);
    snprintf(buf, size, "%s%lld.%0*lld", x < 0 ? "-" : "", llabs(x) / scale, decimals, llabs(x) % scale);
    return buf;
}

static const char *mode_name(const room_t *r)
{
    return r->mode == MODE_AUTO ? "auto" : "manual";
}

static void build_telemetry(char *out, size_t size, const sensors_t *s, const room_t *r)
{
    char dist[16], temp[16], set[16], light[16], power[16], energy[24], saved[16];
    snprintf(out, size,
             "{\"ts\":%" PRId64 ",\"occ\":%d,\"pir\":%d,\"door_open\":%d,\"dist_cm\":%s,\"temp_c\":%s,"
             "\"set_c\":%s,\"light_pct\":%s,\"light\":%d,\"fan\":%d,\"power_w\":%s,\"energy_wh\":%s,"
             "\"saved_pct\":%s,\"mode\":\"%s\",\"lights_ovr\":%d,\"ac_ovr\":%d,\"ac_paused\":%d}",
             esp_timer_get_time() / 1000, r->occ, s->pir && !s->failed[SENSOR_PIR], r->door_open,
             num(dist, sizeof(dist), s->failed[SENSOR_ULTRASONIC] ? NAN : s->dist_cm, 1),
             num(temp, sizeof(temp), s->failed[SENSOR_LM35] ? NAN : s->temp_c, 1),
             num(set, sizeof(set), r->set_c, 1),
             num(light, sizeof(light), s->failed[SENSOR_LIGHT] ? NAN : s->light_pct, 1),
             r->light, r->fan,
             num(power, sizeof(power), r->power_w, 2),
             num(energy, sizeof(energy), (float)r->energy_wh, 3),
             num(saved, sizeof(saved), r->saved_pct, 1),
             mode_name(r), r->lights_ovr, r->ac_ovr, r->ac_paused);
}

static void build_state(char *out, size_t size, const room_t *r)
{
    char set[16];
    snprintf(out, size,
             "{\"mode\":\"%s\",\"occ\":%d,\"set_c\":%s,\"light\":%d,\"fan\":%d,\"lights_ovr\":%d,"
             "\"ac_ovr\":%d,\"ac_paused\":%d,\"door_open\":%d}",
             mode_name(r), r->occ, num(set, sizeof(set), r->set_c, 1), r->light, r->fan,
             r->lights_ovr, r->ac_ovr, r->ac_paused, r->door_open);
}

static void build_faults(char *out, size_t size, const sensors_t *s)
{
    size_t len = strlcpy(out, "{", size);
    bool first = true;
    for (int i = 0; i < SENSOR_COUNT && len < size; i++) {
        if (s->failed[i]) {
            len += snprintf(out + len, size - len, "%s\"%s\":\"%s\"", first ? "" : ",",
                            SENSOR_KEYS[i], s->reason[i] ? s->reason[i] : "failed");
            first = false;
        }
    }
    if (len < size) {
        strlcat(out, "}", size);
    }
}

static void publish_task(void *arg)
{
    static char last_state[256], last_faults[256], buf[512];
    static event_msg_t pending[MAX_PENDING];
    int pending_count = 0;
    int64_t next_telemetry_us = 0;
    int64_t last_state_us = 0;

    for (;;) {
        int64_t now = esp_timer_get_time();
        bool online = s_mqtt && atomic_load(&s_connected);
        bool resync = online && atomic_exchange(&s_resync, false);

        // Events. Faults that happened offline go out once we're back.
        if (online) {
            for (int i = 0; i < pending_count; i++) {
                publish("event", pending[i].json, 1, false);
            }
            pending_count = 0;
        }
        event_msg_t ev;
        while (xQueueReceive(s_events, &ev, 0) == pdTRUE) {
            printf("event %s\n", ev.json);
            if (online) {
                publish("event", ev.json, 1, false);
            } else if (ev.keep && pending_count < MAX_PENDING) {
                pending[pending_count++] = ev;
            }
        }

        sensors_t s;
        room_t r;
        sensors_get(&s);
        room_get(&r);

        build_faults(buf, sizeof(buf), &s);
        if (strcmp(buf, last_faults) != 0 || resync) {
            if (strcmp(buf, last_faults) != 0) {
                printf("faults %s\n", buf);
                strlcpy(last_faults, buf, sizeof(last_faults));
            }
            if (online) {
                publish("faults", buf, 1, true);
            }
        }

        build_state(buf, sizeof(buf), &r);
        bool state_changed = strcmp(buf, last_state) != 0;
        if ((state_changed && now - last_state_us >= STATE_MIN_INTERVAL_MS * 1000LL) || resync) {
            if (state_changed) {
                printf("state %s\n", buf);
                strlcpy(last_state, buf, sizeof(last_state));
            }
            last_state_us = now;
            if (online) {
                publish("state", buf, 1, true);
            }
        }

        if (now >= next_telemetry_us) {
            next_telemetry_us = now + TELEMETRY_PERIOD_MS * 1000LL;
            build_telemetry(buf, sizeof(buf), &s, &r);
            printf("telemetry %s\n", buf);
            if (online) {
                publish("telemetry", buf, 0, false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void net_start(void)
{
    s_events = xQueueCreate(16, sizeof(event_msg_t));
    xTaskCreate(publish_task, "publish", 6144, NULL, 3, NULL);

    if (strlen(CONFIG_SMARTROOM_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "No Wi-Fi SSID set (idf.py menuconfig > Smart Room). Running offline.");
        return;
    }
    mqtt_create();
    if (!wifi_start()) {
        ESP_LOGE(TAG, "Wi-Fi unavailable. Running offline.");
    }
}

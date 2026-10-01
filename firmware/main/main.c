// Test: PIR motion turns the fan on; 5 s without motion turns it off.

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board_pins.h"
#include "pir.h"
#include "fan.h"

static const char *TAG = "pir_fan_test";

#define FAN_OFF_DELAY_S 5.0f

void app_main(void)
{
    pir_config_t pir_cfg = PIR_DEFAULT_CONFIG(PIN_PIR);
    pir_handle_t pir;
    ESP_ERROR_CHECK(pir_create(&pir_cfg, &pir));

    fan_config_t fan_cfg = FAN_DEFAULT_CONFIG(PIN_FAN);
    fan_handle_t fan;
    ESP_ERROR_CHECK(fan_create(&fan_cfg, &fan));

    ESP_LOGI(TAG, "Ready. The PIR may trigger on its own for up to a minute while it settles.");

    bool was_motion = false;
    while (true) {
        bool motion = pir_is_motion(pir);
        if (motion != was_motion) {
            ESP_LOGI(TAG, "PIR: %s", motion ? "motion" : "no motion");
            was_motion = motion;
        }

        if (motion && !fan_is_on(fan)) {
            fan_on(fan);
            ESP_LOGI(TAG, "Fan ON");
        } else if (!motion && fan_is_on(fan) && pir_seconds_since_motion(pir) >= FAN_OFF_DELAY_S) {
            fan_off(fan);
            ESP_LOGI(TAG, "Fan OFF (no motion for %.0f s)", FAN_OFF_DELAY_S);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

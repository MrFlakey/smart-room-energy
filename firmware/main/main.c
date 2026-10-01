// Your application goes here. The drivers live in components/ and the pin map
// in board_pins.h; README.md shows how to use each driver.
//
// Quick start, for example:
//
//   #include "board_pins.h"
//   #include "lm35.h"
//   #include "fan.h"
//
//   lm35_config_t t_cfg = LM35_DEFAULT_CONFIG(PIN_LM35);
//   lm35_handle_t temp;
//   ESP_ERROR_CHECK(lm35_create(&t_cfg, &temp));
//
//   fan_config_t f_cfg = FAN_DEFAULT_CONFIG(PIN_FAN);
//   fan_handle_t fan;
//   ESP_ERROR_CHECK(fan_create(&f_cfg, &fan));
//
//   float c;
//   if (lm35_read_celsius(temp, &c) == ESP_OK && c > 28.0f) {
//       fan_set_percent(fan, 80);
//   }

#include "board_pins.h"

void app_main(void)
{
}

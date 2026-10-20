idf_component_register(SRCS "analog_in.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_adc
                       PRIV_REQUIRES esp_driver_gpio)

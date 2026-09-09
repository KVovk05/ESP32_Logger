idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES i2c_bus env_i2c env_spi rtc_ds1307 oled_display ctrl_adc led_ctrl wifi_prov mqtt_link json
)
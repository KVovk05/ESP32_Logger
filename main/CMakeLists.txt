# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES env_i2c i2c_bus log freertos env_spi rtc_ds1307 nvs_flash wifi_prov oled_display ctrl_adc led_ctrl mqtt_link
)

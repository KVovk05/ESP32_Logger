idf_component_register(
    SRCS "env_i2c.c"
    INCLUDE_DIRS "include"
    REQUIRES  driver bme280_compensate i2c_bus esp_timer log freertos
)
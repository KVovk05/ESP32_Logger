idf_component_register(
    SRCS "env_spi.c"
    INCLUDE_DIRS "include"
    REQUIRES driver bme280_compensate esp_timer log freertos
)
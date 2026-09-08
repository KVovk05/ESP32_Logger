idf_component_register(
    SRCS "rtc_ds1307.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c_bus log
)
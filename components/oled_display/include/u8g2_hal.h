#pragma once
#include "u8g2.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle);
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#ifdef __cplusplus
}
#endif
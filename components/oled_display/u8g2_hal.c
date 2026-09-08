#include "u8g2_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include <string.h>

static i2c_master_dev_handle_t s_dev_handle = NULL;
static uint8_t s_buffer[256];
static size_t s_buf_idx = 0;

void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle)
{
    s_dev_handle = dev_handle;
}

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER:
        s_buf_idx = 0;
        break;

    case U8X8_MSG_BYTE_SEND: {
        uint8_t *data = (uint8_t *)arg_ptr;
        if (s_buf_idx + arg_int <= sizeof(s_buffer)) {
            memcpy(&s_buffer[s_buf_idx], data, arg_int);
            s_buf_idx += arg_int;
        }
        break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER:
        if (s_dev_handle != NULL && s_buf_idx > 0) {
            if (i2c_master_transmit(s_dev_handle, s_buffer, s_buf_idx, 100) != ESP_OK) {
                return 0;
            }
        }
        s_buf_idx = 0;
        break;

    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
        break;

    default:
        return 0;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10);
        break;
    case U8X8_MSG_DELAY_100NANO:
        __asm__ __volatile__("nop; nop; nop; nop;");
        break;
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    case U8X8_MSG_GPIO_RESET:
        break;
    default:
        return 0;
    }
    return 1;
}
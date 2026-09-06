#include "env_i2c.h"
#include "bme280_compensate.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define BME280_ADDR    0x76     // SDO -> GND (Модуль 4.2, Додаток Б: BME280 = 0x76/0x77)
#define I2C_FREQ_HZ    100000

#define REG_CTRL_HUM   0xF2
#define REG_CTRL_MEAS  0xF4
#define REG_PRESS_MSB  0xF7

static const char *TAG = "ENV_I2C";
static i2c_master_dev_handle_t bme_handle;
static bme280_calib_t calib;

// WTR "у чистому вигляді" -- Модуль 4.3: один виклик, довільна довжина
static esp_err_t wtr(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(bme_handle, &reg, 1, out, len, 100);
}

void env_i2c_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BME280_ADDR,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &bme_handle));

    uint8_t chip_id = 0;
    ESP_ERROR_CHECK(wtr(0xD0, &chip_id, 1));
    ESP_LOGI(TAG, "I2C BME280 id=0x%02X (очікували 0x60)", chip_id);

    uint8_t buf_tp[26], buf_h[7];
    ESP_ERROR_CHECK(wtr(0x88, buf_tp, sizeof(buf_tp)));
    ESP_ERROR_CHECK(wtr(0xE1, buf_h, sizeof(buf_h)));
    bme280_parse_calib(&calib, buf_tp, buf_h);

    ESP_LOGI(TAG, "env_i2c готовий, адреса 0x%02X на спільній шині", BME280_ADDR);
}

bool env_i2c_read(float *temp_C, float *press_hPa, float *hum_RH)
{
    uint8_t ctrl_hum = 0x01;                                  // osrs_h = x1
    if (i2c_master_transmit(bme_handle, (uint8_t[]){REG_CTRL_HUM, ctrl_hum}, 2, 100) != ESP_OK) return false;
    uint8_t ctrl_meas = (1 << 5) | (1 << 2) | 0x01;            // osrs_t=x1, osrs_p=x1, mode=forced(01)
    if (i2c_master_transmit(bme_handle, (uint8_t[]){REG_CTRL_MEAS, ctrl_meas}, 2, 100) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));   // час на вимірювання -- forced mode (Модуль 4.4, Частина 3)

    uint8_t raw[8];
    if (wtr(REG_PRESS_MSB, raw, sizeof(raw)) != ESP_OK) return false;

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8)  |  (int32_t)raw[7];

    int32_t  T_int   = bme280_compensate_T(&calib, adc_T);   // ОБОВ'ЯЗКОВО першою -- рахує calib.t_fine
    uint32_t P_q24_8 = bme280_compensate_P(&calib, adc_P);
    uint32_t H_q22_10= bme280_compensate_H(&calib, adc_H);

    *temp_C    = T_int / 100.0f;
    *press_hPa = (P_q24_8 / 256.0f) / 100.0f;
    *hum_RH    = H_q22_10 / 1024.0f;
    return true;
}
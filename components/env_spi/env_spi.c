#include "env_spi.h"
#include "bme280_compensate.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Твої призначені піни
#define PIN_SCLK       18
#define PIN_MISO       17
#define PIN_MOSI       5
#define PIN_CS         16
#define SPI_HOST_USED  SPI3_HOST

#define REG_ID         0xD0
#define REG_CTRL_HUM   0xF2
#define REG_CTRL_MEAS  0xF4
#define REG_PRESS_MSB  0xF7

static const char *TAG = "ENV_SPI";
static spi_device_handle_t bme;
static bme280_calib_t calib;

// Спосіб А (комбінована транзакція) -- burst-read
static esp_err_t bme280_read_regs(uint8_t reg, uint8_t *out, size_t len)
{
    uint8_t tx[1 + len];
    uint8_t rx[1 + len];
    tx[0] = reg | 0x80;               // read-біт (біт 7 = 1)
    memset(&tx[1], 0x00, len);
    spi_transaction_t t = {
        .length = 8 * (1 + len),
        .tx_buffer = tx,
        .rx_buffer = rx
    };
    esp_err_t err = spi_device_polling_transmit(bme, &t);
    if (err == ESP_OK) memcpy(out, &rx[1], len);
    return err;
}

static esp_err_t bme280_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7F), value };   // write-біт (біт 7 = 0)
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx
    };
    return spi_device_polling_transmit(bme, &t);
}

void env_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_USED, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,   // 1 МГц
        .mode = 0,                            // CPOL=0, CPHA=0 (Mode 0)
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST_USED, &devcfg, &bme));

    uint8_t chip_id = 0;
    ESP_ERROR_CHECK(bme280_read_regs(REG_ID, &chip_id, 1));
    ESP_LOGI(TAG, "SPI BME280 id=0x%02X (Waited 0x60)", chip_id);

//    // Reset + затримка на перезавантаження NVM-регістрів
//    bme280_write_reg(0xE0, 0xB6);
//    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t buf_tp[26], buf_h[7];
    ESP_ERROR_CHECK(bme280_read_regs(0x88, buf_tp, sizeof(buf_tp)));
    ESP_ERROR_CHECK(bme280_read_regs(0xE1, buf_h, sizeof(buf_h)));
    bme280_parse_calib(&calib, buf_tp, buf_h);

    ESP_LOGI(TAG, "SPI ready: SCLK=%d MOSI=%d MISO=%d CS=%d", PIN_SCLK, PIN_MOSI, PIN_MISO, PIN_CS);
}

bool env_spi_read(float *temp_C, float *press_hPa, float *hum_RH)
{
    if (bme280_write_reg(REG_CTRL_HUM, 0x01) != ESP_OK) return false;                    // osrs_h = x1
    uint8_t ctrl_meas = (1 << 5) | (1 << 2) | 0x01;                                       // forced mode, osrs_t=x1, osrs_p=x1
    if (bme280_write_reg(REG_CTRL_MEAS, ctrl_meas) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t raw[8];
    if (bme280_read_regs(REG_PRESS_MSB, raw, sizeof(raw)) != ESP_OK) return false;
	
	ESP_LOGI("SPI_RAW", "raw[0..7]: %02X %02X %02X %02X %02X %02X %02X %02X | calib T1=%u", 
	             raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], calib.dig_T1);
    // Збірка сирих значень з регістрів 0xF7..0xFE
    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8)  |  (int32_t)raw[7];

    // Порядок критичний: T перша (рахує calib.t_fine для P і H)
    int32_t  T_int    = bme280_compensate_T(&calib, adc_T);
    uint32_t P_q24_8  = bme280_compensate_P(&calib, adc_P);
    uint32_t H_q22_10 = bme280_compensate_H(&calib, adc_H);

    *temp_C    = T_int / 100.0f;
    *press_hPa = (P_q24_8 / 256.0f) / 100.0f;
    *hum_RH    = H_q22_10 / 1024.0f;
    return true;
}
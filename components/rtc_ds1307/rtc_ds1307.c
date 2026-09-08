#include "rtc_ds1307.h"
#include "i2c_bus.h"
#include "esp_log.h"

#define DS1307_ADDR    0x68     // 7-бітна адреса (Модуль 4.2/4.3)
#define I2C_FREQ_HZ    100000

static const char *TAG = "RTC_DS1307";
static i2c_master_dev_handle_t s_handle;

static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

void rtc_ds1307_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = DS1307_ADDR,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &s_handle));
    ESP_LOGI(TAG, "Tiny RTC (DS1307) готовий, адреса 0x%02X", DS1307_ADDR);
}

bool rtc_ds1307_read_time(rtc_time_t *out)
{
    uint8_t reg = 0x00, raw[7];
    // burst-read з auto-increment -- один "знімок" усіх 7 полів часу за одну транзакцію (Модуль 4.3, Частина 3)
    if (i2c_master_transmit_receive(s_handle, &reg, 1, raw, 7, 100) != ESP_OK) return false;

    out->sec   = bcd_to_dec(raw[0] & 0x7F);   // біт 7 -- CH (Clock Halt), відкидаємо
    out->min   = bcd_to_dec(raw[1]);
    out->hour  = bcd_to_dec(raw[2] & 0x3F);   // 24-годинний формат
    out->wday  = raw[3];                       // НЕ BCD -- просте число 1-7 (Модуль 4.3, Частина 3)
    out->date  = bcd_to_dec(raw[4]);
    out->month = bcd_to_dec(raw[5]);
    out->year  = bcd_to_dec(raw[6]);
    return true;
}

void rtc_ds1307_set_from_tm(const struct tm *utc)
{
    uint8_t buf[8];
    buf[0] = 0x00;                                        // адреса першого регістра (Seconds)
    buf[1] = dec_to_bcd(utc->tm_sec) & 0x7F;               // біт 7 (CH) = 0 -- запускає годинник, Модуль 4.2
    buf[2] = dec_to_bcd(utc->tm_min);
    buf[3] = dec_to_bcd(utc->tm_hour) & 0x3F;              // 24-годинний формат
    buf[4] = (uint8_t)(utc->tm_wday + 1);                  // struct tm: 0=нд..6=сб -> DS1307: 1=нд..7=сб
    buf[5] = dec_to_bcd(utc->tm_mday);
    buf[6] = dec_to_bcd(utc->tm_mon + 1);                  // struct tm: місяці 0-11
    buf[7] = dec_to_bcd(utc->tm_year % 100);
    esp_err_t err = i2c_master_transmit(s_handle, buf, sizeof(buf), 100);
    ESP_LOGI(TAG, "DS1307 засіяно часом із SNTP: %s", err == ESP_OK ? "OK" : "ПОМИЛКА");
}
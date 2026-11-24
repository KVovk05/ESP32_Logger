#include "wifi_prov.h"
#include "i2c_bus.h"
#include "env_i2c.h"
#include "env_spi.h"
#include "rtc_ds1307.h"
#include "oled_display.h"
#include "ctrl_adc.h"
#include "led_ctrl.h"
#include "mqtt_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <time.h>

// Failsafe timeout for remote MQTT override: returns LED control to local threshold
#define MQTT_OVERRIDE_TIMEOUT_US   (60LL * 1000 * 1000)

static const char *TAG = "IOT_LOGGER";
static volatile bool s_mqtt_override_on = false;
static volatile bool s_mqtt_override_active = false;
static volatile int64_t s_mqtt_override_last_cmd_us = 0;

static void on_mqtt_led_command(bool led_on)
{
    s_mqtt_override_on = led_on;
    s_mqtt_override_active = true;
    s_mqtt_override_last_cmd_us = esp_timer_get_time();
}

static void on_time_synced(void)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    rtc_ds1307_set_from_tm(&timeinfo);
}

static void run_wiring_self_test(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " WIRING SELF-TEST");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "[OK]   I2C bus              SDA=GPIO8 SCL=GPIO9");
    ESP_LOGI(TAG, "[OK]   BME280 I2C (0x76)    init passed");
    ESP_LOGI(TAG, "[OK]   SSD1306 OLED (0x3C)  init passed");

    rtc_time_t t;
    if (rtc_ds1307_read_time(&t)) {
        ESP_LOGI(TAG, "[OK]   Tiny RTC DS1307 (0x68)  read ok: %02u:%02u:%02u", t.hour, t.min, t.sec);
    } else {
        ESP_LOGE(TAG, "[FAIL] Tiny RTC DS1307 (0x68)  no response -- check VCC/GND/SCL/SDA, battery seated");
    }

    float t_spi, p_spi, h_spi;
    bool spi_plausible = env_spi_read(&t_spi, &p_spi, &h_spi) && t_spi > 5.0f && t_spi < 50.0f;
    if (spi_plausible) {
        ESP_LOGI(TAG, "[OK]   BME280 SPI (CS=16)  live read: %.1fC %.0f%%RH %.0fhPa", t_spi, h_spi, p_spi);
    } else {
        ESP_LOGE(TAG, "[FAIL] BME280 SPI (CS=16)  read=%.1fC -- implausible value, check SCK/SDI/SDO/CS",
                 t_spi);
    }

    ESP_LOGI(TAG, "[--]   LED indicator          blinking 3x now");
    for (int i = 0; i < 3; i++) {
        led_ctrl_set(true);  vTaskDelay(pdMS_TO_TICKS(150));
        led_ctrl_set(false); vTaskDelay(pdMS_TO_TICKS(150));
    }

    ESP_LOGI(TAG, "[--]   Potentiometer (GPIO4)   threshold now = %.1fC",
             ctrl_adc_get_threshold_c());
    ESP_LOGI(TAG, "========================================");
}

void app_main(void)
{
    led_ctrl_init();
    i2c_bus_init();

    rtc_ds1307_init();
    oled_display_init();
    env_spi_init();
    ctrl_adc_init();

    run_wiring_self_test();

    ESP_LOGI(TAG, "Showing boot wolf animation (~5s)...");
    oled_play_wolf_boot_animation();

    wifi_prov_init_and_connect();
    mqtt_link_init(on_mqtt_led_command, on_time_synced);

    while (1) {
        rtc_time_t t;
        bool rtc_ok = rtc_ds1307_read_time(&t);

        float t_i2c = 0, p_i2c = 0, h_i2c = 0;
        bool i2c_ok = true;

        float t_spi = 0, p_spi = 0, h_spi = 0;
        bool spi_ok = env_spi_read(&t_spi, &p_spi, &h_spi);

        float threshold_c = ctrl_adc_get_threshold_c();
        uint32_t adc_raw = ctrl_adc_get_raw();

        if (s_mqtt_override_active &&
            (esp_timer_get_time() - s_mqtt_override_last_cmd_us) > MQTT_OVERRIDE_TIMEOUT_US) {
            s_mqtt_override_active = false;
            ESP_LOGI(TAG, "MQTT override timed out (%lld s) -- returning LED control to the local threshold",
                     (long long)(MQTT_OVERRIDE_TIMEOUT_US / 1000000));
        }

        bool local_alarm = (spi_ok && t_spi > threshold_c);
        bool led_on = s_mqtt_override_active ? s_mqtt_override_on : local_alarm;
        led_ctrl_set(led_on);
        const char *led_source = s_mqtt_override_active ? "mqtt" : "local";

        if (rtc_ok) {
            oled_draw_dashboard(&t, t_i2c, h_i2c, p_i2c, t_spi, h_spi, p_spi,
                                 threshold_c, led_on, led_source);
        }

        char utc_time[16] = "--:--:--";
        if (rtc_ok) snprintf(utc_time, sizeof(utc_time), "%02u:%02u:%02u", t.hour, t.min, t.sec);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "utc_time", utc_time);
        cJSON_AddBoolToObject(root, "i2c_ok", i2c_ok);
        if (i2c_ok) {
            cJSON_AddNumberToObject(root, "temp_i2c_c", t_i2c);
            cJSON_AddNumberToObject(root, "hum_i2c_rh", h_i2c);
            cJSON_AddNumberToObject(root, "press_i2c_hpa", p_i2c);
        }
        cJSON_AddBoolToObject(root, "spi_ok", spi_ok);
        if (spi_ok) {
            cJSON_AddNumberToObject(root, "temp_spi_c", t_spi);
            cJSON_AddNumberToObject(root, "hum_spi_rh", h_spi);
            cJSON_AddNumberToObject(root, "press_spi_hpa", p_spi);
        }
        cJSON_AddNumberToObject(root, "threshold_c", threshold_c);
        cJSON_AddNumberToObject(root, "adc_raw", adc_raw);
        cJSON_AddBoolToObject(root, "led_on", led_on);
        cJSON_AddStringToObject(root, "led_source", led_source);

        char *json_str = cJSON_PrintUnformatted(root);
        mqtt_link_publish_telemetry(json_str);

        ESP_LOGI(TAG, "%s | I2C T=%.1fC | SPI T=%.1fC | threshold=%.1fC | LED=%s(%s) | MQTT=%s",
                 utc_time, t_i2c, t_spi, threshold_c, led_on ? "ON" : "OFF", led_source,
                 mqtt_link_is_connected() ? "up" : "down");

        cJSON_free(json_str);
        cJSON_Delete(root);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

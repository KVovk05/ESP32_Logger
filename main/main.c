#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_link.h"
#include "i2c_bus.h"
#include "rtc_ds1307.h"
#include "oled_display.h"
#include "env_spi.h"
#include "ctrl_adc.h"
#include "wifi_prov.h"

static const char *TAG = "LOGGER_MAIN";

// 1. Callback керування світлодіодом (Завдання 8.3)
static void handle_led_command(bool on)
{
    ESP_LOGI(TAG, "Control command: %s -> LED %s", on ? "ON" : "OFF", on ? "ON" : "OFF");
    // Якщо підключений фізичний LED, можна перемикати рівень GPIO тут
}

// 2. Callback синхронізації часу
static void handle_time_synced()
{
    ESP_LOGI(TAG, "Time synchronized callback triggered: %ld");
}

void app_main(void)
{
    // 1. Ініціалізація NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Ініціалізація апаратної периферії
    i2c_bus_init();
    rtc_ds1307_init();
    oled_display_init();
    env_spi_init();
    ctrl_adc_init();

    // Анімація під час старту
    oled_play_wolf_boot_animation();

    // 3. Wi-Fi Provisioning та підключення
    ESP_LOGI(TAG, "Запуск Wi-Fi підключення / Provisioning...");
    wifi_prov_init_and_connect();
    ESP_LOGI(TAG, "Wi-Fi готовий!");

    // 4. Ініціалізація MQTT-клієнта
    mqtt_link_init(handle_led_command, handle_time_synced);

    uint32_t pub_counter = 0;

    // 5. Основний робочий цикл
    while (1) {
        rtc_time_t t;
        bool rtc_ok = rtc_ds1307_read_time(&t);

        float temp = 0, press = 0, hum = 0;
        bool spi_ok = env_spi_read(&temp, &press, &hum);

        float threshold_c = ctrl_adc_get_threshold_c();

        if (rtc_ok) {
            ESP_LOGI(TAG, "RTC: %02u:%02u:%02u", t.hour, t.min, t.sec);
        }

        if (spi_ok) {
            ESP_LOGI(TAG, "BME280: %.2f C | Lim: %.1f C", temp, threshold_c);
        } else {
            ESP_LOGI(TAG, "Limit ADC: %.1f C", threshold_c);
        }

        if (rtc_ok) {
            bool led_state = (temp > threshold_c);
            oled_draw_dashboard(&t, 0.0f, 0.0f, 0.0f, temp, hum, press, threshold_c, led_state, "local");
        }

        // Завдання 8.2: Публікація тестового рядка раз на 2 секунди (4 * 500 мс)
        if (++pub_counter >= 4) {
            mqtt_link_publish_telemetry("{\"test\":1}");
            pub_counter = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
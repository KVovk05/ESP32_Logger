#include "wifi_prov.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>

#define STA_CONNECT_TIMEOUT_MS   300000   // 5 хв запасного таймауту

static const char *TAG = "WIFI_PROV";
static EventGroupHandle_t s_event_group;
static const int CONNECTED_BIT = BIT0;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_PROV_EVENT) {
        switch (id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started -- connect your phone to this device's Wi-Fi AP");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
            ESP_LOGI(TAG, "Received credentials: SSID=%s", (const char *)cfg->ssid);
            break;
        }
        case WIFI_PROV_CRED_FAIL:
            ESP_LOGE(TAG, "Provisioning failed -- check SSID/password and retry in the app");
            break;
        case WIFI_PROV_END:
            wifi_prov_mgr_deinit();   // сервіс SoftAP більше не потрібен -- звільняємо ресурси
            break;
        default:
            break;
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_event_group, CONNECTED_BIT);
    }
}

void wifi_prov_init_and_connect(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    s_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    wifi_prov_mgr_config_t prov_cfg = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        char service_name[24];
        snprintf(service_name, sizeof(service_name), "PROV_iot_logger_%02X%02X", mac[4], mac[5]);

        ESP_LOGI(TAG, "Not provisioned -- starting SoftAP \"%s\"", service_name);
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "iot_logger_pop",
                                                          service_name, NULL));

        ESP_LOGI(TAG, "Waiting for IP...");
        xEventGroupWaitBits(s_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    } else {
        ESP_LOGI(TAG, "Already provisioned -- connecting as a regular STA");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect()); // Ініціалізація з'єднання з роутером за даними з NVS

        ESP_LOGI(TAG, "Waiting for IP with stored credentials (timeout %d s)...",
                 STA_CONNECT_TIMEOUT_MS / 1000);
        EventBits_t bits = xEventGroupWaitBits(s_event_group, CONNECTED_BIT, pdFALSE, pdTRUE,
                                                pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));
        if (!(bits & CONNECTED_BIT)) {
            ESP_LOGW(TAG, "Stored Wi-Fi credentials did not connect in time -- resetting "
                          "provisioning and rebooting into SoftAP so they can be re-entered");
            wifi_prov_mgr_reset_provisioning();
            esp_restart();
        }
        wifi_prov_mgr_deinit();
    }

    ESP_LOGI(TAG, "Wi-Fi ready");
}
#include "mqtt_link.h"
#include "mqtt_client.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>

#define TOPIC_TELEMETRY   "logger/telemetry"
#define TOPIC_CONTROL     "logger/control/led"   // payload: "ON" / "OFF"

static const char *TAG = "MQTT_LINK";
static esp_mqtt_client_handle_t s_client;
static bool s_connected = false;
static mqtt_link_led_cb_t s_led_cb = NULL;

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT connected to %s, subscribing to %s", CONFIG_MQTT_BROKER_URI, TOPIC_CONTROL);
        esp_mqtt_client_subscribe(s_client, TOPIC_CONTROL, 1);   // QoS 1
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected -- esp-mqtt will retry reconnecting in the background");
        break;
    case MQTT_EVENT_DATA: {
        // event->topic/event->data НЕ null-terminated -- порівнюємо за довжиною, копіюємо в локальний буфер
        if (strncmp(event->topic, TOPIC_CONTROL, event->topic_len) == 0 &&
            strlen(TOPIC_CONTROL) == event->topic_len) {
            char payload[8] = {0};
            int len = event->data_len < 7 ? event->data_len : 7;
            memcpy(payload, event->data, len);
            bool led_on = (strcmp(payload, "ON") == 0);
            ESP_LOGI(TAG, "Control command: %s -> LED %s", payload, led_on ? "ON" : "OFF");
            if (s_led_cb != NULL) s_led_cb(led_on);
        }
        break;
    }
    default:
        break;
    }
}

void mqtt_link_init(mqtt_link_led_cb_t on_led_command, mqtt_link_time_synced_cb_t on_time_synced)
{
    s_led_cb = on_led_command;

    // Wi-Fi вже підключений (wifi_prov, Частина 0) -- синхронізуємо час один раз, щоб "засіяти" Tiny RTC (Крок 3).
    // Звичайний mqtt:// не потребує коректного часу для самого підключення -- це лише для RTC.
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp_cfg);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK && on_time_synced != NULL) {
        on_time_synced();
    }

	esp_mqtt_client_config_t mqtt_cfg = {
	        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
	        .credentials.client_id = CONFIG_MQTT_CLIENT_IDENTIFIER,
	        .session.keepalive = 5,             // 5 секунд пінг
	        .network.timeout_ms = 1000,         // Таймаут на операції сокета 1 секунда (не зависати назавжди!)
	        .network.reconnect_timeout_ms = 2000, // Спроба перепідключення кожні 2 секунди
	    };
    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

bool mqtt_link_is_connected(void)
{
    return s_connected;
}

void mqtt_link_publish_telemetry(const char *json_payload)
{
    if (!s_connected) {
        return;
    }
    
    // Якщо сокет відвалився, esp_mqtt_client_publish поверне -1 (помилка) або -2 (out of memory)
    int msg_id = esp_mqtt_client_publish(s_client, TOPIC_TELEMETRY, json_payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish failed, dropping packet (broker unreachable)");
    }
}
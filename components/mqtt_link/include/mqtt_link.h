#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mqtt_link_led_cb_t)(bool led_on);
typedef void (*mqtt_link_time_synced_cb_t)(void);

// Викликати ПІСЛЯ wifi_prov_init_and_connect() (Частина 0) -- ця функція вже
// не займається Wi-Fi сама, лише SNTP (для Tiny RTC) + підключення до брокера.
void mqtt_link_init(mqtt_link_led_cb_t on_led_command, mqtt_link_time_synced_cb_t on_time_synced);
bool mqtt_link_is_connected(void);
void mqtt_link_publish_telemetry(const char *json_payload);

#ifdef __cplusplus
}
#endif
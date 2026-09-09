#include "led_ctrl.h"
#include "driver/gpio.h"

#define LED_PIN   GPIO_NUM_6

void led_ctrl_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(LED_PIN, 0);   // безпечний стан за замовчуванням -- LED вимкнений
}

void led_ctrl_set(bool on)
{
    gpio_set_level(LED_PIN, on ? 1 : 0);
}
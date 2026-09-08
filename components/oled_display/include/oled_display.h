#pragma once
#include "rtc_ds1307.h"

#ifdef __cplusplus
extern "C" {
#endif

void oled_display_init(void);
void oled_play_wolf_boot_animation(void);   // блокує рівно ~5 секунд (Крок 3, за вимогою проєкту)
void oled_draw_dashboard(const rtc_time_t *t,
                          float t_i2c, float h_i2c, float p_i2c,
                          float t_spi, float h_spi, float p_spi,
                          float threshold_c, bool led_on, const char *led_source);

#ifdef __cplusplus
}
#endif
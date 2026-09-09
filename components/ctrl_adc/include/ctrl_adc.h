#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     ctrl_adc_init(void);
float    ctrl_adc_get_threshold_c(void);   // поточний поріг тривоги, °C, у діапазоні 15.0-35.0
uint32_t ctrl_adc_get_raw(void);           // сирий усереднений відлік АЦП, 0-4095 (для дашборда/діагностики)

#ifdef __cplusplus
}
#endif
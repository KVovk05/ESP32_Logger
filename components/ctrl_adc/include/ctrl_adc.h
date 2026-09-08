#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void  ctrl_adc_init(void);
float ctrl_adc_get_threshold_c(void);   // поточний поріг тривоги, °C, у діапазоні 15.0-35.0

#ifdef __cplusplus
}
#endif
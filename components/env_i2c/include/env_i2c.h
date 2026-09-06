#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void env_i2c_init(void);
bool env_i2c_read(float *temp_C, float *press_hPa, float *hum_RH);   

#ifdef __cplusplus
}
#endif
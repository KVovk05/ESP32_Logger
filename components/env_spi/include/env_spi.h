#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void env_spi_init(void);
bool env_spi_read(float *temp_C, float *press_hPa, float *hum_RH);

#ifdef __cplusplus
}
#endif
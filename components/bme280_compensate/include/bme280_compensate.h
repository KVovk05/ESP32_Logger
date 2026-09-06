#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4, dig_H5;
    int8_t   dig_H6;
    int32_t  t_fine;
} bme280_calib_t;

void bme280_parse_calib(bme280_calib_t *c, const uint8_t buf_tp[26], const uint8_t buf_h[7]);
int32_t  bme280_compensate_T(bme280_calib_t *c, int32_t adc_T);   // 0.01 град.Ц (5123 -> 51.23)
uint32_t bme280_compensate_P(bme280_calib_t *c, int32_t adc_P);   // Q24.8, Па -- поділити на 256.0
uint32_t bme280_compensate_H(bme280_calib_t *c, int32_t adc_H);   // Q22.10, %RH -- поділити на 1024.0

#ifdef __cplusplus
}
#endif
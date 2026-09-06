#include "include/bme280_compensate.h"
#
static uint16_t u16_le(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }

void bme280_parse_calib(bme280_calib_t *c, const uint8_t buf_tp[26], const uint8_t buf_h[7])
{
    c->dig_T1 = u16_le(&buf_tp[0]);
    c->dig_T2 = (int16_t)u16_le(&buf_tp[2]);
    c->dig_T3 = (int16_t)u16_le(&buf_tp[4]);
    c->dig_P1 = u16_le(&buf_tp[6]);
    c->dig_P2 = (int16_t)u16_le(&buf_tp[8]);
    c->dig_P3 = (int16_t)u16_le(&buf_tp[10]);
    c->dig_P4 = (int16_t)u16_le(&buf_tp[12]);
    c->dig_P5 = (int16_t)u16_le(&buf_tp[14]);
    c->dig_P6 = (int16_t)u16_le(&buf_tp[16]);
    c->dig_P7 = (int16_t)u16_le(&buf_tp[18]);
    c->dig_P8 = (int16_t)u16_le(&buf_tp[20]);
    c->dig_P9 = (int16_t)u16_le(&buf_tp[22]);
    c->dig_H1 = buf_tp[25];

    c->dig_H2 = (int16_t)u16_le(&buf_h[0]);
    c->dig_H3 = buf_h[2];
    c->dig_H4 = (int16_t)((buf_h[3] << 4) | (buf_h[4] & 0x0F));
    c->dig_H5 = (int16_t)((buf_h[5] << 4) | (buf_h[4] >> 4));
    c->dig_H6 = (int8_t)buf_h[6];
}

int32_t bme280_compensate_T(bme280_calib_t *c, int32_t adc_T)
{
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)c->dig_T1 << 1))) * ((int32_t)c->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)c->dig_T1)) * ((adc_T >> 4) - ((int32_t)c->dig_T1))) >> 12) *
            ((int32_t)c->dig_T3)) >> 14;
    c->t_fine = var1 + var2;
    T = (c->t_fine * 5 + 128) >> 8;
    return T;
}

uint32_t bme280_compensate_P(bme280_calib_t *c, int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)c->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 = var2 + ((var1 * (int64_t)c->dig_P5) << 17);
    var2 = var2 + (((int64_t)c->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) + ((var1 * (int64_t)c->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)c->dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)c->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)c->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)c->dig_P7) << 4);
    return (uint32_t)p;
}

uint32_t bme280_compensate_H(bme280_calib_t *c, int32_t adc_H)
{
    int32_t v_x1_u32r;
    v_x1_u32r = c->t_fine - (int32_t)76800;
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)c->dig_H4) << 20) - (((int32_t)c->dig_H5) * v_x1_u32r)) +
                 (int32_t)16384) >> 15) *
                 (((((((v_x1_u32r * (int32_t)c->dig_H6) >> 10) *
                 (((v_x1_u32r * (int32_t)c->dig_H3) >> 11) + (int32_t)32768)) >> 10) + (int32_t)2097152) *
                 (int32_t)c->dig_H2 + 8192) >> 14));
    v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)c->dig_H1) >> 4);
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    return (uint32_t)(v_x1_u32r >> 12);
}
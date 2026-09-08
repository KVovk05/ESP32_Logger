#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint8_t sec, min, hour, wday, date, month, year; } rtc_time_t;

void rtc_ds1307_init(void);
bool rtc_ds1307_read_time(rtc_time_t *out);
void rtc_ds1307_set_from_tm(const struct tm *utc);   

#ifdef __cplusplus
}
#endif
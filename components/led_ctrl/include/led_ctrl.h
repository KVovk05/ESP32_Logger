#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void led_ctrl_init(void);
void led_ctrl_set(bool on);

#ifdef __cplusplus
}
#endif
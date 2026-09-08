#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void wifi_prov_init_and_connect(void);   // блокує до отримання IP -- через SoftAP-provisioning або одразу як STA

#ifdef __cplusplus
}
#endif
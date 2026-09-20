#ifndef GNSS_TEST_FREERTOS_H
#define GNSS_TEST_FREERTOS_H
#include <stdint.h>
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000U
#endif
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(((uint64_t)(ms) * configTICK_RATE_HZ) / 1000U))
void host_enter_critical(void);
void host_exit_critical(void);
#define taskENTER_CRITICAL() host_enter_critical()
#define taskEXIT_CRITICAL() host_exit_critical()
#endif

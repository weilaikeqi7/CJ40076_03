#ifndef GNSS_TEST_TASK_H
#define GNSS_TEST_TASK_H
#include <stdint.h>
uint32_t xTaskGetTickCount(void);
void vTaskDelay(uint32_t ticks);
#endif

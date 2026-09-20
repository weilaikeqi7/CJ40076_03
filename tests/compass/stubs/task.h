#ifndef COMPASS_TEST_TASK_H
#define COMPASS_TEST_TASK_H
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
void vTaskSuspendAll(void);
int xTaskResumeAll(void);
#endif

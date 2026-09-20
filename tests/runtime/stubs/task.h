#ifndef TEST_RUNTIME_TASK_H
#define TEST_RUNTIME_TASK_H
#include "FreeRTOS.h"
TaskHandle_t xTaskGetCurrentTaskHandle(void);
BaseType_t xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg,
                       unsigned priority, TaskHandle_t *handle);
void vTaskSuspend(TaskHandle_t task);
void vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);
void vTaskDelete(TaskHandle_t task);
void vTaskStartScheduler(void);
#endif

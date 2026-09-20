#ifndef TEST_RUNTIME_FREERTOS_H
#define TEST_RUNTIME_FREERTOS_H
#include <stdint.h>
typedef int BaseType_t;
typedef void *TaskHandle_t;
#define pdPASS ((BaseType_t)1)
#define configMINIMAL_STACK_SIZE 32U
#define tskIDLE_PRIORITY 0U
#endif

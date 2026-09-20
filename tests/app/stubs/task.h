#ifndef TEST_APP_TASK_H
#define TEST_APP_TASK_H
#include <stdint.h>
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
void vTaskDelay(uint32_t ticks);
#endif

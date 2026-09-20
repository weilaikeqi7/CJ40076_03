#ifndef TEST_MEAS_TASK_H
#define TEST_MEAS_TASK_H
#include <stdint.h>
void test_enter_critical(void);
void test_exit_critical(void);
#define taskENTER_CRITICAL() test_enter_critical()
#define taskEXIT_CRITICAL() test_exit_critical()
uint32_t xTaskGetTickCount(void);
void vTaskDelay(uint32_t ticks);
void vTaskSuspendAll(void);
int xTaskResumeAll(void);
#endif

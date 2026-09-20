#include "main.h"

#include "app.h"
#include "bsp_gpio.h"
#include "bsp_power.h"

#include "FreeRTOS.h"
#include "task.h"

static TaskHandle_t s_workers[4];

/* 关机不可撤销：挂起其他业务任务，防止等待中的外设操作恢复后重新上电。 */
void app_stop_tasks(void)
{
    TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    unsigned i;
    for (i = 0U; i < 4U; ++i)
    {
        if (s_workers[i] != NULL && s_workers[i] != caller)
        {
            vTaskSuspend(s_workers[i]);
        }
    }
}

static void app_task_boot(void* argument)
{
    BaseType_t ret;
    (void)argument;

    /* 带延时的外设初始化只能在调度器启动后执行，此时当前任务有效。 */
    app_system_init();

    vTaskSuspendAll();
    ret = xTaskCreate(app_task_key, "T_KEY", configMINIMAL_STACK_SIZE * 2U, NULL,
                      tskIDLE_PRIORITY + 4U, &s_workers[0]);
    if (ret != pdPASS) { Error_Handler(); }
    ret = xTaskCreate(app_task_sensor, "T_SENS", configMINIMAL_STACK_SIZE * 3U, NULL,
                      tskIDLE_PRIORITY + 3U, &s_workers[1]);
    if (ret != pdPASS) { Error_Handler(); }
    ret = xTaskCreate(app_task_display, "T_DISP", configMINIMAL_STACK_SIZE * 3U, NULL,
                      tskIDLE_PRIORITY + 2U, &s_workers[2]);
    if (ret != pdPASS) { Error_Handler(); }
    ret = xTaskCreate(app_task_power, "T_PWR", configMINIMAL_STACK_SIZE * 2U, NULL,
                      tskIDLE_PRIORITY + 1U, &s_workers[3]);
    if (ret != pdPASS) { Error_Handler(); }

    /* 任务句柄全部登记后再恢复调度，关机流程才能可靠挂起所有业务任务。 */
    (void)xTaskResumeAll();
    vTaskDelete(NULL);
}

int main(void)
{
    BaseType_t ret;

    /* 配置中断优先级组 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 最先初始化 GPIO 并锁存电源保持，默认关闭各外设供电 */
    bsp_gpio_init();
    bsp_power_init();

    ret = xTaskCreate(app_task_boot, "T_BOOT", configMINIMAL_STACK_SIZE * 3U, NULL,
                      tskIDLE_PRIORITY + 4U, NULL);
    if (ret != pdPASS) { Error_Handler(); }

    vTaskStartScheduler();
    Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

void AppAssertFailed(const char* file, int line)
{
    (void)file;
    (void)line;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name)
{
    (void)task;
    (void)task_name;
    Error_Handler();
}

#ifdef USE_FULL_ASSERT
void assert_failed(const uint8_t* expr, const uint8_t* file, uint32_t line)
{
    (void)expr;
    (void)file;
    (void)line;
    Error_Handler();
}
#endif


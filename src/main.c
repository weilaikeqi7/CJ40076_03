#include "main.h"

#include "app.h"
#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

#if defined(N32_EXPECT_FPU) && (N32_EXPECT_FPU == 1)
#if (__FPU_USED != 1)
#error "FPU was requested, but CMSIS reports __FPU_USED != 1. Check -mfpu and -mfloat-abi."
#endif
#endif

int main(void)
{
    BaseType_t ret;

    /* 最先初始化 GPIO 并保持电源（含电源保持脚置高） */
    board_gpio_init();

    /* 启动核心驱动并完成自检 */
    app_system_init();

    /* ---------------- 建立 4 大专业并发业务任务 ---------------- */
    /* Task 1: 人机交互与按键即时响应 (最高优先级 4，确保长按与单击永不卡顿) */
    ret = xTaskCreate(app_task_key, "T_KEY", configMINIMAL_STACK_SIZE * 2U, NULL, tskIDLE_PRIORITY + 4U, NULL);
    if (ret != pdPASS) { Error_Handler(); }

    /* Task 2: 传感器采集与空间三角经纬度投影解算 (优先级 3) */
    ret = xTaskCreate(app_task_sensor, "T_SENS", configMINIMAL_STACK_SIZE * 3U, NULL, tskIDLE_PRIORITY + 3U, NULL);
    if (ret != pdPASS) { Error_Handler(); }

    /* Task 3: 屏幕画面刷新(100ms)与极低温10kHz自适应闭环温控(1s) (优先级 2) */
    ret = xTaskCreate(app_task_display, "T_DISP", configMINIMAL_STACK_SIZE * 3U, NULL, tskIDLE_PRIORITY + 2U, NULL);
    if (ret != pdPASS) { Error_Handler(); }

    /* Task 4: 电池电压采样、1Hz闪烁监控与欠压紧急断电保护 (优先级 1) */
    ret = xTaskCreate(app_task_power, "T_PWR", configMINIMAL_STACK_SIZE * 2U, NULL, tskIDLE_PRIORITY + 1U, NULL);
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

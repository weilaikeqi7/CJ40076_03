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
    BaseType_t created;

    /* 最先初始化 GPIO 并保持电源（含电源保持脚置高） */
    board_gpio_init();

    created = xTaskCreate(app_run, "APP", configMINIMAL_STACK_SIZE * 6U, NULL, tskIDLE_PRIORITY + 1U, NULL);
    if (created != pdPASS)
    {
        Error_Handler();
    }

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

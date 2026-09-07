#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

#if defined(N32_EXPECT_FPU) && (N32_EXPECT_FPU == 1)
#if (__FPU_USED != 1)
#error "FPU was requested, but CMSIS reports __FPU_USED != 1. Check -mfpu and -mfloat-abi."
#endif
#endif

static void enable_gpio_clock(GPIO_Module* gpio)
{
    if (gpio == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (gpio == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }
    else if (gpio == GPIOC)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
    }
    else if (gpio == GPIOD)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, ENABLE);
    }
    else if (gpio == GPIOE)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOE, ENABLE);
    }
}

static void led_init(GPIO_Module* gpio, uint16_t pin)
{
    GPIO_InitType gpio_init;

    enable_gpio_clock(gpio);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin        = pin;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitPeripheral(gpio, &gpio_init);
}

static void led_on(GPIO_Module* gpio, uint16_t pin)
{
    gpio->PBSC = pin;
}

static void led_toggle(GPIO_Module* gpio, uint16_t pin)
{
    gpio->POD ^= pin;
}

static void led_task(void* argument)
{
    (void)argument;

    led_init(LED1_PORT, LED1_PIN);
    led_init(LED2_PORT, LED2_PIN);
    led_on(LED1_PORT, LED1_PIN);

    while (1)
    {
        led_toggle(LED2_PORT, LED2_PIN);
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}

int main(void)
{
    BaseType_t created;

    created = xTaskCreate(led_task, "LED", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1U, NULL);
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

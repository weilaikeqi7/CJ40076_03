/**
 * @file n32g4fr_it.c
 * @brief 内核异常入口；致命故障停留现场，外设中断由各 BSP 模块处理。
 * 故障处理不调用 RTOS 或日志；若已启动独立看门狗，停止喂狗后将超时复位。
 */
#include "n32g4fr_it.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void DebugMon_Handler(void)
{
}

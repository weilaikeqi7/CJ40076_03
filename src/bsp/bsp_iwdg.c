/**
 * @file bsp_iwdg.c
 * @brief N32G4FR MCU 片上独立看门狗（IWDG）驱动实现
 */
#include "bsp_iwdg.h"

#include "n32g4fr.h"

void bsp_iwdg_init(uint32_t timeout_ms)
{
    /*
     * N32G4FR LSI 内部独立低速振荡器标称频率为 40kHz (周期 0.025ms)。
     * 选用 64 分频 (PRESCALER_DIV64)：
     *   计数时钟 = 40000 / 64 = 625 Hz (每 1 个 tick 约 1.6ms)
     * 12 位倒计数重装载值：
     *   reload = timeout_ms * 625 / 1000
     */
    uint32_t reload = timeout_ms * 625U / 1000U;
    if (reload > 0x0FFFU)
    {
        reload = 0x0FFFU; /* 12 位寄存器最大为 4095 (对应约 6.55 秒) */
    }
    if (reload == 0U)
    {
        reload = 1U;
    }

    /* 1. 解锁 IWDG_PREDIV 与 IWDG_RELV 寄存器写权限 (写入 0x5555) */
    IWDG_WriteConfig(IWDG_WRITE_ENABLE);

    /* 2. 配置 64 分频 */
    IWDG_SetPrescalerDiv(IWDG_PRESCALER_DIV64);

    /* 3. 设定 12 位重装载倒计数值 */
    IWDG_CntReload((uint16_t)reload);

    /* 4. 首次装载计数值 (写入 0xAAAA) */
    IWDG_ReloadKey();

    /* 5. 启动看门狗 (写入 0xCCCC，硬件启动后不可被软件关闭) */
    IWDG_Enable();
}

void bsp_iwdg_feed(void)
{
    IWDG_ReloadKey(); /* 写入 0xAAAA 喂狗刷新倒计数 */
}

/**
 * @file dev_compass.h
 * @brief MCG505 型全国产三维电子罗盘设备驱动（USART2 PA2/PA3，38400 8N1）
 *
 * 协议（遵守 ANSI/IEEE Std 754-1985，大端浮点数）：
 *   帧结构：[AA 55: 2B] [长度: Uint8 1B] [地址码: 00 1B] [标识符: Uint8 1B] [数据区: N 字节] [CRC-16: Uint16 大端]
 *   长度为整帧字节总数（含帧头与 CRC）。
 *
 * 核心配置：
 *   - 机械安装方式：Y 轴朝下 180°（标志位 3，参数 23 = 0x17）
 *   - 广播模式：10Hz 连续输出 方位角(1)、俯仰角(2)、横滚角(3)
 *   - 磁场校准：磁场空间手动校准（模式 1，默认 12 采样点，短按电源键采样，采满自动输出得分）
 */
#ifndef DEV_COMPASS_H
#define DEV_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 姿态与航向数据结构 */
typedef struct
{
    float    heading;    /* 方位角（0.00° ~ 359.99°） */
    float    pitch;      /* 俯仰角（-90.00° ~ +90.00°） */
    float    roll;       /* 横滚角（-180.00° ~ +180.00°） */
    uint32_t tick_angle; /* 最近一次收到角度帧的系统 tick */
} mcg505_data_t;

/** 校准回调或状态数据结构 */
typedef struct
{
    uint32_t sample_count; /* 当前已采样点数（从 1 开始累加） */
    float    cal_score;    /* 磁场校准得分（<0.168 优，0.168~0.248 良，0.248~0.328 中，0.328~0.36 差，99.9 强磁干扰异常） */
    bool     score_valid;  /* 是否已收到校准完成得分 */
} mcg505_cal_state_t;

/**
 * @brief 控制罗盘硬件供电，上电时自动等待启动稳定并刷新接收缓存
 * @param on true 打开供电，false 关闭供电
 */
void mcg505_power_ctl(bool on);

/**
 * @brief 执行罗盘启动自检（在指定超时内等待第一帧有效姿态角广播）
 * @param timeout_ms 超时时间（毫秒，通常 3000ms）
 * @return true 自检成功，false 自检失败（超时无有效角度帧）
 */
bool mcg505_self_check(uint32_t timeout_ms);

/**
 * @brief 初始化并配置 MCG505 电子罗盘：
 *        开启电源 -> 等待启动稳定 -> 设置安装方式为 Y 轴朝下 180° -> 设置输出组件(方位/俯仰/横滚) -> 启动 10Hz 广播模式。
 * @note  必须在 FreeRTOS 任务上下文调用。
 */
void mcg505_init(void);

/**
 * @brief 喂串口接收数据并解析协议帧，主循环/传感器任务周期性调用。
 */
void mcg505_poll(void);

/**
 * @brief 获取当前最新姿态与方位角数据。
 */
const mcg505_data_t* mcg505_get_data(void);

/**
 * @brief 获取校准过程中的采样点数与得分状态。
 */
const mcg505_cal_state_t* mcg505_get_cal_state(void);

/**
 * @brief 检查罗盘在线存活状态（根据最近一次收到角度帧时间戳）。
 */
bool mcg505_is_alive(uint32_t timeout_ms);

/* ----------------------------- 校准控制接口 ----------------------------- */

/**
 * @brief 启动磁场空间手动校准（5击调用）。
 *        发送开始校准指令 StartCal (0x0F, Mode=1: 磁场空间手动校准)。
 *        注：手册说明发送开始校准指令后，罗盘会自动采集第一组数据并返回采样点编号 1。
 */
void mcg505_start_mag_cal(void);

/**
 * @brief 发送单次校准数据采样指令（短按电源键调用）。
 *        发送 TakeUserCalSample 指令（0x11）。
 */
void mcg505_take_sample(void);

/**
 * @brief 停止校准（未完成采样时 6 击调用）。
 *        发送 StopCal 指令（0x10）。
 */
void mcg505_stop_cal(void);

/**
 * @brief 恢复出厂设置（10击调用）：清除自定义校准参数，恢复出厂设置并重新配置 Y 轴朝下 180°及 10Hz 广播模式。
 */
void mcg505_factory_reset(void);

/**
 * @brief CRC-16-CCITT (XModem) 校验函数（符合 MCG505 手册标准算法）。
 */
uint16_t mcg505_crc16(const uint8_t* buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* DEV_COMPASS_H */

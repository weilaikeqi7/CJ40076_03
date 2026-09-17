/**
 * @file mcp406.h
 * @brief MCP-406-TTL 三维高精度电子罗盘驱动（USART2 PA2/PA3，38400 8N1）
 *
 * 协议（兼容 PNI TCM5 / TCM XB 协议，遵守 ANSI/IEEE Std 754-1985）：
 *   帧结构：[Length: Uint16 大端] [CmdID: Uint8] [Data: N 字节] [CRC-16: Uint16 大端]
 *   Length 为整帧字节总数（含 Length 与 CRC）。
 *
 * 核心配置：
 *   - 机械安装方式：Y 轴朝下 180°（标志位 10，参数 23 = 0x17）
 *   - 广播模式：10Hz 连续输出 方位角(5)、俯仰角(24=0x18)、横滚角(25=0x19)
 *   - 磁场校准：空间手动校准（TCM5 方式 10 = 0x0A）
 */
#ifndef MCP406_H
#define MCP406_H

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
} mcp406_data_t;

/** 校准回调或状态数据结构 */
typedef struct
{
    uint32_t sample_count; /* 当前已采样点数（从 1 开始累加） */
    float    cal_score;    /* 校准得分（<=1.02 为优良中差正常得分，>1.02 或 35/99.9/400 为异常） */
    bool     score_valid;  /* 是否已收到校准完成得分 */
} mcp406_cal_state_t;

/**
 * @brief 控制罗盘硬件供电，上电时自动等待启动稳定并刷新接收缓存
 * @param on true 打开供电，false 关闭供电
 */
void mcp406_power_ctl(bool on);

/**
 * @brief 执行罗盘启动自检（在指定超时内等待第一帧有效姿态角广播）
 * @param timeout_ms 超时时间（毫秒，通常 3000ms）
 * @return true 自检成功，false 自检失败（超时无有效角度帧）
 */
bool mcp406_self_check(uint32_t timeout_ms);

/**
 * @brief 初始化并配置 MCP-406 电子罗盘：
 *        开启电源 -> 等待启动 -> 设安装方式为 Y 轴朝下 180° -> 设输出组件 -> 启动 10Hz 广播模式。
 * @note  必须在 FreeRTOS 任务上下文调用。
 */
void mcp406_init(void);

/**
 * @brief 喂串口接收数据并解析协议帧，主循环/传感器任务周期性调用。
 */
void mcp406_poll(void);

/**
 * @brief 获取当前最新姿态与方位角数据。
 */
const mcp406_data_t* mcp406_get_data(void);

/**
 * @brief 获取校准过程中的采样点数与得分状态。
 */
const mcp406_cal_state_t* mcp406_get_cal_state(void);

/**
 * @brief 检查罗盘在线存活状态（根据最近一次收到角度帧时间戳）。
 */
bool mcp406_is_alive(uint32_t timeout_ms);

/* ----------------------------- 校准控制接口 ----------------------------- */

/**
 * @brief 启动磁场空间手动校准（5击调用）。
 *        发送 TCM 5 型空间手动校准指令（0x0A，参数 10）。
 *        注：手册说明发送开始校准指令后，罗盘会自动采集第一组数据并返回采样点编号 1。
 */
void mcp406_start_mag_cal(void);

/**
 * @brief 发送单次校准数据采样指令（短按电源键调用）。
 *        发送 TakeUserCalSample 指令（0x1F）。
 */
void mcp406_take_sample(void);

/**
 * @brief 停止校准（未完成采样时 6 击调用）。
 *        发送 StopCal 指令（0x0B）。
 */
void mcp406_stop_cal(void);

/**
 * @brief 保存配置及校准参数到罗盘 EEPROM（校准得分正常时 6 击调用，或修改参数后调用）。
 *        发送 Save 指令（0x09）。
 */
void mcp406_save(void);

/**
 * @brief 恢复出厂设置（10击调用）：清除自定义补偿，恢复罗盘出厂校准并重新下发 Y 轴朝下 180°及 10Hz 广播配置。
 */
void mcp406_factory_reset(void);

/**
 * @brief CRC-16 校验函数（兼容手册 ANSI/IEEE Std 754 算法）。
 */
uint16_t mcp406_crc16(const uint8_t* buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* MCP406_H */

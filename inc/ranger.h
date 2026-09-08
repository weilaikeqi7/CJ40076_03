/**
 * @file ranger.h
 * @brief DYC-15A 激光测距机驱动（UART6 PB0/PB1 重映射，115200 8N1）
 *
 * 协议帧：0xEE 0x16 | 数据长度(1) | 设备码 0x03 | 命令码 | 参数 0~4 | 校验和
 *   数据长度 = 设备码+命令码+参数 的总字节数（2~9）
 *   校验和   = (设备码+命令码+参数) 求和低 8 位
 *
 * 上电时序：PB3 同时控制测距机 VCC 与 POWER_ON 引脚，拉高后需等待约 1.5s
 *           （驱动电容充电）才能下发指令——ranger_init() 已完成等待。
 *
 * 使用：ranger_init()（任务上下文）-> 主循环调 ranger_poll() ->
 *       发送 ranger_range_single() 等命令后从 ranger_get_range() 取结果。
 */
#ifndef RANGER_H
#define RANGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 目标模式（命令 0x03 参数） */
typedef enum
{
    RANGER_TARGET_FIRST = 0x01, /* 首目标测距 */
    RANGER_TARGET_LAST  = 0x02, /* 末目标测距 */
    RANGER_TARGET_MULTI = 0x03, /* 多目标测距 */
} ranger_target_t;

/** 测距结果状态（首末/单目标模式） */
typedef enum
{
    RANGER_ST_SINGLE     = 0x00, /* 单目标 */
    RANGER_ST_HAS_FRONT  = 0x01, /* 有前目标 */
    RANGER_ST_HAS_BACK   = 0x02, /* 有后目标 */
    RANGER_ST_BOTH       = 0x03, /* 多目标模式：前后目标均有 */
    RANGER_ST_OUT_OF_RANGE = 0x04, /* 超距 */
} ranger_status_t;

/** 测距结果 */
typedef struct
{
    uint8_t  status;      /* 见 ranger_status_t；多目标模式高 4 位为目标编号[0,N-1] */
    uint8_t  target_no;   /* 多目标模式下的结果编号 */
    float    distance_m;  /* 距离，米 */
    uint32_t tick;        /* 更新时间 tick */
    bool     continuous;  /* 来自连续测距(0x04) 还是单次测距(0x02) */
} ranger_range_t;

/** 自检结果（命令 0x01 响应） */
typedef struct
{
    uint8_t echo_strength; /* 回波强度 0~255 */
    bool    fpga_ok;       /* Status1.bit0 */
    bool    laser_on;      /* Status1.bit1 出光状态 */
    bool    main_wave;     /* Status1.bit2 有主波 */
    bool    echo;          /* Status1.bit3 有回波 */
    bool    bias_on;       /* Status1.bit4 */
    bool    bias_ok;       /* Status1.bit5 */
    bool    temp_ok;       /* Status1.bit6 */
    bool    power_5v6_ok;  /* Status0.bit0 */
    uint32_t tick;
} ranger_selfcheck_t;

/** 上电（PB3 拉高）+ 串口初始化 + 等待 1.6s 电容充电。任务上下文调用 */
void ranger_init(void);

/** 下电（测距前请先 ranger_stop()） */
void ranger_deinit(void);

/** 喂串口数据解析响应帧，主循环周期调用 */
void ranger_poll(void);

/* ------------------------------ 命令（发送，异步等响应） ------------------------------ */

void ranger_self_check(void);              /* 设备自检（会出光，注意防护） */
void ranger_range_single(void);            /* 单次测距 */
void ranger_range_continuous_start(void);  /* 连续测距 */
void ranger_range_stop(void);              /* 停止测距 */
void ranger_set_target_mode(ranger_target_t mode);
void ranger_set_continuous_freq(uint8_t hz); /* 1~10Hz */
void ranger_set_gate_min(uint16_t m);      /* 最小选通距离 10~20000m，0=关闭选通 */
void ranger_set_gate_max(uint16_t m);      /* 最大选通距离 10~20000m */

/* ------------------------------ 结果读取 ------------------------------ */

/** 取最近一次测距结果；返回 true 表示是新结果（取走后清除新标志） */
bool ranger_get_range(ranger_range_t* out);

/** 取最近一次自检结果 */
bool ranger_get_selfcheck(ranger_selfcheck_t* out);

/** 最近一次异常帧（命令 0x06）的 Status1 位图；0xFF=无异常 */
uint8_t ranger_last_error(void);

/** 是否在线（超时内收到过任意合法响应帧） */
bool ranger_is_alive(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* RANGER_H */

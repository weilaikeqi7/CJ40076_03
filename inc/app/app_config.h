/**
 * @file app_config.h
 * @brief CJ40076 V3 业务层全局配置常量
 *
 * 业务规格来源：CJ40076_测试人员测试手册（V2.0 基线）+ 本版（V3）变更：
 *   - 距离/角度/高程均显示 1 位小数；新断码屏界面
 *   - 三击计数清零立即写 Flash；正常关机和欠压关机都写 Flash
 *   - 电池图标 4 段显示（框 + 3 格条）
 *   - 补偿值显示在高程区（1 位小数），实时生效值回到各自区域
 *   - 高程/距离/航向最高位为特殊笔画段，只需显示 0~3
 *   - 测距机初始化后常供电（不按测量上电）
 *   - 帧间静默统一 200ms 判定一轮结束；单目标只按首目标（F）显示
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------ 按键 ------------------------------ */
#define APP_KEY_SCAN_MS        10U   /* 扫描周期 */
#define APP_KEY_DEBOUNCE_MS    30U   /* 消抖 */
#define APP_KEY_MULTICLICK_MS  600U  /* 模式键多击窗口（末次点击后超时生效） */
#define APP_KEY_LONG_MS        3000U /* 电源键长按关机 */
#define APP_KEY_BOTH_LONG_MS   1000U /* 双键同按（设置页切页/保存） */
#define APP_KEY_REPEAT_MS      100U  /* 设置页按住连续调节间隔 */

/* ------------------------------ 测距 ------------------------------ */
#define APP_MEASURE_SILENCE_MS    200U   /* 帧间静默判定一轮结束（统一 200ms） */
#define APP_MEASURE_TIMEOUT_MS    3000U  /* 单轮无回包超时 */
#define APP_MEASURE_CONT_PERIOD_MS 8000U /* 连续模式轮次周期 */
#define APP_MEASURE_TEST_PERIOD_MS 12000U /* 测试模式笔次周期 */

/** 测距无效距离值（整数部分 0xFFFF） */
#define APP_RANGE_INVALID_INT 0xFFFFU

/* ------------------------------ 姿态/罗盘 ------------------------------ */
/** 默认补偿（0.01 度单位）：PIt=0.00°，HIt=+90.00°，HEr=0.00° */
#define APP_DEFAULT_PIT_C01 0
#define APP_DEFAULT_HIT_C01 9000
#define APP_DEFAULT_HER_C01 0

/** 补偿范围（0.01 度）：PIt ±90°，HIt/HEr ±180° */
#define APP_PIT_MAX_C01 9000
#define APP_HIT_MAX_C01 18000
#define APP_HER_MAX_C01 18000

/** LCD 俯仰显示映射：实际 ±85°~±88° 映射为显示 ±85°~±90° */
#define APP_PIT_DISPLAY_MAP_START_C01 8500
#define APP_PIT_DISPLAY_MAP_END_C01   8800

/** 航向显示范围（0.01 度）：0.00°~359.99° */
#define APP_HEADING_PERIOD_C01 36000
#define APP_HEADING_MAX_C01    35999

/** IMU 数据超时（超过则认为姿态无效） */
#define APP_IMU_TIMEOUT_MS 500U
/** GNSS 定位数据超时 */
#define APP_GNSS_TIMEOUT_MS 2500U

/* ------------------------------ 电池 ------------------------------ */
/** 电量分档（mV）：4 段显示（框 + 3 格条），80% 与 100% 同为 4 格 */
#define APP_BATT_LVL4_MV 3800U /* >= 3 格条全亮（4 格） */
#define APP_BATT_LVL3_MV 3700U /* >= 2 格条 */
#define APP_BATT_LVL2_MV 3600U /* >= 1 格条 */
                               /* 以下 = 1 格（仅外框） */

/** 欠压关机阈值（mV） */
#define APP_BATT_LOW_OFF_MV 2600U
/** 电池采样周期 */
#define APP_BATT_CHECK_MS 500U

/* ------------------------------ 计数 ------------------------------ */
#define APP_COUNT_MAX 9999U

/* ------------------------------ 显示交替周期 ------------------------------ */
#define APP_DISP_FE_TOGGLE_FAST_MS 1000U /* 单次/连续模式 F/E 切换 */
#define APP_DISP_FE_TOGGLE_SLOW_MS 2000U /* 多功能/测试模式 F/E 切换 */
#define APP_DISP_LL_TOGGLE_MS     1000U /* 经纬度交替 */
#define APP_DISP_RENDER_MS        100U  /* 渲染周期 */

#endif /* APP_CONFIG_H */

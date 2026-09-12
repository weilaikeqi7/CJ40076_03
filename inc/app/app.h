/**
 * @file app.h
 * @brief CJ40076 V3 应用主状态机
 *
 * 职责：
 *   - 启动序列（参数加载、JY901B 配置与角度帧自检、测距机/显示屏上电）
 *   - 按键事件分发（模式切换、测量触发、计数清零、校准入口、长按关机）
 *   - 外设供电调度（测距机常开；IMU/GNSS 按模式；校准页 GNSS 关/IMU 保）
 *   - 多功能/测试模式目标坐标解算
 *   - 电池分档与欠压关机（<2600mV，保存计数后断电）
 *   - 长按 3s 正常关机（保存计数后断电）
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** 4大并发业务任务函数 */
void app_task_key(void* argument);     /* 优先级 4：按键扫描与即时人机交互响应 (10ms) */
void app_task_sensor(void* argument);  /* 优先级 3：测距/姿态/GNSS轮询与目标坐标解算 */
void app_task_display(void* argument); /* 优先级 2：屏幕渲染(100ms)与低温10kHz闭环温控(1s) */
void app_task_power(void* argument);   /* 优先级 1：电池电量分档与3.0V欠压安全保护(500ms) */

/** 系统启动与多任务初始化入口 */
void app_system_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */

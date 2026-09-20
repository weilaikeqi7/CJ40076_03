/**
 * @file app_measure.h
 * @brief 测距轮次状态机（业务规则见 app_config.h 头部注释）
 *
 * 一轮测量：发多目标设置 -> 发单次测距 -> 收集回包 ->
 *           末帧静默 200ms 判定结束 -> 聚合发布（近=F / 远=E，中间目标丢弃）
 *   - 单目标（status 0x00 或仅 1 帧编号 0）：只出首目标 F
 *   - 3s 无回包：发布无目标（横杠）
 *   - 每发布一轮（含无目标）计数 +1（上限 9999）
 *   - 连续模式：启动立即测，之后每 8s 一轮，电源键停止，不自动停
 *   - 测试模式：启动立即测，之后每 12s 一笔，电源键停止
 *   - 单次/多功能模式：测量中重按电源键 = 重新发起；连续/测试 = 停止
 */
#ifndef APP_MEASURE_H
#define APP_MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MEAS_MODE_SINGLE = 0, /* 单次 */
    MEAS_MODE_CONT,       /* 连续（8s 周期） */
    MEAS_MODE_MULTI,      /* 多功能（单次测量 + 目标坐标解算） */
    MEAS_MODE_TEST,       /* 测试（12s 周期多功能测量） */
} meas_mode_t;

/** 一轮发布结果（距离单位：毫米） */
typedef struct
{
    bool     near_valid;   /* 首目标 F 有效 */
    uint32_t near_mm;
    bool     far_valid;    /* 末目标 E 有效 */
    uint32_t far_mm;
    uint32_t publish_tick; /* 发布时刻 */
    uint32_t round_id;     /* 非零轮次编号，每次实际发送 SINGLE 时递增。 */
    uint32_t request_id;   /* 请求代号；用 result_is_current 判断是否被取消。 */
    uint32_t start_tick;   /* 实际发送 SINGLE 的时刻。 */
    meas_mode_t mode;      /* 本轮发送时的测量模式。 */
    bool attitude_valid;  /* 发送时姿态有效性，禁止用稍后的姿态替代。 */
    int32_t heading_c01;
    int32_t pitch_c01;
} measure_result_t;

/** 切换模式：停止会话/轮次并清除结果 */
void measure_set_mode(meas_mode_t mode);

/** 停止当前会话/轮次并清除结果（模式不变，进入校准页时用） */
void measure_stop(void);

/** 电源键短按：按当前模式启动/重发/停止 */
void measure_trigger(void);

/** 10ms 周期调用：喂串口解析 + 定时器推进 */
void measure_poll(void);

/** 会话或轮次进行中 */
bool measure_is_running(void);

/** 轮次正在准备/等待指令间隔/测量；停止请求经传感器任务确认后才返回
 * false。连续/测试的轮间等待返回 false，可用于校准前的取消确认。 */
bool measure_round_active(void);

/** 兼容旧指针接口：调用者须在临界区内完成解引用/拷贝。 */
const measure_result_t* measure_get_result(void);

/** 原子拷贝显示结果快照，跨任务读取优先使用此接口。 */
void measure_copy_result(measure_result_t* out);

/** 原子消费一笔完整发布结果；仅供传感器任务中的单一消费者使用。 */
bool measure_take_result(measure_result_t* out);

/** 检查结果所属请求是否仍有效。用于保护坐标发布时，调用者须在提交
 * 坐标的同一短临界区内检查，防止检查后被新请求抢占。 */
bool measure_result_is_current(const measure_result_t* result);

/** 最近实际发出的测距指令编号，测量中重触发也产生新编号。 */
uint32_t measure_round_id(void);

/** 有新发布时返回 true 并清除标志（app 据此刷新显示/计数/坐标） */
bool measure_take_published(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MEASURE_H */

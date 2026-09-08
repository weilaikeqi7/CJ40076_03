/**
 * @file app.c
 * @brief CJ40076 V3 应用主状态机实现
 */
#include "app.h"

#include "app_attitude.h"
#include "app_calib.h"
#include "app_config.h"
#include "app_display.h"
#include "app_key.h"
#include "app_measure.h"
#include "app_store.h"
#include "board.h"
#include "board_adc.h"
#include "board_uart.h"
#include "gnss.h"
#include "jy901b.h"
#include "lcd.h"
#include "ranger.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* 模式循环顺序：单次 -> 连续 -> 多功能 -> 测试 -> 单次 */
static const meas_mode_t mode_cycle[] = {
    MEAS_MODE_SINGLE, MEAS_MODE_CONT, MEAS_MODE_MULTI, MEAS_MODE_TEST,
};

static meas_mode_t cur_mode = MEAS_MODE_SINGLE;

/* 多功能/测试模式的目标坐标（最近一次发布） */
static app_geo_point_t target_near;
static app_geo_point_t target_far;
static bool            target_published; /* 已进入 TARGET 显示 */
static bool            round_was_active;

static uint32_t batt_mv   = 4200U;
static uint8_t  batt_lvl  = 4U;
static bool     imu_on    = false;
static bool     gnss_on   = false;

/* ------------------------------ 供电调度 ------------------------------ */

static void power_apply(void)
{
    bool need_imu;
    bool need_gnss;

    if (calib_page_active() || calib_mag_in_progress())
    {
        /* 校准页/磁场校准：GNSS 关、IMU 保 */
        need_imu  = true;
        need_gnss = false;
    }
    else if (cur_mode == MEAS_MODE_MULTI || cur_mode == MEAS_MODE_TEST)
    {
        need_imu  = true;
        need_gnss = true;
    }
    else
    {
        /* 单次/连续：仅基础测距，IMU/GNSS 关闭（测距机常开） */
        need_imu  = false;
        need_gnss = false;
    }

    if (need_imu != imu_on)
    {
        board_jy901b_power(need_imu);
        imu_on = need_imu;
        if (need_imu)
        {
            /* 重新上电后等待模块启动，重新配置输出（垂直安装保持内部 Flash 值） */
            vTaskDelay(pdMS_TO_TICKS(300U));
            board_uart_flush_rx(BOARD_UART_JY901B);
        }
    }

    if (need_gnss != gnss_on)
    {
        board_gnss_power(need_gnss);
        gnss_on = need_gnss;
        if (need_gnss)
        {
            vTaskDelay(pdMS_TO_TICKS(100U));
            board_uart_flush_rx(BOARD_UART_GNSS);
        }
    }
}

/* ------------------------------ 关机 ------------------------------ */

static void shutdown_proc(void)
{
    /* 磁场校准中长按关机 = 放弃本轮（不发送结束命令） */
    if (calib_mag_in_progress())
    {
        LOGI("calib: mag calibration aborted by power off\r\n");
    }

    LOGI("sys: power off, save count=%lu\r\n", (unsigned long)store_get_count());
    (void)store_save_count(); /* 正常关机与欠压关机均保存计数 */

    lcd_power_off();          /* 先 DISP=0 再断屏电 */
    board_ranger_power(false);
    board_jy901b_power(false);
    board_gnss_power(false);
    board_heater(false);

    board_power_hold(false);  /* 切断整机电源 */
    while (1)
    {
    }
}

/* ------------------------------ 电池 ------------------------------ */

static void battery_check(void)
{
    batt_mv = board_battery_mv();

    if (batt_mv >= APP_BATT_LVL4_MV)
    {
        batt_lvl = 4U;
    }
    else if (batt_mv >= APP_BATT_LVL3_MV)
    {
        batt_lvl = 3U;
    }
    else if (batt_mv >= APP_BATT_LVL2_MV)
    {
        batt_lvl = 2U;
    }
    else
    {
        batt_lvl = 1U;
    }

    if (batt_mv < APP_BATT_LOW_OFF_MV)
    {
        LOGI("sys: low battery %lumV, power off\r\n", (unsigned long)batt_mv);
        shutdown_proc();
    }
}

/* ------------------------------ 按键分发 ------------------------------ */

static void mode_switch_next(void)
{
    uint8_t i;

    for (i = 0U; i < (uint8_t)(sizeof(mode_cycle) / sizeof(mode_cycle[0])); i++)
    {
        if (mode_cycle[i] == cur_mode)
        {
            cur_mode = mode_cycle[(i + 1U) % 4U];
            break;
        }
    }

    measure_set_mode(cur_mode);
    target_published = false;
    power_apply();
    LOGI("app: mode -> %d\r\n", (int)cur_mode);
}

static void on_measure_published(void)
{
    const measure_result_t* res = measure_get_result();
    uint32_t              count = store_get_count() + 1U;

    store_set_count_ram(count); /* 计数 +1（RAM），关机/清零时才落 Flash */

    if (cur_mode == MEAS_MODE_MULTI || cur_mode == MEAS_MODE_TEST)
    {
        /* 取测量完成附近的姿态/定位快照解算目标坐标 */
        app_geo_point_t self;

        target_published = true;
        target_near.valid = false;
        target_far.valid  = false;

        if (res->near_valid && attitude_valid() && coord_get_self(&self))
        {
            coord_compute_target(&self, (float)res->near_mm / 1000.0f, attitude_heading_c01(),
                                 attitude_pitch_c01(), &target_near);
            if (res->far_valid)
            {
                coord_compute_target(&self, (float)res->far_mm / 1000.0f, attitude_heading_c01(),
                                     attitude_pitch_c01(), &target_far);
            }
        }

        LOGI("app: multi published, near_valid=%d target_valid=%d\r\n", (int)res->near_valid,
             (int)target_near.valid);
    }
}

static void handle_key(const app_key_event_t* evt)
{
    /* 长按关机优先级最高（任何页面），磁场校准中即为放弃 */
    if ((evt->evt & APP_KEY_EVT_POWER_LONG) != 0U)
    {
        shutdown_proc();
    }

    /* 校准模块优先消费（多击 4~9、页内调节、双键） */
    if (calib_handle_key(evt))
    {
        /* 进入校准页/校准流程：停止测距（手册 KEY-03） */
        if (calib_get_state() != CALIB_NONE && measure_is_running())
        {
            measure_stop();
        }
        /* 进入/退出校准页可能改变供电需求 */
        power_apply();
        return;
    }

    /* ---------- 正常模式 ---------- */
    if ((evt->evt & APP_KEY_EVT_POWER_SHORT) != 0U)
    {
        measure_trigger();
        return;
    }

    if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U)
    {
        if (evt->arg == 1U)
        {
            /* 单击切模式；测试运行中忽略 */
            if (!(cur_mode == MEAS_MODE_TEST && measure_is_running()))
            {
                mode_switch_next();
            }
        }
        else if (evt->arg == 3U)
        {
            /* 三击：计数清零并立即写 Flash（本版规则） */
            store_set_count_ram(0U);
            if (store_save_count())
            {
                LOGI("app: count cleared and saved\r\n");
            }
        }
        else
        {
            /* 二击及其余：无动作 */
        }
    }
}

/* ------------------------------ 启动序列 ------------------------------ */

static void startup_self_check(void)
{
    uint32_t start = xTaskGetTickCount();

    /* JY901B：上电 + 配置（5Hz、仅角度帧、垂直安装） */
    jy901b_init(JY901B_RATE_5HZ, (uint16_t)JY901B_RSW_ANGLE);
    imu_on = true;

    /* 角度帧自检：3s 内等到第一帧 */
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(3000U))
    {
        attitude_update();
        if (attitude_valid())
        {
            LOGI("sys: JY901B angle frame self-check OK\r\n");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20U));
    }
    LOGI("sys: JY901B self-check FAILED (no angle frame)\r\n");
}

/* ------------------------------ 主任务 ------------------------------ */

void app_run(void* argument)
{
    (void)argument;
    uint32_t     render_ms = 0U;
    uint32_t     batt_ms   = 0U;
    disp_state_t disp;

    rtt_log_init();
    LOGI("sys: CJ40076 V3 boot\r\n");

    board_adc_init();
    store_init();
    attitude_load_offsets();

    display_init();   /* LCD 上电 */
    ranger_init();    /* 测距机常供电（含 1.6s 预热） */
    gnss_init();      /* 初始化 USART1 后先断电（按需供电） */
    board_gnss_power(false);
    startup_self_check(); /* JY901B 配置 + 角度帧自检 */

    app_key_init();

    /* 初始模式 = 单次：IMU/GNSS 按策略关闭 */
    power_apply();

    while (1)
    {
        app_key_event_t evt = app_key_scan();

        if (evt.evt != APP_KEY_EVT_NONE)
        {
            handle_key(&evt);
        }

        measure_poll();
        attitude_update();
        if (gnss_on)
        {
            gnss_poll();
        }

        /* 新一轮开始：清坐标显示回 LOCAL */
        if (measure_round_active() && !round_was_active)
        {
            target_published = false;
        }
        round_was_active = measure_round_active();

        if (measure_take_published())
        {
            on_measure_published();
        }

        /* 100ms 渲染 */
        render_ms += APP_KEY_SCAN_MS;
        if (render_ms >= APP_DISP_RENDER_MS)
        {
            render_ms = 0U;

            memset(&disp, 0, sizeof(disp));
            disp.mode      = cur_mode;
            disp.measuring = measure_round_active();
            disp.result    = measure_get_result();

            disp.att_valid   = attitude_valid();
            disp.heading_c01 = attitude_heading_c01();
            disp.pitch_c01   = attitude_pitch_c01();

            disp.self_valid = coord_get_self(&disp.self);

            disp.target_valid = target_published;
            disp.target_near  = target_near;
            disp.target_far   = target_far;

            disp.count      = store_get_count();
            disp.batt_level = batt_lvl;

            switch (calib_get_state())
            {
            case CALIB_PIT:
                disp.page = DISP_PAGE_PIT;
                break;
            case CALIB_HIT:
                disp.page = DISP_PAGE_HIT;
                break;
            case CALIB_HER:
                disp.page = DISP_PAGE_HER;
                break;
            case CALIB_MAG:
            case CALIB_ACC_BUSY:
            case CALIB_ANG_BUSY:
                disp.page = DISP_PAGE_FULL_ON;
                break;
            default:
                disp.page = DISP_PAGE_NONE;
                break;
            }
            disp.page_value_c01 = calib_page_value_c01();

            display_render(&disp);
        }

        /* 500ms 电池检查 */
        batt_ms += APP_KEY_SCAN_MS;
        if (batt_ms >= APP_BATT_CHECK_MS)
        {
            batt_ms = 0U;
            battery_check();
        }

        vTaskDelay(pdMS_TO_TICKS(APP_KEY_SCAN_MS));
    }
}

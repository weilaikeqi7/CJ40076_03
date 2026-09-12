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

/* 激光测距发起瞬间的姿态快照（锁定瞄准瞬间的真实空间朝向，消除收尾静默期间手抖误差） */
static int32_t         trigger_heading_c01;
static int32_t         trigger_pitch_c01;
static bool            trigger_att_valid;

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
    /* 滞环回差（mV）：防止电池开路电压在分档临界点抖动跳格 */
    const uint32_t HYST_MV = 30U;
    batt_mv = board_battery_mv();

    if (batt_lvl == 0U)
    {
        /* 初次采样无滞环初始化 */
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
    }
    else
    {
        /* 带回差的分档判定：升级需额外超出回差门限，降级保持原门限 */
        if (batt_mv >= (APP_BATT_LVL4_MV + (batt_lvl < 4U ? HYST_MV : 0U)))
        {
            batt_lvl = 4U;
        }
        else if (batt_mv >= (APP_BATT_LVL3_MV + (batt_lvl < 3U ? HYST_MV : 0U)))
        {
            batt_lvl = 3U;
        }
        else if (batt_mv >= (APP_BATT_LVL2_MV + (batt_lvl < 2U ? HYST_MV : 0U)))
        {
            batt_lvl = 2U;
        }
        else
        {
            batt_lvl = 1U;
        }
    }

    if (batt_mv < APP_BATT_LOW_OFF_MV)
    {
        LOGI("sys: low battery %lumV, power off\r\n", (unsigned long)batt_mv);
        shutdown_proc();
    }
}

/* ------------------------------ 极低温屏幕自适应加热 ------------------------------ */

static void heater_temperature_control_step(void)
{
    int16_t  temp_c10 = board_ntc_temperature_c10();
    uint16_t duty     = 0U;

    /* 1. 电池跌落自保护：带载电压低于安全门限强制断开加热，保住主控供电不复位 */
    if (batt_mv < APP_HEATER_VBAT_SAFE_MV)
    {
        board_heater_set_duty(0U);
        return;
    }

    /* 2. 温度分级闭环调节 */
    if (temp_c10 >= APP_HEATER_TEMP_OFF_C10)
    {
        /* 超过 15.0℃：彻底关闭加热 */
        duty = 0U;
    }
    else if (temp_c10 >= APP_HEATER_TEMP_WARM_C10)
    {
        /* 0.0℃ ~ 15.0℃：维持期，15% 占空比 */
        duty = APP_HEATER_DUTY_KEEP_WARM;
    }
    else if (temp_c10 >= APP_HEATER_TEMP_COLD_C10)
    {
        /* -15.0℃ ~ 0.0℃：快速升温期，35% 占空比 */
        duty = APP_HEATER_DUTY_WARM_UP;
    }
    else
    {
        /* <-15.0℃（极寒至 -40℃）：根据电池充裕程度自适应 */
        if (batt_mv >= APP_HEATER_VBAT_RICH_MV)
        {
            duty = APP_HEATER_DUTY_COLD_HIGH; /* 电量充足：40% 快速破冰升温 */
        }
        else
        {
            duty = APP_HEATER_DUTY_COLD_LOW;  /* 电量中等：20% 温和预热唤醒，防止拉垮高内阻电池 */
        }
    }

    board_heater_set_duty(duty);
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
        /* 优先使用开火/触发瞬间捕获的姿态快照，消除测距收尾静默期间手抖引起的方位角漂移 */
        app_geo_point_t self;
        bool att_ok = trigger_att_valid ? trigger_att_valid : attitude_valid();
        int32_t use_heading = trigger_att_valid ? trigger_heading_c01 : attitude_heading_c01();
        int32_t use_pitch   = trigger_att_valid ? trigger_pitch_c01 : attitude_pitch_c01();

        target_near.valid = false;
        target_far.valid  = false;

        /* 使用局部变量在栈上计算完成，绝不在共享变量上“边算边写” */
        app_geo_point_t loc_near;
        app_geo_point_t loc_far;
        bool            loc_pub = false;
        loc_near.valid = false;
        loc_far.valid  = false;

        if (res->near_valid && att_ok && coord_get_self(&self))
        {
            loc_pub = true;
            coord_compute_target(&self, (float)res->near_mm / 1000.0f, use_heading,
                                 use_pitch, &loc_near);
            if (res->far_valid)
            {
                coord_compute_target(&self, (float)res->far_mm / 1000.0f, use_heading,
                                     use_pitch, &loc_far);
            }
        }

        /* 临界区原子提交：耗时 < 0.2 微秒，确保读取端绝对不会读到“写了一半”的数据 */
        taskENTER_CRITICAL();
        target_near      = loc_near;
        target_far       = loc_far;
        target_published = loc_pub;
        taskEXIT_CRITICAL();

        LOGI("app: multi published, near_valid=%d target_valid=%d (snapshot_att=%d)\r\n", (int)res->near_valid,
             (int)target_near.valid, (int)trigger_att_valid);
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


/* ========================================================================== */
/*                       FreeRTOS 4 大专业并发任务实现                        */
/* ========================================================================== */

/* 系统启动与核心外设硬件自检 */
void app_system_init(void)
{
    rtt_log_init();
    LOGI("sys: CJ40076 4-Tasks RTOS boot\r\n");

    board_adc_init();
    store_init();
    attitude_load_offsets();

    display_init();   /* 屏幕上电初始化 */
    ranger_init();    /* 测距机常开供电（含 1.6s 预热启动） */
    gnss_init();      /* GNSS 初始化后按策略待机 */
    board_gnss_power(false);

    startup_self_check(); /* JY901B 配置 + 角度帧等待自检 */
    app_key_init();

    power_apply();    /* 初始模式供电策略 */
}

/* -------------------------------------------------------------------------- */
/* Task 1: 人机交互与按键即时响应任务 (优先级 4, 10ms 周期)                  */
/* -------------------------------------------------------------------------- */
void app_task_key(void* argument)
{
    (void)argument;
    LOGI("task: T_KEY started (Prio 4)\r\n");

    while (1)
    {
        app_key_event_t evt = app_key_scan();
        if (evt.evt != APP_KEY_EVT_NONE)
        {
            handle_key(&evt);
        }
        vTaskDelay(pdMS_TO_TICKS(APP_KEY_SCAN_MS));
    }
}

/* -------------------------------------------------------------------------- */
/* Task 2: 传感器采集与空间三角经纬度投影解算任务 (优先级 3, 10ms 周期)      */
/* -------------------------------------------------------------------------- */
void app_task_sensor(void* argument)
{
    (void)argument;
    LOGI("task: T_SENS started (Prio 3)\r\n");

    while (1)
    {
        /* 1. 激光测距机：常开供电，始终轮询命令与回包状态机 */
        measure_poll();

        /* 2. 姿态传感器：仅在开启供电（多功能/测试/校准）时轮询更新 */
        if (imu_on)
        {
            attitude_update();
        }

        /* 3. 卫星定位模块：仅在开启供电（多功能/测试）时轮询解析 NMEA */
        if (gnss_on)
        {
            gnss_poll();
        }

        /* 边沿检测：新一轮测距开始，清空上一轮 TARGET 状态并瞬态锁定开火姿态快照 */
        if (measure_round_active() && !round_was_active)
        {
            target_published    = false;
            trigger_att_valid   = attitude_valid();
            trigger_heading_c01 = attitude_heading_c01();
            trigger_pitch_c01   = attitude_pitch_c01();
        }
        round_was_active = measure_round_active();

        if (measure_take_published())
        {
            on_measure_published();
        }

        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

/* -------------------------------------------------------------------------- */
/* Task 3: 屏幕画面刷新 (100ms) 与极低温自适应闭环加热 (1s) (优先级 2)       */
/* -------------------------------------------------------------------------- */
void app_task_display(void* argument)
{
    (void)argument;
    uint32_t     heater_ms = 0U;
    disp_state_t disp;
    LOGI("task: T_DISP started (Prio 2)\r\n");

    while (1)
    {
        /* 1. 100ms 构造显示状态并渲染：临界区原子抓取快照（耗时 < 0.5 微秒） */
        memset(&disp, 0, sizeof(disp));
        disp.self_valid  = coord_get_self(&disp.self);

        taskENTER_CRITICAL();
        disp.mode        = cur_mode;
        disp.measuring   = measure_round_active();
        disp.result      = measure_get_result();
        disp.att_valid   = attitude_valid();
        disp.heading_c01 = attitude_heading_c01();
        disp.pitch_c01   = attitude_pitch_c01();
        disp.target_valid= target_published;
        disp.target_near = target_near;
        disp.target_far  = target_far;
        disp.count       = store_get_count();
        disp.batt_level  = batt_lvl;
        taskEXIT_CRITICAL();

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

        /* 2. 1000ms 自适应闭环加热 */
        heater_ms += APP_DISP_RENDER_MS;
        if (heater_ms >= APP_HEATER_CTRL_PERIOD_MS)
        {
            heater_ms = 0U;
            heater_temperature_control_step();
        }

        vTaskDelay(pdMS_TO_TICKS(APP_DISP_RENDER_MS));
    }
}

/* -------------------------------------------------------------------------- */
/* Task 4: 电池电压检测、1Hz闪烁监控与欠压紧急断电任务 (优先级 1, 500ms 周期) */
/* -------------------------------------------------------------------------- */
void app_task_power(void* argument)
{
    (void)argument;
    LOGI("task: T_PWR started (Prio 1)\r\n");

    while (1)
    {
        battery_check();
        vTaskDelay(pdMS_TO_TICKS(APP_BATT_CHECK_MS));
    }
}

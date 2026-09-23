/**
 * @file app.c
 * @brief CJ40076 V3 应用核心调度器与多任务并发实现
 */
#include "app.h"

#include "app_attitude.h"
#include "app_calib.h"
#include "app_config.h"
#include "app_display.h"
#include "app_key.h"
#include "app_measure.h"
#include "app_power.h"
#include "app_store.h"
#include "app_thermal.h"
#include "bsp_adc.h"
#include "bsp_iwdg.h"
#include "dev_compass.h"
#include "dev_display.h"
#include "dev_gnss.h"
#include "dev_ranger.h"

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
static uint32_t        displayed_round_id;

static bool compass_on = false;
static bool gnss_on    = false;

/* ------------------------------ 看门狗与任务保活 ------------------------------ */

/** 独立看门狗超时时间配置（2500ms，允许各任务抢占延迟抖动） */
#define APP_IWDG_TIMEOUT_MS 2500U

/** 4 大并发任务打卡位掩码定义 */
#define TASK_ALIVE_BIT_KEY   (1U << 0)
#define TASK_ALIVE_BIT_SENS  (1U << 1)
#define TASK_ALIVE_BIT_DISP  (1U << 2)
#define TASK_ALIVE_BIT_PWR   (1U << 3)
#define TASK_ALIVE_ALL       (TASK_ALIVE_BIT_KEY | TASK_ALIVE_BIT_SENS | TASK_ALIVE_BIT_DISP | TASK_ALIVE_BIT_PWR)

static volatile uint32_t s_task_alive_bits = 0U;

static void app_mark_alive(uint32_t bit)
{
    taskENTER_CRITICAL();
    s_task_alive_bits |= bit;
    taskEXIT_CRITICAL();
}

static inline void app_mark_key_alive(void)  { app_mark_alive(TASK_ALIVE_BIT_KEY); }
static inline void app_mark_sens_alive(void) { app_mark_alive(TASK_ALIVE_BIT_SENS); }
static inline void app_mark_disp_alive(void) { app_mark_alive(TASK_ALIVE_BIT_DISP); }
static inline void app_mark_pwr_alive(void)  { app_mark_alive(TASK_ALIVE_BIT_PWR); }

static uint32_t app_take_alive_bits(void)
{
    uint32_t bits;
    taskENTER_CRITICAL();
    bits = s_task_alive_bits;
    if ((bits & TASK_ALIVE_ALL) == TASK_ALIVE_ALL)
    {
        s_task_alive_bits = 0U;
    }
    taskEXIT_CRITICAL();
    return bits;
}

/* ------------------------------ 供电调度 ------------------------------ */

static void power_apply(void)
{
    bool need_compass;
    bool need_gnss;

    if (app_power_is_shutting_down()) return;

    if (calib_page_active())
    {
        /* 校准页/磁场校准：GNSS 关、电子罗盘保 */
        need_compass = true;
        need_gnss    = false;
    }
    else if (cur_mode == MEAS_MODE_MULTI || cur_mode == MEAS_MODE_TEST)
    {
        need_compass = true;
        need_gnss    = true;
    }
    else
    {
        /* 单次/连续：仅基础测距，罗盘/GNSS 关闭（测距机常开） */
        need_compass = false;
        need_gnss    = false;
    }

    if (need_compass != compass_on)
    {
        compass_power_ctl(need_compass);
        compass_on = need_compass;
    }

    if (need_gnss != gnss_on)
    {
        gnss_power_ctl(need_gnss);
        gnss_on = need_gnss;
    }
}

/* ------------------------------ 模式与测量分发 ------------------------------ */

static void mode_switch_next(void)
{
    uint8_t i;

    for (i = 0U; i < (uint8_t)(sizeof(mode_cycle) / sizeof(mode_cycle[0])); i++)\
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
}

static void on_measure_published(const measure_result_t* res)
{
    taskENTER_CRITICAL();
    if (!measure_result_is_current(res))
    {
        taskEXIT_CRITICAL();
        return;
    }
    store_set_count_ram(store_get_count() + 1U); /* 与按键清零互斥 */
    taskEXIT_CRITICAL();

    if (res->mode == MEAS_MODE_MULTI || res->mode == MEAS_MODE_TEST)
    {
        /* 优先使用开火/触发瞬间捕获的姿态快照，消除测距收尾静默期间手抖引起的方位角漂移 */
        app_geo_point_t self;
        bool att_ok = res->attitude_valid;
        int32_t use_heading = res->heading_c01;
        int32_t use_pitch   = res->pitch_c01;

        /* 使用局部变量在私有栈上计算完成，绝不在共享变量上“边算边写” */
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
        if (measure_result_is_current(res))
        {
            target_near      = loc_near;
            target_far       = loc_far;
            target_published = loc_pub;
        }
        taskEXIT_CRITICAL();

    }
}

static void handle_key(const app_key_event_t* evt)
{
    /* 长按关机优先级最高（任何页面）：触发整机软关机下电流程 */
    if ((evt->evt & APP_KEY_EVT_POWER_LONG) != 0U)
    {
        app_power_shutdown();
    }

    /* 校准按键仅提交状态与命令，停止测距并应用供电策略后再执行。 */
    if (calib_handle_key(evt))
    {
        /* 进入校准页/校准流程：停止测距 */
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
            /* 三击：计数清零并立即写 Flash */
            store_set_count_ram(0U);
            if (store_save_count())
            {
            }
        }
    }
}

/* ------------------------------ 启动与自检 ------------------------------ */

static void startup_self_check(void)
{
    /* 按编译时选择的罗盘型号配置安装方向、输出内容与广播速率。 */
    compass_init();
    compass_on = true;

    if (compass_self_check(3000U))
    {
    }
    else
    {
    }
}

void app_system_init(void)
{

    bsp_adc_init();
    store_init();
    app_power_init();   /* 初始化电源与电池状态 */
    app_thermal_init(); /* 初始化热管理 */
    attitude_load_offsets();

    display_init();     /* 屏幕设备上电初始化 */
    ranger_init();      /* 测距机常开供电（含 1.6s 预热启动） */
    gnss_init();        /* GNSS 初始化后按策略待机 */
    gnss_power_ctl(false);

    startup_self_check(); /* 罗盘设备配置 + 角度自检 */
    app_key_init();

    power_apply();      /* 初始模式供电策略 */

    /* 启动硬件独立看门狗，进入全系统协同监控保活模式 */
    bsp_iwdg_init(APP_IWDG_TIMEOUT_MS);
}

/* ========================================================================== */
/*                       FreeRTOS 4 大专业并发任务实现                        */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Task 1: 人机交互与按键即时响应任务 (优先级 4, 10ms 周期)                  */
/* -------------------------------------------------------------------------- */
void app_task_key(void* argument)
{
    (void)argument;

    while (1)
    {
        app_mark_key_alive(); /* T_KEY 任务健康打卡 */

        if (app_power_is_shutting_down())
        {
            vTaskDelay(pdMS_TO_TICKS(APP_KEY_SCAN_MS));
            continue;
        }

        app_key_event_t evt = app_key_scan();
        if (evt.evt != APP_KEY_EVT_NONE)
        {
            handle_key(&evt);
        }
        /* 先建立业务状态，再应用供电并推进校准；完成后重新评估是否断电。 */
        power_apply();
        compass_step(); /* 上电配置独立推进，不因测距进行中而停滞 */
        if (!app_power_is_shutting_down() && !measure_round_active())
        {
            calib_step();
        }
        power_apply();
        vTaskDelay(pdMS_TO_TICKS(APP_KEY_SCAN_MS));
    }
}

/* -------------------------------------------------------------------------- */
/* Task 2: 传感器采集与空间三角经纬度投影解算任务 (优先级 3, 10ms 周期)      */
/* -------------------------------------------------------------------------- */
void app_task_sensor(void* argument)
{
    (void)argument;

    while (1)
    {
        app_mark_sens_alive(); /* T_SENS 任务健康打卡 */
        if (app_power_is_shutting_down())
        {
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }

        /* 1. 激光测距机：常开供电，始终轮询命令与回包状态机 */
        measure_poll();

        /* 2. 电子罗盘：仅在开启供电（多功能/测试/校准）时轮询更新 */
        if (compass_on)
        {
            attitude_update();
        }

        /* 3. 卫星定位模块：仅在开启供电（多功能/测试）时轮询解析 NMEA */
        if (gnss_on)
        {
            gnss_poll();
        }

        /* 每次真正发送测距指令都产生新轮次号，活动中重触发也会更新。 */
        uint32_t round_id = measure_round_id();
        if (round_id != displayed_round_id)
        {
            target_published = false;
            displayed_round_id = round_id;
        }

        measure_result_t result;
        if (measure_take_result(&result))
        {
            on_measure_published(&result);
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
    measure_result_t display_result;

    while (1)
    {
        app_mark_disp_alive(); /* T_DISP 任务健康打卡 */
        if (app_power_is_shutting_down())
        {
            vTaskDelay(pdMS_TO_TICKS(APP_DISP_RENDER_MS));
            continue;
        }

        /* 1. 100ms 构造显示状态并渲染：临界区原子抓取快照（耗时 < 0.5 微秒） */
        memset(&disp, 0, sizeof(disp));
        disp.self_valid  = coord_get_self(&disp.self);

        taskENTER_CRITICAL();
        disp.mode        = cur_mode;
        disp.measuring   = measure_round_active();
        measure_copy_result(&display_result);
        disp.result      = &display_result;
        disp.att_valid   = attitude_valid();
        disp.heading_c01 = attitude_heading_c01();
        disp.pitch_c01   = attitude_pitch_c01();
        disp.target_valid= target_published;
        disp.target_near = target_near;
        disp.target_far  = target_far;
        disp.count       = store_get_count();
        disp.batt_level  = app_power_get_batt_lvl();

        switch (calib_get_state())
        {
        case CALIB_PIT:
            disp.page           = DISP_PAGE_PIT;
            disp.page_value_c01 = calib_page_value_c01();
            break;
        case CALIB_HIT:
            disp.page           = DISP_PAGE_HIT;
            disp.page_value_c01 = calib_page_value_c01();
            break;
        case CALIB_HER:
            disp.page           = DISP_PAGE_HER;
            disp.page_value_c01 = calib_page_value_c01();
            break;
        case CALIB_MAG:
            if (compass_mag_uses_samples())
            {
                compass_cal_state_t cal_snapshot;
                compass_get_cal_state_snapshot(&cal_snapshot);
                const compass_cal_state_t* cs = &cal_snapshot;
                disp.page = DISP_PAGE_MAG_CAL;
                disp.cal_cur_samples = cs->sample_count;
                disp.cal_total_samples = APP_MAG_CAL_TOTAL_SAMPLES;
                disp.cal_score = cs->cal_score;
                disp.cal_score_valid = cs->score_valid;
            }
            else
            {
                disp.page = DISP_PAGE_FULL_ON;
            }
            break;
        case CALIB_ACC_BUSY:
        case CALIB_ANG_BUSY:
        case CALIB_FACTORY_BUSY:
            disp.page = DISP_PAGE_FULL_ON;
            break;
        default:
            disp.page = DISP_PAGE_NONE;
            break;
        }
        taskEXIT_CRITICAL();

        display_render(&disp);

        /* 2. 1000ms 极低温热管理自适应闭环温控 */
        heater_ms += APP_DISP_RENDER_MS;
        if (heater_ms >= APP_HEATER_CTRL_PERIOD_MS)
        {
            heater_ms = 0U;
            app_thermal_step(app_power_get_batt_mv());
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

    while (1)
    {
        /* 1. T_PWR 自身打卡 */
        app_mark_pwr_alive();

        /* 2. 4 大并发业务任务协同喂狗判定：仅当全部任务在周期内正常调度打卡，才执行硬件喂狗 */
        uint32_t alive_bits = app_take_alive_bits();
        if ((alive_bits & TASK_ALIVE_ALL) == TASK_ALIVE_ALL)
        {
            bsp_iwdg_feed();        /* 4 任务均健康存活，喂狗重装倒计数 */
        }
        else
        {
            /* 任一任务卡死/阻塞/挂起时，故意停止喂狗，等待硬件看门狗在 2.5 秒后强行复位重启 */
        }

        app_power_check();
        vTaskDelay(pdMS_TO_TICKS(APP_BATT_CHECK_MS));
    }
}

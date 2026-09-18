/**
 * @file app_calib.c
 * @brief 校准与补偿设置状态机实现（适配 MCG505 型全国产三维电子罗盘）
 */
#include "app_calib.h"

#include "app_attitude.h"
#include "app_config.h"
#include "dev_compass.h"
#include "dev_display.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

static calib_state_t state = CALIB_NONE;
static app_offsets_t work; /* 页内编辑中的补偿值（实时生效，保存才落 Flash） */

/* 磁场空间手动校准状态变量 */
static uint16_t s_cal_cur_samples   = 1U;
static uint16_t s_cal_total_samples = 12U;
static float    s_cal_score         = -1.0f;
static bool     s_cal_done          = false;

/* 进入校准前确保电子罗盘已供电并处于就绪状态 */
static void ensure_compass_on(void)
{
    mcg505_power_ctl(true);
}

static bool is_score_normal(float score)
{
    /*
     * MCG505 手册评分标准：
     *   < 0.168 优，0.168~0.248 良，0.248~0.328 中，0.328~0.36 差
     *   > 0.36 或 99.9 分为外界强干扰异常得分
     */
    return (score > 0.0f && score <= 0.36f);
}

static int16_t* page_ptr(void)
{
    switch (state)
    {
    case CALIB_PIT:
        return &work.pit_c01;
    case CALIB_HIT:
        return &work.hit_c01;
    case CALIB_HER:
        return &work.her_c01;
    default:
        return &work.her_c01;
    }
}

static int16_t page_max(void)
{
    return state == CALIB_PIT ? APP_PIT_MAX_C01 : APP_HIT_MAX_C01;
}

static void page_enter(calib_state_t page)
{
    attitude_get_offsets(&work);
    state = page;
    app_key_set_calib_mode(true);
    LOGI("calib: enter page %d\r\n", (int)page);
}

static void page_exit(bool save)
{
    if (save)
    {
        (void)store_save_offsets(&work); /* 保存 Flash（attitude RAM 已实时生效） */
        LOGI("calib: saved, exit\r\n");
    }
    else
    {
        attitude_load_offsets(); /* 放弃未保存修改 */
        LOGI("calib: discard, exit\r\n");
    }
    state = CALIB_NONE;
    app_key_set_calib_mode(false);
}

static void page_adjust(int16_t delta_c01)
{
    int16_t* v   = page_ptr();
    int16_t  max = page_max();
    int32_t  nv  = (int32_t)*v + delta_c01;

    if (nv > max)
    {
        nv = max;
    }
    if (nv < -max)
    {
        nv = -max;
    }
    *v = (int16_t)nv;

    attitude_set_offsets(&work); /* 实时生效 */
}

bool calib_handle_key(const app_key_event_t* evt)
{
    /* ---------- 正常运行态：多击入口 ---------- */
    if (state == CALIB_NONE)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U)
        {
            switch (evt->arg)
            {
            case 4: /* 四击：HEr 航向误差补偿页 */
                ensure_compass_on();
                page_enter(CALIB_HER);
                return true;

            case 5: /* 五击：启动磁场空间手动校准（校准页面不全显，保留俯仰与方位显示） */
                ensure_compass_on();
                s_cal_cur_samples   = 1U; /* 手册：发开始校准后罗盘自动采集第1组并输出编号1 */
                s_cal_total_samples = 12U;
                s_cal_score         = -1.0f;
                s_cal_done          = false;

                mcg505_start_mag_cal();
                state = CALIB_MAG;
                /* 注意：磁场校准中不置 calib_mode=true，保持模式键多击检测生效以接收6击退出事件 */
                LOGI("calib: mag space manual calib started (5-clicks), count=1\r\n");
                return true;

            case 6: /* 六击：仅在磁场校准中有效，NONE 态忽略 */
                return true;

            case 7: /* 七击：PIt 俯仰补偿页 */
                ensure_compass_on();
                page_enter(CALIB_PIT);
                return true;

            /* 八击（加速度校准）与九击（角度校准）已彻底删除，不作响应 */

            case 10: /* 十击：MCG505 罗盘与主控补偿恢复出厂设置 */
                ensure_compass_on();
                mcg505_factory_reset();
                work.pit_c01 = APP_DEFAULT_PIT_C01;
                work.hit_c01 = APP_DEFAULT_HIT_C01;
                work.her_c01 = APP_DEFAULT_HER_C01;
                (void)store_save_offsets(&work);
                attitude_set_offsets(&work);
                LOGI("calib: factory reset done (10-clicks)\r\n");
                return true;

            default:
                break;
            }
        }
        return false;
    }

    /* ---------- 磁场空间手动校准中：短按电源键采样、6击退出 ---------- */
    if (state == CALIB_MAG)
    {
        /* 1. 短按电源键：发送单次采样指令（未采样完成时有效） */
        if ((evt->evt & APP_KEY_EVT_POWER_SHORT) != 0U)
        {
            const mcg505_cal_state_t* cs = mcg505_get_cal_state();
            if (cs->score_valid)
            {
                s_cal_done  = true;
                s_cal_score = cs->cal_score;
            }

            if (!s_cal_done)
            {
                mcg505_take_sample();
                LOGI("calib: take sample key pressed\r\n");
            }
            return true;
        }

        /* 2. 六击：退出磁场校准页面 */
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U && evt->arg == 6U)
        {
            const mcg505_cal_state_t* cs = mcg505_get_cal_state();
            if (cs->score_valid)
            {
                s_cal_done  = true;
                s_cal_score = cs->cal_score;
            }

            if (!s_cal_done)
            {
                /* 未采样完成收到 6 击：发送校准停止指令 */
                mcg505_stop_cal();
                LOGI("calib: calib not finished on 6-clicks, stop cal and exit\r\n");
            }
            else
            {
                /* 采样完成收到 6 击：判定校准得分 */
                if (is_score_normal(s_cal_score))
                {
                    /* 得分正常（<=0.36分）：MCG505 内部已自动持久化保存 */
                    LOGI("calib: score normal (score=%d.%02d), calibration valid\r\n",
                         (int)s_cal_score, (int)((s_cal_score - (int)s_cal_score) * 100));
                }
                else
                {
                    /* 得分异常：强磁干扰 */
                    LOGI("calib: score abnormal (score=%d.%02d), calibration invalid\r\n",
                         (int)s_cal_score, (int)((s_cal_score - (int)s_cal_score) * 100));
                }
            }

            state = CALIB_NONE;
            return true;
        }

        return true; /* 磁场校准中消费其他按键（长按关机除外，app层先判） */
    }

    /* ---------- 补偿设置页（PIt/HIt/HEr） ---------- */
    if ((evt->evt & APP_KEY_EVT_BOTH_LONG) != 0U)
    {
        if (state == CALIB_PIT)
        {
            /* PIt -> HIt 切页（不保存，退出时统一保存） */
            state = CALIB_HIT;
            LOGI("calib: PIt -> HIt\r\n");
        }
        else
        {
            page_exit(true); /* HIt/HEr 页双键长按：保存并退出 */
        }
        return true;
    }

    if ((evt->evt & (APP_KEY_EVT_POWER_SHORT | APP_KEY_EVT_POWER_REPEAT)) != 0U)
    {
        page_adjust(10); /* +0.1° */
        return true;
    }
    if ((evt->evt & (APP_KEY_EVT_MODE_SINGLE | APP_KEY_EVT_MODE_REPEAT)) != 0U)
    {
        page_adjust(-10); /* -0.1° */
        return true;
    }

    return true;
}

calib_state_t calib_get_state(void)
{
    return state;
}

int16_t calib_page_value_c01(void)
{
    if (state >= CALIB_PIT && state <= CALIB_HER)
    {
        return *page_ptr();
    }
    return 0;
}

bool calib_mag_in_progress(void)
{
    return state == CALIB_MAG;
}

bool calib_page_active(void)
{
    return state >= CALIB_PIT && state <= CALIB_MAG;
}

uint16_t calib_mag_cur_samples(void)
{
    const mcg505_cal_state_t* cs = mcg505_get_cal_state();
    if (cs->sample_count > s_cal_cur_samples)
    {
        s_cal_cur_samples = (uint16_t)cs->sample_count;
    }
    return s_cal_cur_samples;
}

uint16_t calib_mag_total_samples(void)
{
    return s_cal_total_samples;
}

bool calib_mag_get_score(float* out_score)
{
    const mcg505_cal_state_t* cs = mcg505_get_cal_state();
    if (cs->score_valid)
    {
        s_cal_done  = true;
        s_cal_score = cs->cal_score;
    }

    if (s_cal_done && out_score != NULL)
    {
        *out_score = s_cal_score;
    }
    return s_cal_done;
}

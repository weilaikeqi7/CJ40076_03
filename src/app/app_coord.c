/**
 * @file app_coord.c
 * @brief 目标坐标/高程解算实现
 */
#include "app_coord.h"

#include "app_config.h"
#include "gnss.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>

#define DEG_PER_M_LAT 111320.0 /* 每米对应的纬度数（近似） */

static float c01_to_deg(int32_t c01)
{
    return (float)c01 / 100.0f;
}

void coord_compute_target(const app_geo_point_t* self, float dist_m, int32_t heading_c01,
                          int32_t pitch_c01, app_geo_point_t* out)
{
    float  heading = c01_to_deg(heading_c01) * (float)M_PI / 180.0f;
    float  pitch   = c01_to_deg(pitch_c01) * (float)M_PI / 180.0f;
    float  horiz   = dist_m * cosf(pitch);
    double d_north = (double)(horiz * cosf(heading));
    double d_east  = (double)(horiz * sinf(heading));
    double cos_lat = cos(self->latitude * M_PI / 180.0);

    out->valid = false;
    if (cos_lat < 1e-6 && cos_lat > -1e-6)
    {
        return; /* 极点附近退化（本设备工作范围不会遇到） */
    }

    out->latitude   = self->latitude + d_north / DEG_PER_M_LAT;
    out->longitude  = self->longitude + d_east / (DEG_PER_M_LAT * cos_lat);
    out->altitude_m = self->altitude_m + dist_m * sinf(pitch);
    out->valid      = true;
}

bool coord_get_self(app_geo_point_t* out)
{
    const gnss_data_t* g = gnss_get_data();

    gnss_poll();

    /* 仅校验通过且 fix_quality>0 的 GGA 作为有效定位 */
    if (g->tick_gga == 0U || g->fix_quality == GNSS_FIX_INVALID)
    {
        out->valid = false;
        return false;
    }
    if ((xTaskGetTickCount() - g->tick_gga) >= pdMS_TO_TICKS(APP_GNSS_TIMEOUT_MS))
    {
        out->valid = false;
        return false;
    }

    out->latitude   = g->latitude;
    out->longitude  = g->longitude;
    out->altitude_m = g->altitude_m;
    out->valid      = true;
    return true;
}

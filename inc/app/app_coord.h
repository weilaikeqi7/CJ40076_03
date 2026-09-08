/**
 * @file app_coord.h
 * @brief 目标坐标/高程解算（平面近似，测程 ≤3km 内误差可忽略）
 *
 *   水平距离 = 斜距 * cos(俯仰)
 *   高差     = 斜距 * sin(俯仰)
 *   目标高程 = 本机高程 + 高差
 *   北向偏移 = 水平距离 * cos(航向)，东向偏移 = 水平距离 * sin(航向)
 *   目标纬度 = 本机纬度 + dNorth / 111320
 *   目标经度 = 本机经度 + dEast / (111320 * cos(本机纬度))
 */
#ifndef APP_COORD_H
#define APP_COORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    double  latitude;   /* 度 */
    double  longitude;  /* 度 */
    float   altitude_m; /* 米 */
    bool    valid;
} app_geo_point_t;

/**
 * @brief 由本机坐标 + 斜距 + 姿态推算目标坐标。
 * @param self       本机位置（GNSS 有效定位）
 * @param dist_m     斜距，米
 * @param heading_c01 航向，0.01°（0~35999）
 * @param pitch_c01   俯仰，0.01°（上正下负）
 * @param out        输出目标点
 */
void coord_compute_target(const app_geo_point_t* self, float dist_m, int32_t heading_c01,
                          int32_t pitch_c01, app_geo_point_t* out);

/** 取本机当前位置（GNSS GGA 有效定位，超时内更新才返回 true） */
bool coord_get_self(app_geo_point_t* out);

#ifdef __cplusplus
}
#endif

#endif /* APP_COORD_H */

/**
 * @file dev_compass.h
 * @brief JY901B attitude sensor device adapter
 */
#ifndef DEV_COMPASS_H
#define DEV_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float    heading;
    float    pitch;
    float    roll;
    uint32_t tick_angle;
} mcp406_data_t;

typedef struct
{
    uint32_t sample_count;
    float    cal_score;
    bool     score_valid;
} mcp406_cal_state_t;

void mcp406_power_ctl(bool on);
bool mcp406_self_check(uint32_t timeout_ms);
void mcp406_init(void);
void mcp406_poll(void);
const mcp406_data_t* mcp406_get_data(void);
const mcp406_cal_state_t* mcp406_get_cal_state(void);
bool mcp406_is_alive(uint32_t timeout_ms);
void mcp406_start_mag_cal(void);
void mcp406_take_sample(void);
void mcp406_stop_cal(void);
void mcp406_save(void);
void mcp406_factory_reset(void);
uint16_t mcp406_crc16(const uint8_t* buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* DEV_COMPASS_H */

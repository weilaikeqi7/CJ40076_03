/**
 * @file dev_compass.h
 * @brief Model-neutral compass device interface.
 */
#ifndef DEV_COMPASS_H
#define DEV_COMPASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "compass_config.h"

typedef struct
{
    float heading;       /* Installed heading in degrees, [0, 360). */
    float pitch;         /* Installed pitch in degrees; JY sign converted in driver. */
    float roll;
    uint32_t tick_angle; /* Last complete valid attitude frame, zero if powered off. */
} compass_data_t;

typedef struct
{
    uint32_t sample_count;
    float cal_score;
    bool score_valid; /* Finite score received, not necessarily acceptable to save. */
} compass_cal_state_t;

const char* compass_model_name(void);
void compass_power_ctl(bool on);
void compass_init(void);
void compass_poll(void);
bool compass_self_check(uint32_t timeout_ms);
bool compass_is_alive(uint32_t timeout_ms);
const compass_data_t* compass_get_data(void);

bool compass_mag_uses_samples(void);
const compass_cal_state_t* compass_get_cal_state(void);
void compass_calib_mag_start(void);
void compass_calib_mag_end(void);
void compass_calib_take_sample(void);
bool compass_calib_score_valid(float score);
/* Accepted commands advance from the key task via compass_step(), without long delays. */
bool compass_factory_reset(void);
bool compass_calib_accel(void);
bool compass_calib_angle_ref(void);
bool compass_is_busy(void);
void compass_step(void);

#ifdef __cplusplus
}
#endif

#endif /* DEV_COMPASS_H */

/**
 * @file compass_config.h
 * @brief 编译期罗盘型号选择，供设备协议和应用层默认补偿共用。
 * 可由编译宏覆盖 COMPASS_MODEL；未指定时使用 JY901B，未知值在编译期报错。
 */
#ifndef COMPASS_CONFIG_H
#define COMPASS_CONFIG_H

#define COMPASS_MODEL_JY901B 1
#define COMPASS_MODEL_MCP406 2
#define COMPASS_MODEL_MCG505 3

#ifndef COMPASS_MODEL
#define COMPASS_MODEL COMPASS_MODEL_JY901B
#endif

#if COMPASS_MODEL != COMPASS_MODEL_JY901B && \
    COMPASS_MODEL != COMPASS_MODEL_MCP406 && \
    COMPASS_MODEL != COMPASS_MODEL_MCG505
#error "Unsupported COMPASS_MODEL"
#endif

#endif /* COMPASS_CONFIG_H */

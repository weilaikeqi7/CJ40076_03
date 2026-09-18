/**
 * @file compass_config.h
 * @brief Compile-time compass model selection.
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

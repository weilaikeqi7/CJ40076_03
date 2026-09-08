/**
 * @file app_store.h
 * @brief Flash 参数区：测量计数 + 角度补偿值（PIt/HIt/HEr）
 *
 * 使用内部 Flash 最后一页（512K 型号为 0x0807F800，2KB）。
 * 记录带 magic + CRC16 校验，损坏时回退默认值。
 *
 * 写入时机（本版规则）：
 *   - 三击计数清零：立即写
 *   - 正常长按关机 / 欠压关机：写计数
 *   - 补偿设置页双键长按：写补偿值
 */
#ifndef APP_STORE_H
#define APP_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 角度补偿值（0.01 度单位，有符号） */
typedef struct
{
    int16_t pit_c01;
    int16_t hit_c01;
    int16_t her_c01;
} app_offsets_t;

/** 上电加载：校验通过读 Flash，否则用默认值并覆写 Flash */
void store_init(void);

/** 读取/设置测量计数（RAM 缓存，set 不写 Flash） */
uint32_t store_get_count(void);
void     store_set_count_ram(uint32_t count);

/** 计数立即写 Flash（三击清零用） */
bool store_save_count(void);

/** 读取/保存角度补偿值 */
void store_get_offsets(app_offsets_t* out);
bool store_save_offsets(const app_offsets_t* offsets);

#ifdef __cplusplus
}
#endif

#endif /* APP_STORE_H */

/**
 * @file app_store.c
 * @brief Flash 参数区实现（最后一页，magic + CRC16）
 */
#include "app_store.h"

#include "app_config.h"
#include "rtt_log.h"

#include "n32g4fr.h"

#include <string.h>

#define STORE_PAGE_ADDR 0x0807F800U /* 512KB Flash 最后一页（2KB） */
#define STORE_MAGIC     0x43004A56UL /* "CJ"V 参数区标识 */

typedef struct
{
    uint32_t magic;
    uint32_t count;
    int16_t  pit_c01;
    int16_t  hit_c01;
    int16_t  her_c01;
    uint16_t reserved;
    uint16_t crc16; /* 对 magic..her_c01（不含本字段） CRC16-CCITT */
} store_record_t;

static uint32_t      cache_count;
static app_offsets_t cache_offsets;

static uint16_t crc16_ccitt(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t  b;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (b = 0U; b < 8U; b++)
        {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void store_defaults(void)
{
    cache_count            = 0U;
    cache_offsets.pit_c01  = APP_DEFAULT_PIT_C01;
    cache_offsets.hit_c01  = APP_DEFAULT_HIT_C01;
    cache_offsets.her_c01  = APP_DEFAULT_HER_C01;
}

/** 整页擦除 + 写入当前缓存 */
static bool store_commit(void)
{
    store_record_t rec;
    const uint32_t* src;
    uint32_t        addr;
    uint32_t        i;

    rec.magic    = STORE_MAGIC;
    rec.count    = cache_count;
    rec.pit_c01  = cache_offsets.pit_c01;
    rec.hit_c01  = cache_offsets.hit_c01;
    rec.her_c01  = cache_offsets.her_c01;
    rec.reserved = 0U;
    rec.crc16    = crc16_ccitt((const uint8_t*)&rec, offsetof(store_record_t, crc16));

    FLASH_Unlock();
    if (FLASH_EraseOnePage(STORE_PAGE_ADDR) != FLASH_COMPL)
    {
        FLASH_Lock();
        return false;
    }

    src  = (const uint32_t*)&rec;
    addr = STORE_PAGE_ADDR;
    for (i = 0U; i < sizeof(rec) / sizeof(uint32_t); i++)
    {
        if (FLASH_ProgramWord(addr, src[i]) != FLASH_COMPL)
        {
            FLASH_Lock();
            return false;
        }
        addr += 4U;
    }
    FLASH_Lock();
    return true;
}

void store_init(void)
{
    const store_record_t* rec = (const store_record_t*)STORE_PAGE_ADDR;

    if (rec->magic == STORE_MAGIC &&
        rec->crc16 == crc16_ccitt((const uint8_t*)rec, offsetof(store_record_t, crc16)))
    {
        cache_count            = rec->count;
        cache_offsets.pit_c01  = rec->pit_c01;
        cache_offsets.hit_c01  = rec->hit_c01;
        cache_offsets.her_c01  = rec->her_c01;
        LOGI("store: loaded count=%u pit=%d hit=%d her=%d\r\n", (unsigned int)cache_count,
             (int)cache_offsets.pit_c01, (int)cache_offsets.hit_c01, (int)cache_offsets.her_c01);
    }
    else
    {
        store_defaults();
        LOGI("store: invalid record, defaults loaded\r\n");
        if (!store_commit())
        {
            LOGI("store: failed to init flash\r\n");
        }
    }
}

uint32_t store_get_count(void)
{
    return cache_count;
}

void store_set_count_ram(uint32_t count)
{
    cache_count = count > APP_COUNT_MAX ? APP_COUNT_MAX : count;
}

bool store_save_count(void)
{
    if (!store_commit())
    {
        LOGI("store: failed to save measure count\r\n");
        return false;
    }
    return true;
}

void store_get_offsets(app_offsets_t* out)
{
    *out = cache_offsets;
}

bool store_save_offsets(const app_offsets_t* offsets)
{
    cache_offsets = *offsets;
    if (!store_commit())
    {
        LOGI("store: failed to save angle offsets\r\n");
        return false;
    }
    LOGI("store: offsets saved pit=%d hit=%d her=%d\r\n", (int)offsets->pit_c01,
         (int)offsets->hit_c01, (int)offsets->her_c01);
    return true;
}

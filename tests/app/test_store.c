#include "app_store.h"
#include "app_config.h"
#include "dev_storage.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Match the persisted ABI independently of the production private struct. */
typedef struct
{
    uint32_t magic;
    uint32_t count;
    int16_t pit_c01;
    int16_t hit_c01;
    int16_t her_c01;
    uint16_t model;
    uint16_t crc;
} record_t;

_Static_assert(offsetof(record_t, crc) == 16, "record CRC offset changed");
_Static_assert(sizeof(record_t) == 20, "record size changed");

static unsigned char flash[2048];
static bool fail_write;
static unsigned writes;
void dev_storage_init(void) {}
void dev_storage_read(uint32_t offset, void* buf, size_t len)
{
    assert(offset + len <= sizeof(flash));
    memcpy(buf, flash + offset, len);
}
bool dev_storage_write_record(const void* data, size_t len)
{
    assert(len == sizeof(record_t));
    assert(len % 4 == 0);
    ++writes;
    if (fail_write) return false;
    memset(flash, 0xFF, sizeof(flash));
    memcpy(flash, data, len);
    return true;
}

static uint16_t crc16(const unsigned char* data, size_t size)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
        {
            bool high = (crc & 0x8000U) != 0;
            crc = (uint16_t)(crc << 1);
            if (high) crc ^= 0x1021U;
        }
    }
    return crc;
}

static void fixture(uint16_t model, uint32_t count)
{
    record_t rec = {0};
    rec.magic = 0x43004A56UL;
    rec.count = count;
    rec.pit_c01 = 123;
    rec.hit_c01 = -456;
    rec.her_c01 = 789;
    rec.model = model;
    rec.crc = crc16((const unsigned char*)&rec, offsetof(record_t, crc));
    memcpy(flash, &rec, sizeof(rec));
    writes = 0;
    fail_write = false;
}

static void assert_defaults(uint32_t count)
{
    app_offsets_t value;
    store_get_offsets(&value);
    assert(store_get_count() == count);
    assert(value.pit_c01 == APP_DEFAULT_PIT_C01);
    assert(value.hit_c01 == APP_DEFAULT_HIT_C01);
    assert(value.her_c01 == APP_DEFAULT_HER_C01);
}

static void assert_record(uint32_t count, const app_offsets_t* offsets)
{
    record_t rec;
    memcpy(&rec, flash, sizeof(rec));
    assert(rec.magic == 0x43004A56UL);
    assert(rec.model == COMPASS_MODEL);
    assert(rec.count == count);
    assert(rec.pit_c01 == offsets->pit_c01);
    assert(rec.hit_c01 == offsets->hit_c01);
    assert(rec.her_c01 == offsets->her_c01);
    assert(rec.crc == crc16(flash, offsetof(record_t, crc)));
}

static void test_tagged_and_migration(void)
{
    app_offsets_t value;
    fixture(COMPASS_MODEL, 42);
    store_init();
    store_get_offsets(&value);
    assert(store_get_count() == 42);
    assert(value.pit_c01 == 123 && value.hit_c01 == -456 && value.her_c01 == 789);
    assert(writes == 0);

    /* Every foreign model plus legacy tag zero must preserve count only. */
    for (uint16_t model = 0; model <= 3; ++model)
    {
        if (model == COMPASS_MODEL) continue;
        fixture(model, 135);
        store_init();
        assert_defaults(135);
        assert(writes == 1);
        store_get_offsets(&value);
        assert_record(135, &value);
        store_init();
        assert_defaults(135);
        assert(writes == 1); /* Migration was persisted, not repeated. */
    }

    fixture(COMPASS_MODEL, APP_COUNT_MAX + 100);
    store_init();
    assert(store_get_count() == APP_COUNT_MAX);
    fixture(0, APP_COUNT_MAX + 100);
    store_init();
    assert_defaults(APP_COUNT_MAX);
}

static void test_corruption_and_persistence(void)
{
    app_offsets_t value;
    fixture(COMPASS_MODEL, 333);
    flash[offsetof(record_t, hit_c01)] ^= 1;
    store_init();
    assert_defaults(0);
    assert(writes == 1);
    store_get_offsets(&value);
    assert_record(0, &value);

    fixture(COMPASS_MODEL, 333);
    flash[0] ^= 1; /* Wrong magic. */
    store_init();
    assert_defaults(0);
    assert(writes == 1);

    memset(flash, 0xFF, sizeof(flash));
    store_init();
    assert_defaults(0);
    store_set_count_ram(765);
    assert(store_save_count());
    store_init();
    assert_defaults(765);
    value = (app_offsets_t){-11, 222, -333};
    assert(store_save_offsets(&value));
    assert_record(765, &value);
    store_init();
    app_offsets_t loaded;
    store_get_offsets(&loaded);
    assert(loaded.pit_c01 == -11 && loaded.hit_c01 == 222 && loaded.her_c01 == -333);
    store_set_count_ram(APP_COUNT_MAX + 1);
    assert(store_get_count() == APP_COUNT_MAX);
}

static void test_write_failures(void)
{
    unsigned char before[sizeof(record_t)];
    fixture(COMPASS_MODEL, 777);
    store_init();
    memcpy(before, flash, sizeof(before));
    fail_write = true;
    assert(!store_save_count());
    app_offsets_t offsets = {1, 2, 3};
    assert(!store_save_offsets(&offsets));
    assert(writes == 2);
    assert(memcmp(before, flash, sizeof(before)) == 0);

    fixture(0, 777);
    fail_write = true;
    store_init();
    assert_defaults(777);
    assert(writes == 1);
    record_t rec;
    memcpy(&rec, flash, sizeof(rec));
    assert(rec.model == 0); /* Failed migration leaves old Flash intact. */

    fixture(COMPASS_MODEL, 777);
    flash[0] ^= 1;
    fail_write = true;
    store_init();
    assert_defaults(0);
    assert(writes == 1);
}

int main(void)
{
    test_tagged_and_migration();
    test_corruption_and_persistence();
    test_write_failures();
    printf("app_store model=%d: PASS\n", COMPASS_MODEL);
    return 0;
}

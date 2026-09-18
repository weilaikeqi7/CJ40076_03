/**
 * @file dev_storage.c
 * @brief 片上 Flash 存储器设备实现
 */
#include "dev_storage.h"

#include "bsp_flash.h"

void dev_storage_init(void)
{
    /* Flash 控制器无需额外动态初始化 */
}

void dev_storage_read(uint32_t offset, void* buf, size_t len)
{
    bsp_flash_read(DEV_STORAGE_PAGE_ADDR + offset, buf, len);
}

bool dev_storage_write_record(const void* data, size_t len)
{
    if (data == NULL || len == 0U || (len % 4U) != 0U)
    {
        return false;
    }

    if (!bsp_flash_erase_page(DEV_STORAGE_PAGE_ADDR))
    {
        return false;
    }

    return bsp_flash_write_words(DEV_STORAGE_PAGE_ADDR, (const uint32_t*)data, len / 4U);
}

/**
 * @file board_flash.c
 * @brief N32G4FR 片上 Flash 扇区擦除与编程底层驱动实现
 */
#include "board_flash.h"

#include "n32g4fr.h"

#include <string.h>

void board_flash_read(uint32_t addr, void* buf, size_t len)
{
    if (buf != NULL && len > 0U)
    {
        memcpy(buf, (const void*)addr, len);
    }
}

bool board_flash_erase_page(uint32_t page_addr)
{
    FLASH_STS status;

    FLASH_Unlock();
    status = FLASH_EraseOnePage(page_addr);
    FLASH_Lock();

    return (status == FLASH_COMPL);
}

bool board_flash_write_words(uint32_t addr, const uint32_t* words, size_t word_count)
{
    size_t i;
    FLASH_STS status;

    if (words == NULL || word_count == 0U)
    {
        return false;
    }

    FLASH_Unlock();
    for (i = 0U; i < word_count; i++)
    {
        uint32_t target_addr = addr + (uint32_t)(i * 4U);
        status = FLASH_ProgramWord(target_addr, words[i]);
        if (status != FLASH_COMPL)
        {
            FLASH_Lock();
            return false;
        }

        /* 写入即时回读校验 */
        if (*(volatile uint32_t*)target_addr != words[i])
        {
            FLASH_Lock();
            return false;
        }
    }
    FLASH_Lock();

    return true;
}

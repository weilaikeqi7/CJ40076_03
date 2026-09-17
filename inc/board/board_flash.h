/**
 * @file board_flash.h
 * @brief N32G4FR 片上 Flash 扇区擦除与编程底层驱动
 */
#ifndef BOARD_FLASH_H
#define BOARD_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** N32G4FR 512KB Flash 每页扇区大小为 2KB */
#define BOARD_FLASH_PAGE_SIZE 2048U

/**
 * @brief 从片上 Flash 指定物理地址安全读取数据
 * @param addr 起始物理地址
 * @param buf 接收数据缓冲区
 * @param len 读取字节长度
 */
void board_flash_read(uint32_t addr, void* buf, size_t len);

/**
 * @brief 擦除片上 Flash 指定页（2KB）
 * @param page_addr 页首地址（必须 2KB 对齐，如 0x0807F800）
 * @return true 擦除成功，false 擦除失败
 */
bool board_flash_erase_page(uint32_t page_addr);

/**
 * @brief 向已擦除的片上 Flash 写入 32 位字序列
 * @param addr 目标起始物理地址（必须 4 字节对齐）
 * @param words 待写入的 32 位字数组指针
 * @param word_count 待写入的字数量
 * @return true 写入成功且回读校验通过，false 写入失败
 */
bool board_flash_write_words(uint32_t addr, const uint32_t* words, size_t word_count);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_FLASH_H */

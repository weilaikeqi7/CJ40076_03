/**
 * @file dev_storage.h
 * @brief 片上 Flash 存储器设备抽象（封装扇区擦除、字编程与数据存取）
 *
 * 职责边界：
 *   - 提供面向存储块的读写擦除抽象，屏蔽底层 Flash 物理页结构；
 *   - 底层调用 bsp_flash 驱动，上层供 app_store 参数管理调用。
 */
#ifndef DEV_STORAGE_H
#define DEV_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 参数存储专用页起始物理地址（512KB Flash 最后一页 2KB） */
#define DEV_STORAGE_PAGE_ADDR 0x0807F800U

/**
 * @brief 初始化存储设备
 */
void dev_storage_init(void);

/**
 * @brief 从存储器指定偏移读取数据
 * @param offset 扇区内偏移（字节）
 * @param buf 接收数据缓冲区
 * @param len 读取字节长度
 * @note offset+len 须不超过 2KB 页范围，buf 至少容纳 len 字节；本层不检查越界。
 */
void dev_storage_read(uint32_t offset, void* buf, size_t len);

/**
 * @brief 将整块结构化数据提交写入存储扇区（整页擦除 + 逐字写入 + 回读校验）
 * @param data 待写入的数据指针（必须 4 字节对齐）
 * @param len 待写入字节数（必须为 4 的整数倍）
 * @return true 表示写入及回读校验成功；false 表示参数非法、擦除或编程失败。
 * @note data 非空、len 非零且不超过 2KB；本层不检查页边界，调用方须保证。
 *       调用方须串行化访问；整页更新非原子，失败后旧记录可能已被擦除。
 */
bool dev_storage_write_record(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DEV_STORAGE_H */

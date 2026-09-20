/**
 * @file main.h
 * @brief 主程序公共故障入口，供初始化、断言和 RTOS 钩子复用。
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "n32g4fr.h"

/** 关闭中断后永久停留；不会返回调用者。 */
void Error_Handler(void);
/** file/line 为断言源文件和行号；当前实现忽略二者并进入 Error_Handler。 */
void AppAssertFailed(const char* file, int line);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */

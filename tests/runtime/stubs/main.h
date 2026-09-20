#ifndef TEST_RUNTIME_MAIN_H
#define TEST_RUNTIME_MAIN_H
#include <stdint.h>
#include "n32g4fr.h"
void __disable_irq(void);
void Error_Handler(void);
void AppAssertFailed(const char *file, int line);
#endif

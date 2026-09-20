#ifndef TEST_RUNTIME_N32G4FR_H
#define TEST_RUNTIME_N32G4FR_H
#include <stdint.h>
#define NVIC_PriorityGroup_4 4U
#define tskIDLE_PRIORITY 0U
void NVIC_PriorityGroupConfig(unsigned group);
#endif

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "n32g4fr.h"

/* N32G4FRML-STB V1.1 LED pins from the official GCC_demo. */
#define LED1_PORT GPIOA
#define LED1_PIN  GPIO_PIN_8
#define LED2_PORT GPIOB
#define LED2_PIN  GPIO_PIN_5

void Error_Handler(void);
void AppAssertFailed(const char* file, int line);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */

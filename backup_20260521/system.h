#ifndef __SYSTEM_H
#define __SYSTEM_H

#include <stdint.h>

void SystemClock_Config(void);
void SysTick_Init(void);
void delay_ms(uint32_t ms);
uint32_t millis(void);

#endif /* __SYSTEM_H */
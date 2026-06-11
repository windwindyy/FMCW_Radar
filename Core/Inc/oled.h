#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_PAGES      8

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_Update(void);

#endif

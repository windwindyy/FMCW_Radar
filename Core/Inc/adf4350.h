#ifndef __ADF4350_H
#define __ADF4350_H

#include <stdint.h>

/* R0 - R5 寄存器默认配置宏定义结构 */
typedef struct {
    uint32_t R0;
    uint32_t R1;
    uint32_t R2;
    uint32_t R3;
    uint32_t R4;
    uint32_t R5;
} ADF4350_Regs;

extern ADF4350_Regs adf4350_regs;

void ADF4350_Init(void);
void ADF4350_SetFrequency_kHz(uint32_t freq_khz);

#endif /* __ADF4350_H */

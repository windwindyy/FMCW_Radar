#ifndef __FFT_H
#define __FFT_H

#include <stdint.h>

#define FFT_SIZE        1024
#define ADC_BUF_SIZE    2048   // DMA双缓冲

void FFT_Init(void);
void FFT_Process(const uint16_t *adc_data);
float FFT_GetPeakFreq(void);
float FFT_GetPeakMagnitude(void);

#endif

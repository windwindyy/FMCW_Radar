/**
  自包含 Radix-2 DIT FFT (浮点, 利用Cortex-M4F FPU)
  1024点实数输入 → 512点频谱
*/
#include "stm32f411xe.h"
#include "fft.h"
#include <math.h>

static float fft_buf[FFT_SIZE * 2];  // interleaved real/imag
static float fft_mag[FFT_SIZE / 2];
static float fft_sample_rate = 1666667.0f; // ~1.67MSPS (APB2/4=25MHz, 15cyc/samp)

void FFT_Init(void) {}

// 位反转重排
static void bit_reverse(float *data, int n)
{
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i < j) {
            float tr = data[i * 2], ti = data[i * 2 + 1];
            data[i * 2]     = data[j * 2];
            data[i * 2 + 1] = data[j * 2 + 1];
            data[j * 2]     = tr;
            data[j * 2 + 1] = ti;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}

// Radix-2 DIT FFT
static void fft_radix2(float *data, int n)
{
    bit_reverse(data, n);

    for (int s = 1; s <= (int)(log2f((float)n) + 0.5f); s++) {
        int m = 1 << s;
        float wm_real = cosf(-2.0f * 3.14159265f / m);
        float wm_imag = sinf(-2.0f * 3.14159265f / m);

        for (int k = 0; k < n; k += m) {
            float w_real = 1.0f, w_imag = 0.0f;
            for (int j = 0; j < m / 2; j++) {
                int even = (k + j) * 2;
                int odd  = (k + j + m / 2) * 2;

                float t_real = w_real * data[odd] - w_imag * data[odd + 1];
                float t_imag = w_real * data[odd + 1] + w_imag * data[odd];

                data[odd]     = data[even] - t_real;
                data[odd + 1] = data[even + 1] - t_imag;
                data[even]    += t_real;
                data[even + 1]+= t_imag;

                float w_tmp = w_real * wm_real - w_imag * wm_imag;
                w_imag = w_real * wm_imag + w_imag * wm_real;
                w_real = w_tmp;
            }
        }
    }
}

// 处理ADC数据: 去直流 → 汉宁窗 → FFT → 幅度
void FFT_Process(const uint16_t *adc_data)
{
    // 计算直流分量
    float dc = 0.0f;
    for (int i = 0; i < FFT_SIZE; i++)
        dc += (float)(adc_data[i] & 0xFFF);
    dc /= FFT_SIZE;

    // 去直流 + 汉宁窗 + 填充实部
    for (int i = 0; i < FFT_SIZE; i++) {
        float val = (float)(adc_data[i] & 0xFFF) - dc;
        float win = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (FFT_SIZE - 1)));
        fft_buf[i * 2]     = val * win;
        fft_buf[i * 2 + 1] = 0.0f;
    }

    fft_radix2(fft_buf, FFT_SIZE);

    // 计算幅度 (仅正频率部分, bin 1..511, 跳过DC)
    float max_mag = 0.0f;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float real = fft_buf[i * 2];
        float imag = fft_buf[i * 2 + 1];
        float mag = sqrtf(real * real + imag * imag);
        fft_mag[i] = mag;
        if (mag > max_mag) max_mag = mag;
    }

    // 幅度归一化 (避免小信号显示为零)
    if (max_mag > 0.0f) {
        for (int i = 1; i < FFT_SIZE / 2; i++)
            fft_mag[i] /= max_mag;
    }
}

float FFT_GetPeakFreq(void)
{
    float max_mag = 0.0f;
    int peak_bin = 1;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        if (fft_mag[i] > max_mag) {
            max_mag = fft_mag[i];
            peak_bin = i;
        }
    }
    return fft_sample_rate * peak_bin / FFT_SIZE;
}

float FFT_GetPeakMagnitude(void)
{
    float max_mag = 0.0f;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        if (fft_mag[i] > max_mag) max_mag = fft_mag[i];
    }
    return max_mag;
}

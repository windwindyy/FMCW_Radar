/*
  FMCW宽带线性调频信号 — 诊断版本

  STM32F411CEU6 (主频100MHz, HSI+PLL) + ADF4350
  参考频率: 25 MHz (外部晶振, PFD = 25MHz, R=1)
  扫频范围: 2200 MHz ~ 4000 MHz
  频率步进: 100 kHz

  LED指示 (PC13, 低电平点亮):
    启动序列快闪5次 → 慢闪2次 → 进入主循环
    主循环LED以1Hz匀速闪烁 (确认MCU正常运行)
    若启动序列不断重复 → MCU在反复复位

  MUXOUT (ADF4350 LD引脚):
    高电平 = PLL锁定
    低电平 = PLL失锁
 */

#include "stm32f411xe.h"
#include "system.h"
#include "spi.h"
#include "adf4350.h"

// ============================================================
//  模式选择
//    0 = FMCW三角波扫频
//    1 = 单频点输出 (推荐先用此模式诊断)
// ============================================================
#define MODE_SWEEP          0
#define MODE_SINGLE         1
#define CURRENT_MODE        MODE_SWEEP    // <-- 切换到扫频模式

// 单频输出频率 (kHz)
#define SINGLE_FREQ_KHZ     2400000U

// 扫频参数 (仅MODE_SWEEP下生效)
#define FREQ_START_MHZ      2200U
#define FREQ_STOP_MHZ       4000U
#define FREQ_STEP_KHZ       10U     // 10kHz步进, 与adf4350.c中MOD=2500一致
#define SWEEP_STEP_DELAY_US 0U     // 0=SPI速率极限: ~0.72s单程, ~1.44s周期

// 预计算常量 (必须与adf4350.c一致!)
#define PFD_KHZ             25000U
#define MOD_VALUE           2500U   // MOD = 25MHz / 10kHz

// ------------------------------------------------------------
// 延时
// ------------------------------------------------------------
static void delay_us(uint32_t us)
{
    volatile uint32_t cnt;
    for (cnt = 0; cnt < us * 17; cnt++) {
        __NOP();
    }
}

// ------------------------------------------------------------
// LED (PC13, 低电平点亮)
// ------------------------------------------------------------
static void LED_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER   &= ~(3U << 26);
    GPIOC->MODER   |=  (1U << 26);
    GPIOC->OTYPER  &= ~(1U << 13);
    GPIOC->OSPEEDR &= ~(3U << 26);
    GPIOC->BSRR = (1U << 13);
}

static void LED_On(void)  { GPIOC->BSRR = (1U << 29); }
static void LED_Off(void) { GPIOC->BSRR = (1U << 13); }

static void LED_Blink(int times, int on_ms, int off_ms)
{
    for (int i = 0; i < times; i++) {
        LED_On();  delay_ms(on_ms);
        LED_Off(); delay_ms(off_ms);
    }
}

// ------------------------------------------------------------
// 高速频率设定 (跳过校验, 专为扫频)
// ------------------------------------------------------------
static void SetFreqFast(uint32_t freq_khz)
{
    uint16_t iv = (uint16_t)(freq_khz / PFD_KHZ);
    uint16_t fv = (uint16_t)(((freq_khz % PFD_KHZ) * MOD_VALUE) / PFD_KHZ);
    uint32_t r0 = ((uint32_t)(fv & 0xFFF) << 3)
                | ((uint32_t)(iv & 0xFFFF) << 15);
    SPI1_Write32(r0);
}

// ------------------------------------------------------------
// 主程序
// ------------------------------------------------------------
int main(void)
{
    /* ---- 时钟 ---- */
    SystemClock_Config();
    SysTick_Init();

    /* ---- LED ---- */
    LED_Init();

    /* ---- 启动序列: 快闪5次 (系统就绪) ---- */
    LED_Blink(5, 100, 100);
    delay_ms(500);

    /* ---- ADF4350 初始化 ---- */
    ADF4350_Init();
    delay_ms(100);   // 等待PLL锁定 (加长到100ms)

    /* ---- 启动序列: 慢闪2次 (ADF4350初始化完成) ---- */
    LED_Blink(2, 300, 300);
    delay_ms(500);

    /* ---- 主逻辑 ---- */
#if (CURRENT_MODE == MODE_SINGLE)
    /* 单频输出: 写一次R0, LED常亮, 空闲 */
    LED_Blink(3, 80, 80);   // 快闪3次 = 进入单频模式
    delay_ms(300);

    SetFreqFast(SINGLE_FREQ_KHZ);

    LED_On();  // 常亮 = 单频模式运行中
    while (1) {
        /* 空闲, 若LED熄灭/闪烁 → MCU可能在复位 */
    }

#else
    /* 三角波扫频 */
    const uint32_t start_khz = FREQ_START_MHZ * 1000U;
    const uint32_t stop_khz  = FREQ_STOP_MHZ  * 1000U;
    const uint32_t step_khz  = FREQ_STEP_KHZ;

    LED_On();  // 扫频期间常亮
    while (1) {
        /* 正向 */
        for (uint32_t f = start_khz; f <= stop_khz; f += step_khz) {
            SetFreqFast(f);
            if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
        }

        /* 反向 */
        for (uint32_t f = stop_khz - step_khz; f > start_khz; f -= step_khz) {
            SetFreqFast(f);
            if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
        }
        SetFreqFast(start_khz);
        if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
    }
#endif
}

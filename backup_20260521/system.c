#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "system.h"

static volatile uint32_t sys_tick_count = 0;

/*
  系统时钟配置 (100MHz, 内部HSI)

  HSI = 16 MHz
  PLLM = 8   → VCO输入 = 16/8 = 2 MHz
  PLLN = 100 → VCO输出 = 2*100 = 200 MHz
  PLLP = 2   → SYSCLK = 200/2 = 100 MHz
  PLLQ = 4   → 48 MHz (USB/SDIO)

  HCLK  = SYSCLK/1   = 100 MHz
  APB2  = HCLK/1     = 100 MHz
  APB1  = HCLK/2     = 50 MHz
 */
void SystemClock_Config(void)
{
    // 使能 FPU (Cortex-M4F)
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));

    // 使能 HSI (16 MHz)
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    // FLASH: 3等待周期 + 预取 + 指令缓存 + 数据缓存
    FLASH->ACR |= FLASH_ACR_LATENCY_3WS;
    FLASH->ACR |= FLASH_ACR_PRFTEN;
    FLASH->ACR |= FLASH_ACR_ICEN;
    FLASH->ACR |= FLASH_ACR_DCEN;

    // 总线分频: HCLK=/1, APB2=/1 (100MHz), APB1=/2 (50MHz)
    RCC->CFGR &= ~(0xFU << 4);              // HPRE  = /1
    RCC->CFGR &= ~(0x7U << 13);             // PPRE2 = /1
    RCC->CFGR &= ~(0x7U << 10);
    RCC->CFGR |=  (0x4U << 10);             // PPRE1 = /2

    /* PLL 配置: PLLM=8, PLLN=100, PLLP=2, PLLQ=4, PLLSRC=HSI */
    RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |
                       RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ |
                       RCC_PLLCFGR_PLLSRC);
    RCC->PLLCFGR |= (8U << RCC_PLLCFGR_PLLM_Pos);
    RCC->PLLCFGR |= (100U << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= (4U << RCC_PLLCFGR_PLLQ_Pos);
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;

    // 使能 PLL
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // 切换到 PLL
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

// SysTick 初始化 (1ms中断, 100MHz 时钟)
void SysTick_Init(void)
{
    SysTick_Config(100000);
}

// SysTick 中断处理函数
void SysTick_Handler(void)
{
    sys_tick_count++;
    HAL_IncTick();
}

// 毫秒级延时
void delay_ms(uint32_t ms)
{
    uint32_t start = sys_tick_count;
    while ((sys_tick_count - start) < ms);
}

// 计算两个事件之间的时间间隔
uint32_t millis(void)
{
    return sys_tick_count;
}

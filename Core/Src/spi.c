/**
  SPI1驱动 (STM32F411CEU6)

  引脚分配:
    PA5 = SPI1_SCK  (时钟, AF5)
    PA7 = SPI1_MOSI (数据, AF5)
    PC14= LE        (锁存使能)

  SPI时钟: APB2=100MHz, /8分频 = 12.5MHz (ADF4350 max=20MHz)
  ADF4350: CLK上升沿锁存数据, MSB先发, 32位一帧
 */

#include "stm32f411xe.h"
#include "spi.h"

// LE引脚定义: PC14
#define LE_PORT     GPIOC
#define LE_PIN      (1U << 14)

#define LE_HIGH()   (LE_PORT->BSRR = LE_PIN)
#define LE_LOW()    (LE_PORT->BSRR = (LE_PIN << 16))  // BSRR[31:16] 复位

/*
  初始化SPI1和LE引脚

  SPI1配置:
     主模式, 仅发送, 8位数据帧
     Mode 0 (CPOL=0, CPHA=0)
     速率 = APB2/2 = 50MHz (BR=000)
     MSB先发
 */
void SPI1_Init(void)
{
    // 使能GPIOA, GPIOC (AHB1总线) 和 SPI1 (APB2总线)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /*
      PA5 (SPI1_SCK):  AF5, Very High Speed
      PA7 (SPI1_MOSI): AF5, Very High Speed
    */

    // PA5: MODER=10 (AF), OSPEEDR=11 (Very High), AFR[0]=AF5
    GPIOA->MODER   &= ~(3U << 10);
    GPIOA->MODER   |=  (2U << 10);
    GPIOA->OSPEEDR |=  (3U << 10);
    GPIOA->AFR[0]  &= ~(0xFU << 20);
    GPIOA->AFR[0]  |=  (5U << 20);

    // PA7: MODER=10 (AF), OSPEEDR=11 (Very High), AFR[0]=AF5
    GPIOA->MODER   &= ~(3U << 14);
    GPIOA->MODER   |=  (2U << 14);
    GPIOA->OSPEEDR |=  (3U << 14);
    GPIOA->AFR[0]  &= ~(0xFU << 28);
    GPIOA->AFR[0]  |=  (5U << 28);

    // PC14 (LE): 通用推挽输出, Very High Speed
    GPIOC->MODER   &= ~(3U << 28);
    GPIOC->MODER   |=  (1U << 28);
    GPIOC->OTYPER  &= ~(1U << 14);
    GPIOC->OSPEEDR |=  (3U << 28);

    // LE初始高电平 (LE上升沿锁存数据)
    LE_HIGH();

    /*
      SPI1 CR1 配置:
      Bit 15: BIDIMODE = 0 (双向模式关)
      Bit 14: BIDIOE   = 0
      Bit 13: CRCEN    = 0
      Bit 12: CRCNEXT  = 0
      Bit 11: DFF      = 0 (8位数据帧)
      Bit 10: RXONLY   = 0
      Bit 9:  SSM      = 1 (软件NSS管理)
      Bit 8:  SSI      = 1 (NSS内部拉高)
      Bit 7:  LSBFIRST = 0 (MSB先发)
      Bit 6:  SPE      = 0 (稍后使能)
      Bit 5:3 BR       = 011 (APB2/8 = 12.5MHz, ADF4350 max=20MHz)
      Bit 2:  MSTR     = 1 (主模式)
      Bit 1:  CPOL     = 0
      Bit 0:  CPHA     = 0
     */
    SPI1->CR1 = 0;
    SPI1->CR1 = (1U << 9)
              | (1U << 8)
              | (3U << 3)
              | (1U << 2);

    // CR2: 默认全0
    SPI1->CR2 = 0;

    // 使能SPI1
    SPI1->CR1 |= (1U << 6);
}

/*
  向ADF4350写入32位寄存器数据
  data：32位寄存器值 (低3位地址)

  时序:
    1. 拉低LE
    2. 依次发送4个字节 (MSB first: data[31:24], [23:16], [15:8], [7:0])
    3. 等待发送完成
    4. 拉高LE
 */
void SPI1_Write32(uint32_t data)
{
    volatile uint8_t dummy;

    // 拉低LE开始传输
    LE_LOW();

    // 发送4字节, MSB first
    // Byte 3: data[31:24]
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = (uint8_t)((data >> 24) & 0xFF);
    while (!(SPI1->SR & SPI_SR_TXE));

    // Byte 2: data[23:16]
    SPI1->DR = (uint8_t)((data >> 16) & 0xFF);
    while (!(SPI1->SR & SPI_SR_TXE));

    // Byte 1: data[15:8]
    SPI1->DR = (uint8_t)((data >> 8) & 0xFF);
    while (!(SPI1->SR & SPI_SR_TXE));

    // Byte 0: data[7:0]
    SPI1->DR = (uint8_t)(data & 0xFF);

    // 等待最后一字节发送完成
    while (!(SPI1->SR & SPI_SR_TXE));
    while (SPI1->SR & SPI_SR_BSY);

    // 清除接收缓冲区
    dummy = SPI1->DR;
    dummy = SPI1->SR;
    (void)dummy;

    // LE上升沿前延时: 确保最后CLK到LE的setup时间 (>=25ns, 20 NOP ~200ns)
    for (volatile int i = 0; i < 20; i++) __NOP();

    // 锁存数据 (LE上升沿)
    LE_HIGH();

    // LE上升沿后延时: 确保LE高电平保持 (>=20ns)
    for (volatile int i = 0; i < 20; i++) __NOP();
}

/*
  ADF4350宽带频率合成器驱动

  参考频率: 外部晶振25 MHz
  R分频: 1
  鉴相频率(PFD): 25 / 1 = 25 MHz
  输出频率范围: 2200 MHz ~ 4000 MHz
  频率步进: 10 kHz
  MOD = PFD / f_RES = 25MHz / 10kHz = 2500

  频率计算：
    VCO_freq = f_PFD * (INT + FRAC/MOD)
    RF_OUT = VCO_freq / RF_Divider

  VCO范围: 2200 MHz ~ 4400 MHz
    RF_Divider = 1, VCO = RF_OUT

  INT/FRAC计算:
    INT = freq_khz / 25000
    FRAC = (freq_khz % 25000) * 2500 / 25000

  Prescaler: 8/9
    INT_MIN = 75 (8/9 prescaler 要求)
    最小INT值: 2200/25=88 >= 75
    最大INT值: 4000/25=160 <= 65535

  波段选择时钟分频器:
    PFD = 25MHz, BandSelDiv = 250 -> 25MHz/250 = 100kHz
 */

#include "stm32f411xe.h"
#include "adf4350.h"
#include "spi.h"

#define ADF4350_PFD_MHZ     25U     // 鉴相频率 25MHz
#define ADF4350_MOD         2500U   // MOD = PFD / 步进 = 25MHz/10kHz
#define ADF4350_R_COUNTER   1U      // R分频器 = 1
#define ADF4350_BAND_SEL    200U    // 波段选择分频器: 25MHz/200=125kHz (≤125kHz spec上限)

// 全局变量
ADF4350_Regs adf4350_regs;

// 内部粗延时
static void delay_us(uint32_t us)
{
    volatile uint32_t cnt;
    for (cnt = 0; cnt < us * 17; cnt++) {
        __NOP();
    }
}


//寄存器的最低3位(DB[2:0])为寄存器地址
/*
  构建Register 5的值
  标准R5值 = 0x00580005
 */
static uint32_t ADF4350_BuildR5(void)
{
    return 0x00580005U;
}

//输出控制
/*
  构建Register 4的值
  rf_div_sel：RF输出分频选择

  波段选择时钟分频器 = 64
  输出功率 = +5dBm
  反馈回路直接从 VCO 获取信号
 */
static uint32_t ADF4350_BuildR4(uint8_t rf_div_sel)
{
    uint32_t r4 = 0;

    r4 |= 4U;                                  // DB[2:0] = 100 (R4 addr)
    r4 |= (3U << 3);                           // DB[4:3] = 11 (+5dBm)
    r4 |= (1U << 5);                           // DB5 = 1 (RF Out Enable)
    r4 |= (0U << 6);                           // DB[7:6] = 00 (AUX power)
    r4 |= (0U << 8);                           // DB8 = 0 (AUX disable)
    r4 |= (0U << 9);                           // DB9 = 0
    r4 |= (0U << 10);                          // DB10 = 0 (no MTLD)
    r4 |= (0U << 11);                          // DB11 = 0 (VCO power up)
    r4 |= ((uint32_t)ADF4350_BAND_SEL << 12);  // DB[19:12] Band Sel Div = 64
    r4 |= ((uint32_t)(rf_div_sel & 0x07) << 20); // DB[22:20] RF Div Sel
    r4 |= (1U << 23);                          // DB23 = 1 (Feedback=Fundamental)

    return r4;
}

//杂散优化
/*
  构建Register 3的值
 */
static uint32_t ADF4350_BuildR3(void)
{
    uint32_t r3 = 0;

    r3 |= 3U;                      // DB[2:0] = 011 (R3 addr)
    r3 |= (150U << 3);             // DB[14:3] Clock Divider = 150
    r3 |= (0U << 15);              // DB[16:15] = 00 (clk div off)
    r3 |= (0U << 18);              // DB18 = 0 (CSR off)
    r3 |= (0U << 21);              // DB21 = 0 (Charge Cancel off）
    r3 |= (0U << 22);              // DB22 = 0 (ABP = 6ns frac-N)
    r3 |= (0U << 23);              // DB23 = 0 (Band Sel Clk = Low)

    return r3;
}

//核心控制
/*
  构建Register 2的值

  R Counter = 1
  CP Current = 2.50mA
 //电流越大，锁定速度越快，但杂散变大；电流越小，噪声越低，但锁定慢
  鉴相器极性为正
  MUXOUT = 锁定检测
  低噪模式 (ADF4350: DB[30:29]保留位，设为00)
 */
static uint32_t ADF4350_BuildR2(void)
{
    uint32_t r2 = 0;

    r2 |= 2U;                      // DB[2:0] = 010 (R2 addr)
    r2 |= (0U << 3);               // Counter Reset = disabled
    r2 |= (0U << 4);               // CP Three-State = disabled
    r2 |= (0U << 5);               // Power-Down = disabled
    r2 |= (1U << 6);               // PD Polarity = Positive
    r2 |= (0U << 7);               // LDP = 10ns
    r2 |= (0U << 8);               // LDF = FRAC-N
    r2 |= (7U << 9);               // CP Current = 0111 -> 2.50mA
    r2 |= (0U << 13);              // Double Buffer = disabled
    r2 |= (ADF4350_R_COUNTER << 14); // R Counter = 1
    r2 |= (0U << 24);              // RDIV2 = disabled
    r2 |= (0U << 25);              // Ref Doubler = disabled
    r2 |= (6U << 26);              // MUXOUT = 110 (Digital Lock Detect)
    r2 |= (0U << 29);              // DB[30:29] = 00 (ADF4350保留位)

    return r2;
}

//模数与预分频
/*
  构建Register 1的值
  mod 模数值 (2~4095)
  Prescaler = 8/9, Phase = 1
 */
static uint32_t ADF4350_BuildR1(uint16_t mod)
{
    uint32_t r1 = 0;

    r1 |= 1U;                              // DB[2:0] = 001 (R1 addr)
    r1 |= ((uint32_t)(mod & 0xFFF) << 3);  // DB[14:3] = MOD
    r1 |= (1U << 15);                      // Phase = 1 (recommended)
    r1 |= (1U << 27);                      // Prescaler = 8/9
    r1 |= (0U << 28);                      // Phase Adjust = off (ADF4350不支持)

    return r1;
}

// 频率设定
/*
  构建Register 0的值
  integer_val：整数部分 (75~65535 for 8/9 prescaler)
  frac_val：分数部分 (0~MOD-1)
 */
static uint32_t ADF4350_BuildR0(uint16_t integer_val, uint16_t frac_val)
{
    uint32_t r0 = 0;

    r0 |= 0U;                                          // DB[2:0] = 000 (R0 addr)
    r0 |= ((uint32_t)(frac_val & 0xFFF) << 3);         // DB[14:3] = FRAC
    r0 |= ((uint32_t)(integer_val & 0xFFFF) << 15);    // DB[30:15] = INT

    return r0;
}

/*
  ADF4350初始化

  上电初始化序列: R5 -> R4 -> R3 -> R2 -> R1 -> R0
  初始频率设为2200MHz (VCO=2200MHz, Div=1)
  INT = 2200/25 = 88, FRAC = 0, MOD = 2500 (f_RES = 10kHz)
 */
void ADF4350_Init(void)
{
    // 初始化SPI
    SPI1_Init();

    // 等待ADF4350上电稳定
    delay_us(5000);

    // 构建初始寄存器值 (初始频率2200MHz, VCO=2200MHz, Div=1)
    // INT = 2200/25 = 88, FRAC = 0, MOD = 250
    adf4350_regs.R5 = ADF4350_BuildR5();
    adf4350_regs.R4 = ADF4350_BuildR4(0);               // Div = 1
    adf4350_regs.R3 = ADF4350_BuildR3();
    adf4350_regs.R2 = ADF4350_BuildR2();
    adf4350_regs.R1 = ADF4350_BuildR1(ADF4350_MOD);      // MOD = 250
    adf4350_regs.R0 = ADF4350_BuildR0(88, 0);            // INT=88, FRAC=0

    // 依次写入 R5 -> R4 -> R3 -> R2 -> R1 -> R0
    SPI1_Write32(adf4350_regs.R5);
    delay_us(100);
    SPI1_Write32(adf4350_regs.R4);
    delay_us(100);
    SPI1_Write32(adf4350_regs.R3);
    delay_us(100);
    SPI1_Write32(adf4350_regs.R2);
    delay_us(100);
    SPI1_Write32(adf4350_regs.R1);
    delay_us(100);
    SPI1_Write32(adf4350_regs.R0);
    delay_us(1000);
}

/*
  设置ADF4350输出频率(kHz), 范围: 2200000 ~ 4000000
  方法:
    1. RF分频器固定为1 (无分频)
    2. 计算 INT = freq_khz / 25000
       FRAC = (freq_khz % 25000) * 250 / 25000
    3. 写R0
 */
void ADF4350_SetFrequency_kHz(uint32_t freq_khz)
{
    uint16_t int_val, frac_val;
    uint32_t new_r0;

    // 频率范围限制
    if (freq_khz < 2200000) freq_khz = 2200000;
    if (freq_khz > 4000000) freq_khz = 4000000;

    /* 计算INT和FRAC
      VCO_freq = PFD * (INT + FRAC/MOD)
      PFD = 25 MHz = 25000 kHz, MOD = 250
      INT = freq_khz / 25000
      FRAC = (freq_khz % 25000) * 250 / 25000
     */
    int_val  = (uint16_t)(freq_khz / 25000U);
    frac_val = (uint16_t)(((freq_khz % 25000U) * ADF4350_MOD) / 25000U);

    // 构建R0
    new_r0 = ADF4350_BuildR0(int_val, frac_val);

    // 写R0 (触发VCO自动校准和双缓冲寄存器更新)
    adf4350_regs.R0 = new_r0;
    SPI1_Write32(new_r0);
}

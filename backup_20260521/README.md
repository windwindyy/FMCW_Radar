# FMCW Radar — 最终稳定版本备份

**日期:** 2026-05-21

## 硬件
- MCU: STM32F411CEU6 @ 100MHz (HSI+PLL)
- 频率合成器: ADF4350 (25MHz 外部晶振)

## 最终参数

| 参数 | 值 |
|------|-----|
| PFD | 25 MHz (R=1) |
| MOD | 2500 |
| 频率步进 | 10 kHz |
| 扫频范围 | 2200 - 4000 MHz |
| SPI 时钟 | 12.5 MHz (APB2/8) |
| Band Select | 200 (125 kHz) |
| CP 电流 | 2.50 mA |
| 输出功率 | +5 dBm |
| 扫频模式 | 连续三角波, 全速 |
| 扫频周期 | ~1.4s |

## 修复记录
1. GCC 启动文件替换 (Keil→GCC语法)
2. 链接脚本 (STM32F411XE_FLASH.ld)
3. SysTick_Handler 重复定义移除
4. Band Select 分频器 64→200
5. SPI 时钟 50MHz→12.5MHz (ADF4350 spec max 20MHz)
6. LE setup/hold 时序增强
7. Syscalls 桩实现
8. 调试模式 (扫频/单频可选)
9. MOD 250→2500 (100kHz→10kHz步进)
10. 全速扫频 (SWEEP_DELAY=0, ~4μs/步)

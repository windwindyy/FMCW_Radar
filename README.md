# FMCW Radar

基于 STM32F411CEU6 + ADF4350 + ADL5380 的 FMCW 宽带线性调频雷达系统，用于通信电路系统课程设计。

## 系统架构

```
发射链路:  ADF4350 ──→ 天线
              │ (共用LO)
接收链路:  天线 ──→ QPL9058×2 ──→ 巴伦 ──→ ADL5380 ──→ LPF ──→ INA331 ──→ ADC
```

## 硬件

### 发射机

| 模块 | 型号 | 引脚 |
|---|---|---|
| MCU | STM32F411CEU6 (100MHz) | — |
| 频率合成器 | ADF4350 (25MHz 外部晶振) | SPI1: PA5(SCK), PA7(MOSI), PC14(LE) |
| OLED | SSD1306 128×64 I2C | PB13(SCL), PB15(SDA) |
| LED | PC13 | 低电平点亮, 状态指示 |

### 接收机

| 模块 | 型号 | 说明 |
|---|---|---|
| 低噪声放大器 ×2 | QPL9058 | 天线接收信号两级放大 |
| 巴伦 (RF) | — | 单端转差分，送入混频器 RF 输入端 |
| 巴伦 (LO) | — | LO 单端转差分，送入混频器 LO 输入端 |
| 正交解调器 | ADL5380 | 混频器，LO 共用发射机 ADF4350 输出 |
| 低通滤波器 | 三阶 π 型 LC 切比雪夫 | 差分 IF 滤波，去除和频分量 |
| 仪表放大器 | INA331 | IF 差分信号放大，送入 ADC |
| ADC | 内置 ADC1, PA0 | 中频信号采样，输入信号处理 |

## 功能

### 发射机

- **三角波扫频**: 2200 MHz → 4000 MHz, 10 kHz 步进
- **单频模式**: 可切换定点频率输出 (2.4 GHz)
- **SPI 寄存器级驱动**: ADF4350 高速频率更新, 带 LE 引脚精确时序控制

### 接收机

- **RF 前端**: 天线 → QPL9058 两级 LNA 放大 → 巴伦单端转差分 → ADL5380 混频
- **LO**: 共用发射机 ADF4350 输出，经巴伦单端转差分送入 ADL5380
- **IF 滤波**: 三阶 π 型 LC 切比雪夫低通滤波器，去除和频分量，保留差频（拍频）
- **IF 放大**: INA331 仪表放大器差分输入，单端输出至 ADC
- **ADC 采样**: 1.67 MSPS DMA 连续采集, 1024 点 FFT 实时频谱分析
- **距离测量**: 根据拍频频率和扫频参数计算目标距离, OLED 显示

## OLED 显示布局

```
FMCW Radar V2
Fpeak: XXXXXX Hz
Dist:  XX.X m
S=2200-4000MHz
```

## 距离计算

```
R = f_b · c · T_sweep / (2 · B)
```

| 参数 | 值 |
|---|---|
| 光速 c | 299792458 m/s |
| 扫频带宽 B | 1800 MHz |
| 单程扫频时间 T | ~0.9 s (可校准) |

## 编译 & 烧录

- 工具链: GCC ARM (arm-none-eabi-gcc)
- IDE: EIDE (Embedded IDE)
- 烧录器: ST-Link (SWD)

### 可调参数

`Core/Src/main.c` 中 `USER CODE BEGIN PD` 区域:

```c
#define CURRENT_MODE    MODE_SWEEP   // MODE_SWEEP 或 MODE_SINGLE
#define STEP_TIME_EST_US 5U          // 每步 SPI 耗时, 影响距离精度
```

## 项目结构

```
├── Core/
│   ├── Inc/           # 头文件
│   └── Src/           # 源文件
│       ├── main.c             # 主程序
│       ├── adf4350.c          # ADF4350 驱动
│       ├── spi.c              # SPI1 寄存器级驱动
│       ├── oled.c             # OLED 软件 I2C 驱动
│       ├── fft.c              # 1024点 Radix-2 FFT
│       ├── system.c           # 系统时钟 & SysTick
│       └── stm32f4xx_it.c     # 中断服务
├── Drivers/           # HAL & CMSIS
├── MDK-ARM/           # EIDE 工程配置
└── FMCW_Radar.ioc     # CubeMX 工程
```

## 许可

MIT License

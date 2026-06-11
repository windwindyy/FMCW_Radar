/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "system.h"
#include "spi.h"
#include "adf4350.h"
#include "oled.h"
#include "fft.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MODE_SWEEP          0
#define MODE_SINGLE         1
#define CURRENT_MODE        MODE_SWEEP

#define SINGLE_FREQ_KHZ     2400000U

#define FREQ_START_MHZ      2200U
#define FREQ_STOP_MHZ       4000U
#define FREQ_STEP_KHZ       10U
#define SWEEP_STEP_DELAY_US 0U

#define PFD_KHZ             25000U
#define MOD_VALUE           2500U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
DMA_HandleTypeDef hdma_adc1;
static uint16_t adc_dma_buf[ADC_BUF_SIZE];
static volatile uint8_t adc_data_ready;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
static void ADC_DMA_Start(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void delay_us(uint32_t us)
{
    volatile uint32_t cnt;
    for (cnt = 0; cnt < us * 17; cnt++) {
        __NOP();
    }
}

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

static void SetFreqFast(uint32_t freq_khz)
{
    uint16_t iv = (uint16_t)(freq_khz / PFD_KHZ);
    uint16_t fv = (uint16_t)(((freq_khz % PFD_KHZ) * MOD_VALUE) / PFD_KHZ);
    uint32_t r0 = ((uint32_t)(fv & 0xFFF) << 3)
                | ((uint32_t)(iv & 0xFFFF) << 15);
    SPI1_Write32(r0);
}

// ADC DMA 连续采样启动
static void ADC_DMA_Start(void)
{
    HAL_ADC_Stop(&hadc1);
    HAL_ADC_DeInit(&hadc1);
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&hadc1);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, ADC_BUF_SIZE);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SystemCoreClock = 100000000;
  SysTick_Init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  LED_Init();
  LED_Blink(5, 100, 100);
  delay_ms(500);

  OLED_Init();
  OLED_ShowString(0, 0, "FMCW Radar V2");
  OLED_ShowString(0, 2, "ADC FFT Ready");
  OLED_Update();

  ADF4350_Init();
  delay_ms(100);
  LED_Blink(2, 300, 300);
  delay_ms(500);

  ADC_DMA_Start();
  FFT_Init();

#if (CURRENT_MODE == MODE_SINGLE)
  LED_Blink(3, 80, 80);
  delay_ms(300);
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if (CURRENT_MODE == MODE_SINGLE)
    SetFreqFast(SINGLE_FREQ_KHZ);
    LED_On();
    while (1) {
        if (adc_data_ready) {
            adc_data_ready = 0;
            FFT_Process(adc_dma_buf);
            OLED_ShowString(0, 0, "FMCW Radar V2");
            OLED_ShowString(0, 2, "Fpeak:       Hz");
            OLED_ShowNum(7, 2, (uint32_t)FFT_GetPeakFreq(), 6);
            OLED_ShowString(0, 4, "Mag:         ");
            OLED_ShowNum(5, 4, (uint32_t)(FFT_GetPeakMagnitude() * 1000), 4);
            OLED_ShowString(0, 6, "Mode: SINGLE");
            OLED_Update();
        }
    }
#else
    {
        const uint32_t start_khz = FREQ_START_MHZ * 1000U;
        const uint32_t stop_khz  = FREQ_STOP_MHZ  * 1000U;
        const uint32_t step_khz  = FREQ_STEP_KHZ;

        LED_On();
        OLED_ShowString(0, 0, "FMCW Radar V2");
        OLED_ShowString(0, 6, "S=2200-4000MHz");
        while (1) {
            for (uint32_t f = start_khz; f <= stop_khz; f += step_khz) {
                SetFreqFast(f);
                if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
            }
            if (adc_data_ready) {
                adc_data_ready = 0;
                FFT_Process(adc_dma_buf);
                OLED_ShowString(0, 2, "Fpeak:       Hz");
                OLED_ShowNum(7, 2, (uint32_t)FFT_GetPeakFreq(), 6);
                OLED_ShowString(0, 4, "Mag:         ");
                OLED_ShowNum(5, 4, (uint32_t)(FFT_GetPeakMagnitude() * 1000), 4);
                OLED_Update();
            }
            for (uint32_t f = stop_khz - step_khz; f > start_khz; f -= step_khz) {
                SetFreqFast(f);
                if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
            }
            SetFreqFast(start_khz);
            if (SWEEP_STEP_DELAY_US) delay_us(SWEEP_STEP_DELAY_US);
            if (adc_data_ready) {
                adc_data_ready = 0;
                FFT_Process(adc_dma_buf);
                OLED_ShowString(0, 2, "Fpeak:       Hz");
                OLED_ShowNum(7, 2, (uint32_t)FFT_GetPeakFreq(), 6);
                OLED_ShowString(0, 4, "Mag:         ");
                OLED_ShowNum(5, 4, (uint32_t)(FFT_GetPeakMagnitude() * 1000), 4);
                OLED_Update();
            }
        }
    }
#endif
    /* USER CODE END 3 */
  }
  /* USER CODE END 3 */
}


/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_14, GPIO_PIN_SET);

  /*Configure GPIO pins : PC13 PC14 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        adc_data_ready = 1;
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

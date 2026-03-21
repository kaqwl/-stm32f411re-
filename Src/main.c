/**
 ******************************************************************************
 * @file    GPIO/GPIO_IOToggle/Src/main.c
 * @author  MCD Application Team
 * @brief   This example describes how to configure and use GPIOs through
 *          the STM32F4xx HAL API.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"
#include "uart.h"
#include <stdio.h>
#include "system_time.h"
#include "timer4_time.h"
/** @addtogroup STM32F4xx_HAL_Examples
 * @{
 */

/** @addtogroup GPIO_IOToggle
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static GPIO_InitTypeDef GPIO_InitStruct;
static uint32_t last_tick = 0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void)
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();
  SystemClock_Config();
  UART_Init();

  /* Получаем частоту TIM4 (APB1) */
  uint32_t prescaler = 99;
  uint32_t period = 999;
  /* Инициализируем TIM4 */
  TIM4_Time_Init(prescaler, period);

  UART_StartReceive();

  /*##-1- Enable GPIOA Clock (to be able to program the configuration registers) */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*##-2- Configure PA05 IO in output push-pull mode to drive external LED ###*/
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  last_tick = HAL_GetTick();
  uint32_t last_blink = SystemTime_GetMs();
  // int counter = 0;

  // uint32_t last_systick = SystemTime_GetMs();
  uint32_t last_tim4 = TIM4_Time_GetMs();
  while (1)
  {
    UART_ProcessInput();
    uint32_t current_tick = HAL_GetTick();
    uint8_t tick = 0;
    if (current_tick - last_tick >= 1000)
    {
      last_tick = current_tick;
      tick = 1;
    }

    if (tick)
    {
      // char msg[64];
      // snprintf(msg, sizeof(msg), "Counter: %d\r\n", counter++);
      // UART_SendString(msg);

      // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }

    // if (SystemTime_IsElapsed(last_blink, 1000))
    // {
    //   HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    //   last_blink = SystemTime_GetMs();
    // }

    if (TIM4_Time_IsElapsed(last_tim4, 1000))
    {
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
      last_tim4 = TIM4_Time_GetMs();
    }
  }
}

/*
┌─────────────────────────────────────────────────────────────┐
│                     STM32F411 Clock Tree                    │
├─────────────────────────────────────────────────────────────┤
│  HSE (8MHz) → PLL → SYSCLK (100 MHz) → Ядро (CPU)           │
│                                      │                      │
│                                      ├─▶ AHB (HCLK) 100 MHz │
│                                      │   ├─▶ RAM, Flash, DMA│
│                                      │   ├─▶ GPIO           │       ├─▶ USART1, USART6│
│                                      │       └─▶ TIM10-11   │
│                                      │                      │
│                                      └─▶ APB1 (PCLK1) 50 MHz│
│                                          ├─▶ **USART2**     │
│                                          ├─▶ I2C, SPI2-3    │
│                                          └─▶ TIM2-5         │
└─────────────────────────────────────────────────────────────┘


                     ┌─────────────────────────────────────┐
                     │              PLL (VCO)              │
                     │            200 МГц (N=200)          │
                     └──────────────────┬──────────────────┘
                                        │
                        ┌───────────────┴───────────────┐
                        │                               │
                  [ / PLLP=2 ]                    [ / PLLQ=4 ]
                        │                               │
                        ▼                               ▼
                  ┌───────────┐                   ┌───────────┐
                  │  SYSCLK   │                   │   USB     │
                  │  100 МГц  │                   │  48 МГц   │
                  └─────┬─────┘                   └───────────┘
                        │
                        ▼
                  ┌───────────┐
                  │    AHB    │ HCLK (System Bus)
                  │  100 МГц  │
                  └─────┬─────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ▼               ▼               ▼
   ┌─────────┐    ┌───────────┐   ┌───────────┐
   │  CPU    │    │   APB2    │   │   APB1    │
   │  RAM    │    │ 100 МГц   │   │  50 МГц   │
   │  Flash  │    │ (Быстрая) │   │(Медленная)│
   │  DMA    │    └─────┬─────┘   └─────┬─────┘
   │  GPIO   │          │               │
   └─────────┘    ┌─────┴─────┐   ┌─────┴──────────┐
                  │           │   │                │
                  ▼           ▼   ▼                ▼
              USART1      SPI1  USART2          I2C1/2/3
              ADC1        TIM1  TIM2            TIM3/4/5

*/
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
static void Error_Handler(void)
{
  while (1)
  {
  }
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
 * @}
 */

/**
 * @}
 */

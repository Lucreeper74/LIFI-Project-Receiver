/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_uart.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32_opal_receiver.h"
#include "stm32_opal_uart_rx_cmd.h"
#include "ssd1306_oled.h"
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const char* display_TAG = "OPAL RX Unit";
bool TIM3_ITR_FLAG = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void display_summary(char* status_str, int bit_errors_count, int bit_valid_count) {
  char status_dis_str[20];
  sprintf(status_dis_str, "R= %s", status_str);

  char ber_str[20];
  sprintf(ber_str, "Bit Error= %d", bit_errors_count);

  char bit_valid_str[24];
  sprintf(bit_valid_str, "Bit Valid= %d", bit_valid_count);

  ssd1306_clear_screen();

  ssd1306_draw_string(display_TAG, (TPosition){20, 0});
  ssd1306_draw_string("---Summary---", (TPosition){18, 8});

  ssd1306_draw_string(status_dis_str, (TPosition){0, 24});
  ssd1306_draw_string(ber_str, (TPosition){0, 32});
  ssd1306_draw_string(bit_valid_str, (TPosition){0, 40});

  ssd1306_update();
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  OPAL_Receiver_Init(&hadc1, &htim2);
  UART_RX_Init(&huart2);

  ssd1306_init();
  ssd1306_clear_screen();
  ssd1306_draw_string("RX OPAL RECEIVER", (TPosition) {20, 0});
  ssd1306_draw_string("Hello World!!", (TPosition) {32, 32});
  ssd1306_update();

  OPAL_Receiver_Start_Sniffing(&hrx);

  uint16_t pwr_indicator = 0;

  char* RX_data_str_fields[] = {
      "STATUS=",
      "BIN_ERRORS=",
      "BIN_VALID=",
  };

  char* RX_str_prefix = "RX_DATA: ";

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Listen UART Commands and process them
    if (huart_rx.cmd_ready) {
        UART_Command cmd = UART_RX_ParseCmd(&huart_rx);
        printf(cmd.has_param ? "Received Command: %s, with param: %s\r\n" : "Received Command: %s\r\n", cmd.command, cmd.param);
        OPAL_RX_UART_processCommand(&cmd, &hrx);
    }

    if (TIM3_ITR_FLAG) {
      // Compute the y(n) after filtering
      pwr_indicator = compute_filter(void);
    }

    // OPAL Receiver Processing
    OPAL_Receiver_Process(&hrx);
 
    // If a frame is ready to be decoded, decode it!
    if (hrx.Status == OPAL_RECEIVER_WAITING_DECODE) {
        OPAL_Frame receivedFrame;
        OPAL_Status decoding_status = OPAL_Receiver_Decode(&hrx, &receivedFrame);

        int bit_errors_count = 0;
        char* status_str = "UNSPECIFIED";
        uint8_t status_int = 0x0;

        switch (decoding_status) {
          case OPAL_SUCCESS:
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            status_str = "SUCCESS";
            status_int = 0x1;
            bit_errors_count = 0;
            break;
            
          case OPAL_ERROR_CRC_MISMATCH:
            status_str = "CRC_MISMATCH";
            status_int = 0x2;
            bit_errors_count = OPAL_Frame_getHammingDistance(&receivedFrame, &OPAL_TestFrame);
            break;

          default:
            break;
        }

        int bit_valid_count = (OPAL_FRAME_SIZE * 8) - bit_errors_count;

        // Print received data summary
        printf("Data received! %s %s%s, %s%i, %s%i\r\n", 
          RX_str_prefix, 
          RX_data_str_fields[0], status_str, 
          RX_data_str_fields[1], bit_errors_count,
          RX_data_str_fields[2], bit_valid_count);

        
        // Display received data summary
        display_summary(status_str, bit_errors_count, bit_valid_count);

        //

        // Send summary to BLE module
        // Send BER to BLE module
        char ber_res[20];
        strcpy(ber_res, "BER=");
        char ber_str[12];
        sprintf(ber_str, "%d\0", (uint16_t) ((bit_errors_count * 1000) / (bit_errors_count + bit_valid_count)));
        strcat(ber_res, ber_str);
        HAL_UART_Transmit(&huart1, (uint8_t*) &ber_res, sizeof(ber_res), 1);
        HAL_Delay(100);

        // Send pw level to BLE module
        char pwr_res[20];
        strcpy(pwr_res, "RX_PWR=");
        char pwr_str[12];
        sprintf(pwr_str, "%d\0", 50);
        strcat(pwr_res, pwr_str);
        HAL_UART_Transmit(&huart1, (uint8_t*) &pwr_res, sizeof(pwr_res), 1);
        HAL_Delay(100);

        // Send status to BLE module
        char stat_res[20];
        strcpy(stat_res, "STATUS=");
        char stat_str[12];
        sprintf(stat_str, "%d\0", status_int);
        strcat(stat_res, stat_str);
        HAL_UART_Transmit(&huart1, (uint8_t*) &stat_res, sizeof(stat_res), 1);
        HAL_Delay(100);

        //

        OPAL_Receiver_Start_Sniffing(&hrx); // Return to sniffing mode
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_ADC12
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance == ADC1) {
    OPAL_Receiver_Buffer_Callback(&hrx, true);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc->Instance == ADC1) {
    OPAL_Receiver_Buffer_Callback(&hrx, false);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    UART_RX_Callback(huart);
}

PUTCHAR_PROTOTYPE
{
  // Used for Printf
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
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

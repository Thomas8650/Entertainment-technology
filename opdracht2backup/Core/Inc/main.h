/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

#include "stm32h5xx_nucleo.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Cs_Pin_Pin GPIO_PIN_5
#define Cs_Pin_GPIO_Port GPIOC
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define JTDI_Pin GPIO_PIN_15
#define JTDI_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

// MCP23S17 Register Addresses
#define MCP23S17_IODIRA   0x00  // I/O Direction Register Port A
#define MCP23S17_IODIRB   0x01  // I/O Direction Register Port B
#define MCP23S17_IPOLA    0x02  // Input Polarity Port A
#define MCP23S17_IPOLB    0x03  // Input Polarity Port B
#define MCP23S17_GPINTENA 0x04  // Interrupt-on-change Port A
#define MCP23S17_GPINTENB 0x05  // Interrupt-on-change Port B
#define MCP23S17_DEFVALA  0x06  // Default Compare Port A
#define MCP23S17_DEFVALB  0x07  // Default Compare Port B
#define MCP23S17_INTCONA  0x08  // Interrupt Control Port A
#define MCP23S17_INTCONB  0x09  // Interrupt Control Port B
#define MCP23S17_IOCON    0x0A  // Configuration Register
#define MCP23S17_GPPUA    0x0C  // Pull-up Resistor Port A
#define MCP23S17_GPPUB    0x0D  // Pull-up Resistor Port B
#define MCP23S17_INTFA    0x0E  // Interrupt Flag Port A
#define MCP23S17_INTFB    0x0F  // Interrupt Flag Port B
#define MCP23S17_INTCAPA  0x10  // Interrupt Capture Port A
#define MCP23S17_INTCAPB  0x11  // Interrupt Capture Port B
#define MCP23S17_GPIOA    0x12  // GPIO Port A
#define MCP23S17_GPIOB    0x13  // GPIO Port B
#define MCP23S17_OLATA    0x14  // Output Latch Port A
#define MCP23S17_OLATB    0x15  // Output Latch Port B

// MCP23S17 Hardware Address (A2, A1, A0 all connected to GND)
#define MCP23S17_ADDR     0x00

// MCP23S17 Control Byte Format: 0100 A2 A1 A0 R/W
#define MCP23S17_WRITE_CMD (0x40 | (MCP23S17_ADDR << 1))
#define MCP23S17_READ_CMD  (0x40 | (MCP23S17_ADDR << 1) | 0x01)

// Button Matrix Configuration
#define MATRIX_ROWS       4
#define MATRIX_COLS       4
#define BASE_NOTE         60  // Middle C (C4)
#define DEBOUNCE_TIME_MS  20  // Debounce delay in milliseconds

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

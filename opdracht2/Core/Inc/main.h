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
#define Mcp_cs_Pin GPIO_PIN_9
#define Mcp_cs_GPIO_Port GPIOC
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

// ============================================================
// KNOPPENMATRIX CONFIGURATIE
// ============================================================
// De matrix bestaat uit 4 rijen en 4 kolommen = 16 knoppen.
// Elke knop krijgt een uniek MIDI-nootnummer op basis van
// zijn positie (rij en kolom) in de matrix.
#define MATRIX_ROWS       4   // Aantal rijen in de knoppenmatrix
#define MATRIX_COLS       4   // Aantal kolommen in de knoppenmatrix

// ============================================================
// MIDI NOOT-MAPPING FORMULE
// ============================================================
// Formule:  midi_noot = BASIS_NOOT + (rij x 4) + kolom
//
// BASIS_NOOT = 60 = Midden C (ook wel C4 genoemd).
// Dit is het startpunt: knop [rij=0, kolom=0] speelt noot 60.
//
// Elke stap naar rechts (kolom +1) verhoogt de noot met 1.
// Elke stap naar beneden (rij +1) verhoogt de noot met 4
// (want er zijn 4 kolommen per rij).
//
// Volledige mapping van de 4x4 matrix:
// +--------+--------+--------+--------+
// | K[0,0] | K[0,1] | K[0,2] | K[0,3] |
// |  60 C4 |  61 C#4|  62 D4 |  63 D#4|
// +--------+--------+--------+--------+
// | K[1,0] | K[1,1] | K[1,2] | K[1,3] |
// |  64 E4 |  65 F4 |  66 F#4|  67 G4 |
// +--------+--------+--------+--------+
// | K[2,0] | K[2,1] | K[2,2] | K[2,3] |
// |  68 G#4|  69 A4 |  70 A#4|  71 B4 |
// +--------+--------+--------+--------+
// | K[3,0] | K[3,1] | K[3,2] | K[3,3] |
// |  72 C5 |  73 C#5|  74 D5 |  75 D#5|
// +--------+--------+--------+--------+
//
// Zo speel je met 16 knoppen de noten C4 t/m D#5 (een octaaf
// plus een grote secunde).
#define BASE_NOTE         60  // BASIS_NOOT = 60 = Midden C (C4)

// Debouncetijd in milliseconden.
// Als een knop ingedrukt wordt, kan het contact even trillen
// (bounce). We wachten 20 ms voordat we de toestand accepteren
// zodat we geen dubbele MIDI-berichten sturen.
#define DEBOUNCE_TIME_MS  20  // Debouncevertraging in milliseconden

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

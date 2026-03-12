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
#include "tusb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

SPI_HandleTypeDef hspi3;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_SPI3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void MCP23S17_WriteReg(uint8_t reg, uint8_t data);
uint8_t MCP23S17_ReadReg(uint8_t reg);
void Debug_Print(const char* msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/**
 * @brief Dit is noodzakelijk voor de TinyUSB bibliotheek.
 * De bibliotheek moet weten hoeveel milliseconden de microcontroller al draait.
 * We gebruiken hiervoor de standaard HAL tijdsfunctie.
 */
uint32_t tusb_time_millis_api(void) {
  return HAL_GetTick();
}

/**
 * @brief Functie om via SPI een register in de MCP23S17 I/O Expander te schrijven.
 * @param reg Het adres van het register (zie datasheet, bv: 0x00 is IODIRA).
 * @param data De 8-bit waarde die we in dit register willen opslaan.
 */
void MCP23S17_WriteReg(uint8_t reg, uint8_t data) {
    // 0x40 is de Opcode voor een Write-actie (Device adres bits A0,A1,A2 zijn 0)
    uint8_t txData[3] = {0x40, reg, data}; 
    
    // CS (Chip Select) omlaag trekken om te zeggen "Hey MCP chip, luister nu!"
    HAL_GPIO_WritePin(Cs_pin_GPIO_Port, Cs_pin_Pin, GPIO_PIN_RESET);
    // Verzend de 3 bytes via SPI3
    HAL_SPI_Transmit(&hspi3, txData, 3, 100);
    // CS weer omhoog trekken om de communicatie te beëindigen
    HAL_GPIO_WritePin(Cs_pin_GPIO_Port, Cs_pin_Pin, GPIO_PIN_SET);
}

/**
 * @brief Functie om via SPI de waarde van een register op te vragen.
 * @param reg Het adres van het register dat we willen inlezen.
 * @return De ingestelde of gemeten 8-bit waarde uit de chip.
 */
uint8_t MCP23S17_ReadReg(uint8_t reg) {
    // 0x41 is de Opcode voor een Read-actie 
    uint8_t txData[3] = {0x41, reg, 0xFF}; 
    uint8_t rxData[3] = {0}; // Hierin vangen we het antwoord op
    
    // SPI communicatie starten
    HAL_GPIO_WritePin(Cs_pin_GPIO_Port, Cs_pin_Pin, GPIO_PIN_RESET);
    // TransmitReceive stuurt data heen en vangt tegelijkertijd data op
    HAL_SPI_TransmitReceive(&hspi3, txData, rxData, 3, 100);
    HAL_GPIO_WritePin(Cs_pin_GPIO_Port, Cs_pin_Pin, GPIO_PIN_SET);
    
    // De chip antwoordt in de 3de byte, dus we geven array positie 2 terug
    return rxData[2]; 
}

/**
 * @brief Stuur tekst (net als printf) naar je PC via UART.
 * Aangesloten op VCP (Virtual COM Port) van de ST-Link zodat PuTTY het kan lezen.
 */
void Debug_Print(const char* msg) {
  // We gebruiken de Board Support Package UART poort nr. 1 (in code 'COM1'). 
  // Dit komt normaal uit op USART2 via PA2.
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
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
  MX_USB_PCD_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  // Initialize TinyUSB stack (CRITICAL - was missing!)
  tusb_init();
  
  // Enable USB pull-up to signal device presence to host
  HAL_PCD_DevConnect(&hpcd_USB_DRD_FS);

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  // --- SPI TEST MCP23S17 START ---
  HAL_Delay(500); // Korte opstart-delay om te zorgen dat we dit zinnetje in PuTTY niet mislopen na een harde reset

  char dbg_buf[128];
  Debug_Print("\r\n\r\n=== STARTING STM32 & MCP23S17 INIT ===\r\n");

  // TESTFASE: Testen of de fysieke SPI bedrading werkt.
  // We schrijven en lezen twee verschillende hexadeximale patronen (0x55 is 01010101, 0xAA is 10101010).
  MCP23S17_WriteReg(0x00, 0x55); 
  uint8_t test_read1 = MCP23S17_ReadReg(0x00);
  snprintf(dbg_buf, sizeof(dbg_buf), "TEST 1 - Geschreven: 0x55, Gelezen: 0x%02X\r\n", test_read1);
  Debug_Print(dbg_buf);

  MCP23S17_WriteReg(0x00, 0xAA); 
  uint8_t test_read2 = MCP23S17_ReadReg(0x00);
  snprintf(dbg_buf, sizeof(dbg_buf), "TEST 2 - Geschreven: 0xAA, Gelezen: 0x%02X\r\n", test_read2);
  Debug_Print(dbg_buf);

  // Als de chip antwoordt met wat we gestuurd hebben, is de SPI bus correct aangesloten!
  if (test_read1 == 0x55 && test_read2 == 0xAA) {
      Debug_Print(">>> SPI COMMUNICATIE SUCCESVOL! DE MCP23S17 WERKT! <<<\r\n");
  } else {
      Debug_Print(">>> FOUT: SPI communicatie gefaald. Controleer de bedrading (MOSI, MISO, SCK of CS)! <<<\r\n");
  }

  // --- OPRERATIONELE INSTELFASE --- 
  // Na eventueel falen of slagen van de test, overschrijven we de instellingen 
  // voor het daadwerkelijke knoppenmatrix scenario zoals in de powerpoint werd gevraagd:
  MCP23S17_WriteReg(0x00, 0x00); // IODIRA (Data Direction Reg A) = 0x00 = Alle pinnen op port A zijn OUTPUT (Kolommen)
  MCP23S17_WriteReg(0x01, 0xFF); // IODIRB (Data Direction Reg B) = 0xFF = Alle pinnen op port B zijn INPUT (Rijen)
  MCP23S17_WriteReg(0x0D, 0xFF); // GPPUB  (Pull-Up Resistor Reg B) = 0xFF = Pull-up weerstanden activeren op Port B

  Debug_Print("MCP23S17 instellingen voor Knoppenmatrix geladen.\r\n");
  Debug_Print("======================================\r\n\r\n");
  // --- SPI TEST MCP23S17 EIND ---

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  uint32_t board_millis = 0;        // Timer voor de matrix scanner (Voor debouncing)
  uint32_t debug_millis = 0;        // Timer voor debug monitor (PuTTY print output)
  uint8_t debug_raw_reads[4] = {0}; // Array om de 'pure' ingelezen waarden per kolom op te slaan (voor printen)
  
  /* Array om de vorige status van alle 16 knoppen bij te houden. 
   * Formaat = [kolom_nummer][rij_nummer]. 
   * De MCP23S17 heeft pull-ups (1), dus oningedrukt is `1` en ingedrukt trekt het signaal naar aarde = `0`.
   * We beginnen met de virtuele aanname dat in het begin nog géén enkele knop is ingedrukt. */
  uint8_t previous_button_state[4][4] = {
      {1, 1, 1, 1}, // Kolom 0 Rijen(0-3)
      {1, 1, 1, 1}, // Kolom 1 Rijen(0-3)
      {1, 1, 1, 1}, // Kolom 2 Rijen(0-3)
      {1, 1, 1, 1}  // Kolom 3 Rijen(0-3)
  };

  while (1)
  {
    // 1. TinyUSB Task: Deze specifieke kleine functie moet constant aangeroepen worden, in theorie
    // regelt dit alles voor de communicatie van de USB MIDI verbinding!
    tud_task();

    /* 2. Het hardwarematige Matrix Scan Algoritme scannen. 
     * Er is een 10ms vertraging ingebouwd (Debouncing of 'ontdenderen').
     * Mechanische knoppen stuiteren en sturen trillende signalen voordat ze constant zijn. 
     * Door niet élke cyclus te scannen maar slechts elke 10ms negeren we kleine signaalruis.*/
    if (HAL_GetTick() - board_millis > 10)
    {
      board_millis = HAL_GetTick();

      // Loop één voor één over de 4 kolommen heen. Dit is het actieve 'scannen'.
      for (int col = 0; col < 4; col++) {
          
          /* Zet uitsluitend de huidige kolom LOW(0) en de rest HIGH(1).
           * PORTA stuurt stroom aan (Outputs). Via een formule `~(1 << col)` maken we 
           * bv bij 'col 0' (= bitschuiven met nul = 00000001, inversie: 11111110) enkel pin 0 een nul (LOW). */
          uint8_t porta_val = ~(1 << col);
          MCP23S17_WriteReg(0x12, porta_val); // Omdat IODIRA 0x00 is (output) schrijven we de logic naar register GPIOA (0x12)

          /* Nu één kolom LOW(0) is, lezen we PORTB (de inputs). 
           * Deze blijven door de pull-up in theorie hoog. Maar als op de kruising van onze actieve, "LOW gemaakte" kolom
           * en een input-rij iemand een circuit sluit (door te drukken op knop), zal ook de input van PORTB laag(0) uitlezen! */
          uint8_t portb_val = MCP23S17_ReadReg(0x13); // Uitlezen register GPIOB (0x13)
          debug_raw_reads[col] = portb_val;           // Sla even op in een apart array zodat we het onderaan in de debug monitor in PuTTy zien
          
          // Loop over de 4 rijen van deze kolom om te kijken wie op 0 of 1 staat.
          for (int row = 0; row < 4; row++) {
              
              // Kijk lokaal voor deze specifieke rij-pin in de ingezonden portb_val (bitwise AND).
              // Is deze 1 of 0?
              uint8_t current_state = (portb_val & (1 << row)) ? 1 : 0;
              
              // Vergelijk deze status met de opgeslagen status van onze array in millisecondes hiervoor.
              if (current_state != previous_button_state[col][row]) {
                  
                  // Het is niet langer hetzelfde -> ER IS OP DE KNOP GEDRUKT OF DE KNOP IS LOSGELATEN!
                  
                  // Bereken eerst welke noot we moeten sturen volgens de PDF formule:
                  // MIDI formula Midden C: midi_noot = 60 + (rij * 4) + kolom
                  uint8_t midi_noot = 60 + (col * 4) + row; 
                  
                  if (current_state == 0) {
                      // OUDE staat was 1, NIEUWE staat is 0 -> Knop is ingedrukt!
                      char msg[64];
                      snprintf(msg, sizeof(msg), "Noot AAN:  %d (Col:%d Row:%d)\r\n", midi_noot, col, row);
                      Debug_Print(msg);

                      // Alleen MIDI sturen via TinyUSB als de 'User-USB-poort' kabel is aangesloten en gezien door Windows
                      if (tud_mounted()) {
                          // Note ON message: 0x90 = Command Note ON Channel 0, [midi_noot] is de Note, 127 = Velocity (Hoeveelheid kracht in aanslag, 127 is max)
                          uint8_t note_on_msg[3] = { 0x90, midi_noot, 127 }; 
                          tud_midi_stream_write(0, note_on_msg, 3);
                      }
                  } 
                  else {
                      // OUDE staat was 0, NIEUWE staat is 1 -> Knop is losgelaten!
                      char msg[64];
                      snprintf(msg, sizeof(msg), "Noot UIT:  %d (Col:%d Row:%d)\r\n", midi_noot, col, row);
                      Debug_Print(msg);

                      if (tud_mounted()) {
                           // Note OFF message: 0x80 = Command Note OFF, Velocity 0
                          uint8_t note_off_msg[3] = { 0x80, midi_noot, 0 };
                          tud_midi_stream_write(0, note_off_msg, 3);
                      }
                  }
                  
                  // Update onze 2D status matrix, we onthouden we deze positie voor de volgende 10ms scan in de toekomst.
                  previous_button_state[col][row] = current_state;
              }
          }
      }
    }

    // 3. Debug Monitor: Elke 500ms visueel status update in putty over wat pin PA en PB registeren 
    if (HAL_GetTick() - debug_millis > 500)
    {
        debug_millis = HAL_GetTick();
        char dbg_msg[128];
        snprintf(dbg_msg, sizeof(dbg_msg), "RAW INPUTS -> Col0: 0x%02X | Col1: 0x%02X | Col2: 0x%02X | Col3: 0x%02X\r\n", 
                 debug_raw_reads[0], debug_raw_reads[1], debug_raw_reads[2], debug_raw_reads[3]);
        Debug_Print(dbg_msg);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 0x7;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi3.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi3.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi3.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi3.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi3.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi3.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi3.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi3.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi3.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = USBD_FS_SPEED;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Cs_pin_GPIO_Port, Cs_pin_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : Cs_pin_Pin */
  GPIO_InitStruct.Pin = Cs_pin_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(Cs_pin_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
uint32_t board_millis(void)
{
  return HAL_GetTick();
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

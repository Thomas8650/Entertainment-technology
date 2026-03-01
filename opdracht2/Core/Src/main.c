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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SPI_IO_TIMEOUT_MS 50U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */

// ============================================================
// TOESTAND VAN ELKE KNOP IN DE MATRIX
// ============================================================
// Voor elke knop houden we vier dingen bij in een struct:
typedef struct {
  bool current_state;        // Huidige toestand: true = ingedrukt, false = losgelaten
  bool previous_state;       // Vorige toestand (om te zien of er iets veranderd is)
  uint32_t last_change_time; // Tijdstip (ms) waarop de toestand voor het laatst veranderde
  bool debounce_stable;      // true = toestandsverandering is stabiel en mag verwerkt worden
} ButtonState_t;

// Tweedimensionaal array: één ButtonState_t per knop [rij][kolom]
ButtonState_t button_matrix[MATRIX_ROWS][MATRIX_COLS];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void MCP23S17_WriteRegister(uint8_t reg, uint8_t value);
uint8_t MCP23S17_ReadRegister(uint8_t reg);
void MCP23S17_Init(void);
void ScanButtonMatrix(void);
void ProcessButtonEvents(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// TinyUSB requires this function for timing
uint32_t tusb_time_millis_api(void) {
  return HAL_GetTick();
}

/**
 * @brief Write a value to an MCP23S17 register
 * @param reg: Register address
 * @param value: Value to write
 */
void MCP23S17_WriteRegister(uint8_t reg, uint8_t value) {
  uint8_t tx_data[3];
  
  tx_data[0] = MCP23S17_WRITE_CMD;  // Control byte (write)
  tx_data[1] = reg;                  // Register address
  tx_data[2] = value;                // Data to write
  
  // Pull CS low
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_RESET);
  
  // Send data via SPI
  if (HAL_SPI_Transmit(&hspi2, tx_data, 3, SPI_IO_TIMEOUT_MS) != HAL_OK) {
    return;
  }
  
  // Pull CS high
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_SET);
}

/**
 * @brief Read a value from an MCP23S17 register
 * @param reg: Register address
 * @return Value read from register
 */
uint8_t MCP23S17_ReadRegister(uint8_t reg) {
  uint8_t tx_data[3] = {MCP23S17_READ_CMD, reg, 0xFF};
  uint8_t rx_data[3] = {0};
  
  // Pull CS low
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_RESET);
  
  // Send read command + register and clock out one data byte
  if (HAL_SPI_TransmitReceive(&hspi2, tx_data, rx_data, 3, SPI_IO_TIMEOUT_MS) != HAL_OK) {
    return 0xFF;
  }
  
  // Pull CS high
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_SET);
  
  return rx_data[2];
}

/**
 * @brief Initialize the MCP23S17 for button matrix scanning
 */
void MCP23S17_Init(void) {
  // Small delay for MCP23S17 power-up
  HAL_Delay(10);
  
  // Configure IOCON register (optional - default settings are usually fine)
  // BANK=0, MIRROR=0, SEQOP=0, DISSLW=0, HAEN=1, ODR=0, INTPOL=0
  MCP23S17_WriteRegister(MCP23S17_IOCON, 0x08); // Enable hardware addressing
  
  // Configure Port A (GPA0-GPA3) as OUTPUTS for columns
  MCP23S17_WriteRegister(MCP23S17_IODIRA, 0x00); // All outputs
  
  // Configure Port B (GPB0-GPB3) as INPUTS for rows
  MCP23S17_WriteRegister(MCP23S17_IODIRB, 0x0F); // Lower 4 bits as inputs
  
  // Enable internal pull-up resistors on Port B (rows)
  MCP23S17_WriteRegister(MCP23S17_GPPUB, 0x0F); // Pull-ups on GPB0-GPB3
  
  // Set all columns HIGH initially (inactive state)
  MCP23S17_WriteRegister(MCP23S17_GPIOA, 0xFF);
}

/**
 * @brief Scan de 4x4 knoppenmatrix en sla de knoptoestanden op.
 *
 * HOE WERKT EEN KNOPPENMATRIX?
 * De matrix heeft 4 kolommen (uitgangen) en 4 rijen (ingangen).
 * We activeren één kolom tegelijk door die LOW te zetten.
 * Daarna lezen we de 4 rijen: als een knop in die kolom ingedrukt
 * is, verbindt hij de rij met de lage kolom → de rij leest LOW.
 * Zo bepalen we voor elke knop of hij ingedrukt is.
 */
void ScanButtonMatrix(void) {
  static uint32_t last_scan = 0;
  uint32_t now = HAL_GetTick(); // Huidig tijdstip in milliseconden
  
  // Scan maar elke 5 ms om de processor niet onnodig te belasten
  if (now - last_scan < 5) {
    return; // Nog geen 5 ms verstreken → sla deze scan over
  }
  last_scan = now;
  
  for (uint8_t col = 0; col < MATRIX_COLS; col++) {
    // Stap 1: Zet de actieve kolom LOW, alle andere kolommen HIGH.
    // Voorbeeld: col=0 → col_pattern = 0b11111110
    // Alleen bit 0 is laag, de rest hoog.
    uint8_t col_pattern = ~(1 << col);
    MCP23S17_WriteRegister(MCP23S17_GPIOA, col_pattern);
    
    // Kleine vertraging zodat het signaal stabiel is voor we lezen
    for(volatile int i = 0; i < 100; i++); // ca. 10 microseconden
    
    // Stap 2: Lees de 4 rijwaarden van Port B.
    // Elke bit staat voor één rij: 0 = laag (knop ingedrukt), 1 = hoog (niet ingedrukt).
    uint8_t row_values = MCP23S17_ReadRegister(MCP23S17_GPIOB);
    
    // Stap 3: Controleer elke rij
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
      // De knop is ingedrukt als de bijbehorende bit LOW is (logische inversie!).
      // '!' keert de waarde om: LOW (0) wordt true (ingedrukt).
      bool is_pressed = !(row_values & (1 << row));
      
      ButtonState_t* btn = &button_matrix[row][col]; // Verwijzing naar deze knop
      
      // --- DEBOUNCE LOGICA ---
      // Mechanische knoppen 'stuiteren' even bij indrukken/loslaten.
      // We accepteren een toestandsverandering pas als die minstens
      // DEBOUNCE_TIME_MS milliseconden stabiel is.
      if (is_pressed != btn->current_state) {
        // Er is een verschil met de vorige toestand
        if ((now - btn->last_change_time) > DEBOUNCE_TIME_MS) {
          // Genoeg tijd verstreken → toestandsverandering is echt
          btn->previous_state = btn->current_state;
          btn->current_state = is_pressed;   // Sla nieuwe toestand op
          btn->last_change_time = now;        // Onthoud het tijdstip
          btn->debounce_stable = true;        // Klaarzetten voor verwerking
          
          // DEBUG: Knippert de LED bij elke knopverandering
          BSP_LED_Toggle(LED_GREEN);
        }
      }
    }
  }
  
  // Deactiveer alle kolommen na de scan (alles hoog = inactief)
  MCP23S17_WriteRegister(MCP23S17_GPIOA, 0xFF);
}

/**
 * @brief Verwerk knoptoestandsveranderingen en stuur MIDI-berichten.
 *
 * MIDI NOOT-MAPPING:
 * Elke knop in de matrix krijgt een uniek MIDI-nootnummer.
 * Formule:  midi_noot = BASIS_NOOT + (rij x 4) + kolom
 *
 * Uitleg van de formule:
 *  - BASIS_NOOT (60) is het startpunt = Midden C (C4).
 *  - (rij x 4): elke rij telt 4 kolommen, dus elke rij lager
 *    verhoogt de noot met 4 halve tonen.
 *  - kolom: elke stap naar rechts verhoogt de noot met 1 halve toon.
 *
 * Voorbeelden:
 *  - Knop [rij=0, col=0]: 60 + (0x4) + 0 = 60  → C4  (Midden C)
 *  - Knop [rij=0, col=3]: 60 + (0x4) + 3 = 63  → D#4
 *  - Knop [rij=1, col=0]: 60 + (1x4) + 0 = 64  → E4
 *  - Knop [rij=3, col=3]: 60 + (3x4) + 3 = 75  → D#5
 *
 * MIDI BERICHTFORMAAT (3 bytes):
 *  Byte 0: Status   → 0x90 = Note ON  op kanaal 1
 *                     0x80 = Note OFF op kanaal 1
 *  Byte 1: Nootnummer (0-127)
 *  Byte 2: Velocity  → 127 = maximale aanslagsterkte (indrukken)
 *                        0 = stilte (loslaten)
 */
void ProcessButtonEvents(void) {
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      ButtonState_t* btn = &button_matrix[row][col];
      
      // Alleen verwerken als de debouncetijd verstreken is én er iets veranderd is
      if (btn->debounce_stable) {

        // --- MIDI NOOT BEREKENING ---
        // Pas de formule toe: midi_noot = BASIS_NOOT + (rij x 4) + kolom
        // BASE_NOTE = 60 (gedefinieerd in main.h)
        uint8_t midi_note = BASE_NOTE + (row * 4) + col;
        
        if (btn->current_state) {
          // Knop INGEDRUKT → stuur Note ON bericht
          // 0x90 = Note ON commando op MIDI-kanaal 1
          // midi_note = het berekende nootnummer
          // 127 = maximale velocity (aanslagsterkte)
          uint8_t note_on[3] = {0x90, midi_note, 127};
          
          if (tud_mounted()) { // Controleer of de USB-verbinding actief is
            tud_midi_stream_write(0, note_on, 3); // Stuur 3 bytes via USB-MIDI
          }
        } else {
          // Knop LOSGELATEN → stuur Note OFF bericht
          // 0x80 = Note OFF commando op MIDI-kanaal 1
          // midi_note = hetzelfde nootnummer als bij Note ON
          // 0 = velocity 0 (toon stoppen)
          uint8_t note_off[3] = {0x80, midi_note, 0};
          
          if (tud_mounted()) { // Controleer of de USB-verbinding actief is
            tud_midi_stream_write(0, note_off, 3); // Stuur 3 bytes via USB-MIDI
          }
        }
        
        // Markeer als verwerkt zodat we dit bericht niet opnieuw sturen
        btn->debounce_stable = false;
      }
    }
  }
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
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  
  // Initialize the onboard LED
  BSP_LED_Init(LED_GREEN);
  
  // Initialize TinyUSB
  tusb_init();
  
  // Initialize MCP23S17 I/O expander for button matrix
  MCP23S17_Init();
  
  // Initialize button states
  for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
      button_matrix[r][c].current_state = false;
      button_matrix[r][c].previous_state = false;
      button_matrix[r][c].last_change_time = 0;
      button_matrix[r][c].debounce_stable = false;
    }
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    // TinyUSB device task - handles USB communication
    tud_task();
    
    // Scan button matrix for changes
    ScanButtonMatrix();
    
    // Process button events and send MIDI messages
    ProcessButtonEvents();
    
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
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 0x7;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi2.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi2.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : Cs_Pin_Pin */
  GPIO_InitStruct.Pin = Cs_Pin_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Cs_Pin_GPIO_Port, &GPIO_InitStruct);

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

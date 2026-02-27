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
#include <stdbool.h>
#include <string.h>
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

SPI_HandleTypeDef hspi2;

PCD_HandleTypeDef hpcd_USB_DRD_FS;

/* USER CODE BEGIN PV */

// Button Matrix State Variables
typedef struct {
  bool current_state;      // Current button state (pressed/released)
  bool previous_state;     // Previous button state for edge detection
  uint32_t last_change_time; // Last time the state changed (for debouncing)
  bool debounce_stable;    // Is the button state stable after debouncing?
} ButtonState_t;

ButtonState_t button_matrix[MATRIX_ROWS][MATRIX_COLS];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_SPI2_Init(void);
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
  HAL_SPI_Transmit(&hspi2, tx_data, 3, HAL_MAX_DELAY);
  
  // Pull CS high
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_SET);
}

/**
 * @brief Read a value from an MCP23S17 register
 * @param reg: Register address
 * @return Value read from register
 */
uint8_t MCP23S17_ReadRegister(uint8_t reg) {
  uint8_t tx_data[2];
  uint8_t rx_data = 0;
  
  tx_data[0] = MCP23S17_READ_CMD;   // Control byte (read)
  tx_data[1] = reg;                  // Register address
  
  // Pull CS low
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_RESET);
  
  // Send command and register address
  HAL_SPI_Transmit(&hspi2, tx_data, 2, HAL_MAX_DELAY);
  
  // Read data
  HAL_SPI_Receive(&hspi2, &rx_data, 1, HAL_MAX_DELAY);
  
  // Pull CS high
  HAL_GPIO_WritePin(Cs_Pin_GPIO_Port, Cs_Pin_Pin, GPIO_PIN_SET);
  
  return rx_data;
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
  
  // DEBUG: Verify SPI communication by reading back registers
  uint8_t test_read = MCP23S17_ReadRegister(MCP23S17_IODIRA);
  if (test_read == 0x00) {
    // SPI communication works! Blink LED rapidly 3 times
    for(int i = 0; i < 3; i++) {
      BSP_LED_On(LED_GREEN);
      HAL_Delay(100);
      BSP_LED_Off(LED_GREEN);
      HAL_Delay(100);
    }
  } else {
    // SPI communication FAILED! LED stays ON
    BSP_LED_On(LED_GREEN);
    HAL_Delay(2000);
  }
}

/**
 * @brief Scan the 4x4 button matrix and update button states
 */
void ScanButtonMatrix(void) {
  static uint32_t last_scan = 0;
  uint32_t now = HAL_GetTick();
  
  // Scan every 5ms to reduce load
  if (now - last_scan < 5) {
    return;
  }
  last_scan = now;
  
  for (uint8_t col = 0; col < MATRIX_COLS; col++) {
    // Step 1: Set active column LOW, others HIGH
    uint8_t col_pattern = ~(1 << col); // e.g., col=0 → 0b11111110
    MCP23S17_WriteRegister(MCP23S17_GPIOA, col_pattern);
    
    // Small delay for signal stabilization (use microseconds if available)
    for(volatile int i = 0; i < 100; i++); // ~10us delay
    
    // Step 2: Read row values from Port B
    uint8_t row_values = MCP23S17_ReadRegister(MCP23S17_GPIOB);
    
    // Step 3: Check each row
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
      // Button is pressed when row reads LOW (pulled down through button)
      bool is_pressed = !(row_values & (1 << row));
      
      ButtonState_t* btn = &button_matrix[row][col];
      
      // Debouncing logic
      if (is_pressed != btn->current_state) {
        // State change detected
        if ((now - btn->last_change_time) > DEBOUNCE_TIME_MS) {
          // Enough time has passed, accept the state change
          btn->previous_state = btn->current_state;
          btn->current_state = is_pressed;
          btn->last_change_time = now;
          btn->debounce_stable = true;
          
          // DEBUG: Toggle LED on any button state change
          BSP_LED_Toggle(LED_GREEN);
        }
      }
    }
  }
  
  // Deactivate all columns after scanning
  MCP23S17_WriteRegister(MCP23S17_GPIOA, 0xFF);
}

/**
 * @brief Process button state changes and send MIDI messages
 */
void ProcessButtonEvents(void) {
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      ButtonState_t* btn = &button_matrix[row][col];
      
      // Check for state change (button press or release)
      if (btn->debounce_stable) {
        // Calculate MIDI note number
        uint8_t midi_note = BASE_NOTE + (row * 4) + col;
        
        if (btn->current_state) {
          // Button pressed → Send Note ON
          uint8_t note_on[3] = {0x90, midi_note, 127}; // Channel 1, velocity 127
          
          if (tud_mounted()) {
            tud_midi_stream_write(0, note_on, 3);
          }
        } else {
          // Button released → Send Note OFF
          uint8_t note_off[3] = {0x80, midi_note, 0}; // Channel 1, velocity 0
          
          if (tud_mounted()) {
            tud_midi_stream_write(0, note_off, 3);
          }
        }
        
        // Mark as processed
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
  /* USER CODE BEGIN 2 */
  
  // Initialize TinyUSB stack (CRITICAL - was missing!)
  tusb_init();
  
  // Enable USB pull-up to signal device presence to host
  HAL_PCD_DevConnect(&hpcd_USB_DRD_FS);
  
  // Initialize MCP23S17 I/O expander
  MCP23S17_Init();
  
  // Initialize button matrix state
  memset(button_matrix, 0, sizeof(button_matrix));

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

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    // 1. TinyUSB Task (keeps the USB connection alive)
    tud_task();

    // 2. Scan the button matrix
    ScanButtonMatrix();
    
    // 3. Process button events and send MIDI messages
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
  RCC_OscInitStruct.PLL.PLLN = 129;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
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

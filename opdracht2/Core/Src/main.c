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
SPI_HandleTypeDef hspi3;

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
static void MX_ICACHE_Init(void);
static void MX_SPI3_Init(void);
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
 * @brief Print debug message via UART2
 * @param msg: String to print
 */
void Debug_Print(const char* msg) {
  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
}

/**
 * @brief Print debug message with number
 * @param msg: String to print
 * @param num: Number to print
 */
void Debug_PrintNum(const char* msg, uint8_t num) {
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "%s%d\r\n", msg, num);
  Debug_Print(buffer);
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
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
  
  // Small delay for CS to settle (tCSS = 50ns min)
  for(volatile int i = 0; i < 10; i++);
  
  // Send data via SPI
  if (HAL_SPI_Transmit(&hspi3, tx_data, 3, SPI_IO_TIMEOUT_MS) != HAL_OK) {
    HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
    return;
  }
  
  // Small delay before CS high (tCSH = 50ns min)
  for(volatile int i = 0; i < 10; i++);
  
  // Pull CS high
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  
  // CS disable time (tCSD = 50ns min)
  for(volatile int i = 0; i < 10; i++);
}

/**
 * @brief Read a value from an MCP23S17 register
 * @param reg: Register address
 * @return Value read from register
 */
uint8_t MCP23S17_ReadRegister(uint8_t reg) {
  uint8_t tx_data[3] = {MCP23S17_READ_CMD, reg, 0x00};
  uint8_t rx_data[3] = {0};
  
  // Pull CS low
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
  
  // Small delay for CS to settle (tCSS = 50ns min)
  for(volatile int i = 0; i < 10; i++);
  
  // Send read command + register and clock out one data byte
  if (HAL_SPI_TransmitReceive(&hspi3, tx_data, rx_data, 3, SPI_IO_TIMEOUT_MS) != HAL_OK) {
    HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
    return 0xFF;
  }
  
  // Small delay before CS high (tCSH = 50ns min)
  for(volatile int i = 0; i < 10; i++);
  
  // Pull CS high
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  
  // CS disable time (tCSD = 50ns min)
  for(volatile int i = 0; i < 10; i++);
  
  return rx_data[2];
}

/**
 * @brief Initialize the MCP23S17 for button matrix scanning
 */
void MCP23S17_Init(void) {
  // Small delay for MCP23S17 power-up
  HAL_Delay(100);  // Verhoogd naar 100ms voor stabiele startup
  
  // Debug: Show what opcodes we're using
  char opcode_debug[100];
  snprintf(opcode_debug, sizeof(opcode_debug), 
    "  Using opcodes: WRITE=0x%02X, READ=0x%02X\r\n", 
    MCP23S17_WRITE_CMD, MCP23S17_READ_CMD);
  Debug_Print(opcode_debug);
  
  Debug_Print("  Configuring IOCON (trying HAEN=1 first)...\r\n");
  // PROBEER EERST MET HAEN=1 (bit 3)
  // BANK=0, MIRROR=0, SEQOP=0, DISSLW=0, HAEN=1, ODR=0, INTPOL=0
  MCP23S17_WriteRegister(MCP23S17_IOCON, 0x08); // HAEN enabled
  HAL_Delay(10);
  
  // Test of het werkt
  uint8_t iocon_test = MCP23S17_ReadRegister(MCP23S17_IOCON);
  snprintf(opcode_debug, sizeof(opcode_debug), 
    "  IOCON readback: 0x%02X (wrote 0x08)\r\n", iocon_test);
  Debug_Print(opcode_debug);
  
  if (iocon_test == 0xFF || iocon_test == 0x00) {
    Debug_Print("  HAEN=1 failed, trying HAEN=0...\r\n");
    MCP23S17_WriteRegister(MCP23S17_IOCON, 0x00); // HAEN disabled
    HAL_Delay(10);
  }
  
  Debug_Print("  Configuring PORTA as outputs...\r\n");
  // Configure Port A (GPA0-GPA3) as OUTPUTS for columns
  MCP23S17_WriteRegister(MCP23S17_IODIRA, 0x00); // All outputs
  
  Debug_Print("  Configuring PORTB as inputs...\r\n");
  // Configure Port B (GPB0-GPB3) as INPUTS for rows
  MCP23S17_WriteRegister(MCP23S17_IODIRB, 0x0F); // Lower 4 bits as inputs
  
  Debug_Print("  Enabling pull-ups on PORTB...\r\n");
  // Enable internal pull-up resistors on Port B (rows)
  MCP23S17_WriteRegister(MCP23S17_GPPUB, 0x0F); // Pull-ups on GPB0-GPB3
  
  Debug_Print("  Setting all columns HIGH...\r\n");
  // Set all columns HIGH initially (inactive state)
  MCP23S17_WriteRegister(MCP23S17_GPIOA, 0xFF);
  
  // TEST: Lees configuratie terug om te verifiëren
  Debug_Print("\r\n  === VERIFICATION READS ===\r\n");
  
  uint8_t test_iocon = MCP23S17_ReadRegister(MCP23S17_IOCON);
  uint8_t test_iodira = MCP23S17_ReadRegister(MCP23S17_IODIRA);
  uint8_t test_iodirb = MCP23S17_ReadRegister(MCP23S17_IODIRB);
  uint8_t test_gppub = MCP23S17_ReadRegister(MCP23S17_GPPUB);
  uint8_t test_gpioa = MCP23S17_ReadRegister(MCP23S17_GPIOA);
  uint8_t test_gpiob = MCP23S17_ReadRegister(MCP23S17_GPIOB);
  
  char buf[100];
  snprintf(buf, sizeof(buf), "  IOCON: 0x%02X (expect 0x00)\r\n", test_iocon);
  Debug_Print(buf);
  snprintf(buf, sizeof(buf), "  IODIRA: 0x%02X (expect 0x00 = outputs)\r\n", test_iodira);
  Debug_Print(buf);
  snprintf(buf, sizeof(buf), "  IODIRB: 0x%02X (expect 0x0F = inputs bits 0-3)\r\n", test_iodirb);
  Debug_Print(buf);
  snprintf(buf, sizeof(buf), "  GPPUB: 0x%02X (expect 0x0F = pull-ups on)\r\n", test_gppub);
  Debug_Print(buf);
  snprintf(buf, sizeof(buf), "  GPIOA: 0x%02X (expect 0xFF = all HIGH)\r\n", test_gpioa);
  Debug_Print(buf);
  snprintf(buf, sizeof(buf), "  GPIOB: 0x%02X (expect 0x0F = all HIGH with pull-ups)\r\n", test_gpiob);
  Debug_Print(buf);
  
  // SPI communicatie diagnose
  if (test_iodira == 0xFF && test_iodirb == 0xFF) {
    Debug_Print("\r\n  *** ERROR: SPI reads all 0xFF - possible no device! ***\r\n");
  } else if (test_iodira == 0x00 && test_iodirb == 0x00 && test_gppub == 0x00) {
    Debug_Print("\r\n  *** ERROR: SPI reads all 0x00 - check SPI wiring! ***\r\n");
  } else if (test_gpiob != 0x0F) {
    Debug_Print("\r\n  *** WARNING: GPIOB not 0x0F - check pull-ups or wiring! ***\r\n");
  } else {
    Debug_Print("\r\n  *** SUCCESS: MCP23S17 configured correctly! ***\r\n");
  }
  Debug_Print("\r\n");
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
  static uint32_t last_debug = 0;
  static bool debug_printed = false;
  uint32_t now = HAL_GetTick(); // Huidig tijdstip in milliseconden
  
  // Scan maar elke 5 ms om de processor niet onnodig te belasten
  if (now - last_scan < 5) {
    return; // Nog geen 5 ms verstreken → sla deze scan over
  }
  last_scan = now;
  
  // Print debug info elke 5 seconden
  bool print_debug = (now - last_debug > 5000);
  if (print_debug) {
    Debug_Print("\r\n--- Matrix Scan Debug ---\r\n");
    last_debug = now;
  }
  
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
    
    if (print_debug && col == 0) {
      char buf[80];
      snprintf(buf, sizeof(buf), "Col %d: Write=0x%02X, Read GPIOB=0x%02X\r\n", col, col_pattern, row_values);
      Debug_Print(buf);
    }
    
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
          
          // Debug output
          char debug_msg[50];
          snprintf(debug_msg, sizeof(debug_msg), "BTN[%d,%d] PRESSED -> Note ON %d\r\n", row, col, midi_note);
          Debug_Print(debug_msg);
          
          if (tud_mounted()) { // Controleer of de USB-verbinding actief is
            tud_midi_stream_write(0, note_on, 3); // Stuur 3 bytes via USB-MIDI
          } else {
            Debug_Print("  (USB not mounted)\r\n");
          }
        } else {
          // Knop LOSGELATEN → stuur Note OFF bericht
          // 0x80 = Note OFF commando op MIDI-kanaal 1
          // midi_note = hetzelfde nootnummer als bij Note ON
          // 0 = velocity 0 (toon stoppen)
          uint8_t note_off[3] = {0x80, midi_note, 0};
          
          // Debug output
          char debug_msg[50];
          snprintf(debug_msg, sizeof(debug_msg), "BTN[%d,%d] RELEASED -> Note OFF %d\r\n", row, col, midi_note);
          Debug_Print(debug_msg);
          
          if (tud_mounted()) { // Controleer of de USB-verbinding actief is
            tud_midi_stream_write(0, note_off, 3); // Stuur 3 bytes via USB-MIDI
          } else {
            Debug_Print("  (USB not mounted)\r\n");
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
  MX_ICACHE_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  
  // EERSTE TEST: Stuur test bericht direct na UART init
  HAL_Delay(100); // Kleine delay voor UART stabilisatie
  
 
  
  // Initialize the onboard LED
  BSP_LED_Init(LED_GREEN);
  
  // Send startup message
  Debug_Print("\r\n=================================\r\n");
  Debug_Print("MIDI Controller Initializing...\r\n");
  Debug_Print("=================================\r\n");
  
  // Initialize TinyUSB
  Debug_Print("Initializing USB MIDI...\r\n");
  tusb_init();
  
  // Initialize MCP23S17 I/O expander for button matrix
  Debug_Print("Initializing MCP23S17...\r\n");
  
  // Show which pins are actually configured for SPI3
  Debug_Print("\r\n  === SPI3 PIN CONFIGURATION ===\r\n");
  Debug_Print("  SPI3 is configured as:\r\n");
  Debug_Print("    - SPI3_MOSI = PC12\r\n");
  Debug_Print("    - SPI3_MISO = PB0\r\n");
  Debug_Print("    - SPI3_SCK  = PB1\r\n");
  Debug_Print("    - CS (manual) = PC9\r\n\r\n");
  
  // HARDWARE TEST 1: Check Chip Select pin
  Debug_Print("  Testing CS pin (PC9)...\r\n");
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  GPIO_PinState cs_high = HAL_GPIO_ReadPin(Mcp_cs_GPIO_Port, Mcp_cs_Pin);
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  GPIO_PinState cs_low = HAL_GPIO_ReadPin(Mcp_cs_GPIO_Port, Mcp_cs_Pin);
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  
  char cs_buf[80];
  snprintf(cs_buf, sizeof(cs_buf), "  CS toggle test: HIGH=%d, LOW=%d (expect 1,0)\r\n", cs_high, cs_low);
  Debug_Print(cs_buf);
  
  // CRITICAL: Test if MISO can be read as HIGH
  Debug_Print("\r\n  *** CRITICAL MISO PIN TEST ***\r\n");
  Debug_Print("  Reading MISO (PB0) state without SPI...\r\n");
  
  // Temporarily reconfigure PB0 as input with pull-up
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_Delay(10);
  
  GPIO_PinState miso_pullup = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
  
  // Try with pull-down
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_Delay(10);
  GPIO_PinState miso_pulldown = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
  
  // Restore PB0 to SPI function
  MX_SPI3_Init();
  
  char miso_buf[100];
  snprintf(miso_buf, sizeof(miso_buf), 
    "  MISO with pull-up: %d, with pull-down: %d\r\n", 
    miso_pullup, miso_pulldown);
  Debug_Print(miso_buf);
  
  if (miso_pullup == 0) {
    Debug_Print("  *** ERROR: MISO stuck LOW! ***\r\n");
    Debug_Print("  Possible causes:\r\n");
    Debug_Print("    - PB0 shorted to GND\r\n");
    Debug_Print("    - MCP23S17 SO pin damaged\r\n");
    Debug_Print("    - Wrong pin in .ioc file\r\n\r\n");
  } else if (miso_pulldown == 1) {
    Debug_Print("  *** ERROR: MISO stuck HIGH! ***\r\n");
    Debug_Print("    - PB0 shorted to 3.3V\r\n\r\n");
  } else {
    Debug_Print("  MISO pin can toggle - hardware OK!\r\n\r\n");
  }
  
  // ***** CRITICAL: Test if MCP23S17 is powered and responding *****
  Debug_Print("  *** MCP23S17 POWER TEST ***\r\n");
  Debug_Print("  Check with multimeter:\r\n");
  Debug_Print("    1. VDD (pin 9) should be 3.3V\r\n");
  Debug_Print("    2. VSS (pin 6) should be 0V (GND)\r\n");
  Debug_Print("    3. RESET (pin 18) should be 3.3V\r\n");
  Debug_Print("    4. A0, A1, A2 (pins 12,13,17) should be 0V (GND)\r\n");
  Debug_Print("    5. CS (pin 11) should be 3.3V when idle\r\n\r\n");
  
  Debug_Print("  Reading MCP23S17 IODIR defaults (should be 0xFF)...\r\n");
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
  HAL_Delay(1);
  
  uint8_t read_cmd[] = {0x41, 0x00, 0x00};  // READ opcode, IODIRA register
  uint8_t read_reply[3] = {0};
  HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi3, read_cmd, read_reply, 3, 100);
  
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  
  char power_buf[120];
  snprintf(power_buf, sizeof(power_buf), 
    "  Default IODIRA read: 0x%02X (expect 0xFF if chip is working)\r\n", 
    read_reply[2]);
  Debug_Print(power_buf);
  
  if (read_reply[2] == 0xFF) {
    Debug_Print("  SUCCESS! MCP23S17 is responding!\r\n\r\n");
  } else if (read_reply[2] == 0x00) {
    Debug_Print("  ERROR: Chip returns 0x00 - MISO stuck LOW or chip not powered!\r\n");
    Debug_Print("    -> Check VDD, RESET, and MISO wiring\r\n\r\n");
  } else {
    snprintf(power_buf, sizeof(power_buf), 
      "  WARNING: Unexpected value 0x%02X - chip may be in unknown state\r\n\r\n", 
      read_reply[2]);
    Debug_Print(power_buf);
  }
  
  // HARDWARE TEST 2: Check if SPI is working at all
  Debug_Print("\r\n  Testing SPI communication...\r\n");
  Debug_Print("  Sending: [0xAA 0x55 0xFF]\r\n");
  uint8_t spi_test_tx[3] = {0xAA, 0x55, 0xFF};
  uint8_t spi_test_rx[3] = {0x00, 0x00, 0x00};
  
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef spi_status = HAL_SPI_TransmitReceive(&hspi3, spi_test_tx, spi_test_rx, 3, 100);
  HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
  
  char spi_buf[120];
  snprintf(spi_buf, sizeof(spi_buf), 
    "  SPI Status: %d (0=OK), Received: [0x%02X 0x%02X 0x%02X]\r\n", 
    spi_status, spi_test_rx[0], spi_test_rx[1], spi_test_rx[2]);
  Debug_Print(spi_buf);
  
  if (spi_test_rx[0] == 0x00 && spi_test_rx[1] == 0x00 && spi_test_rx[2] == 0x00) {
    Debug_Print("\r\n  *** ERROR: MCP23S17 NOT RESPONDING! ***\r\n");
    Debug_Print("  Hardware checklist:\r\n");
    Debug_Print("    [ ] MCP23S17 VDD (pin 9) connected to 3.3V?\r\n");
    Debug_Print("    [ ] MCP23S17 VSS (pin 6) connected to GND?\r\n");
    Debug_Print("    [ ] MCP23S17 RESET (pin 18) connected to 3.3V?\r\n");
    Debug_Print("    [ ] A0, A1, A2 (pins 12,13,17) connected to GND?\r\n");
    Debug_Print("    [ ] MISO wire (PB0 to MCP pin 15) connected?\r\n");
    Debug_Print("    [ ] Chip orientation correct (check pin 1)?\r\n");
    Debug_Print("\r\n  💡 TIP: Try loopback test:\r\n");
    Debug_Print("     Temporarily connect PC12 (MOSI) to PB0 (MISO)\r\n");
    Debug_Print("     If you see [0xAA 0x55 0xFF], SPI pins work!\r\n\r\n");
  } else if (spi_test_rx[0] == 0xFF && spi_test_rx[1] == 0xFF && spi_test_rx[2] == 0xFF) {
    Debug_Print("\r\n  *** WARNING: SPI MISO floating (all 0xFF)! ***\r\n");
    Debug_Print("  MISO (PB0) not connected or MCP23S17 not powered\r\n\r\n");
  } else {
    Debug_Print("  ✅ SPI communication OK - MCP23S17 responding!\r\n\r\n");
  }
  
  // HARDWARE TEST 3: Try different opcodes if needed
  Debug_Print("  Testing MCP23S17 opcodes...\r\n");
  
  // Test met HAEN disabled opcode (A2-A0 = don't care when HAEN=0)
  uint8_t test_opcodes[][2] = {
    {0x40, 0x41},  // Current: A2-A0 = 000
    {0x42, 0x43},  // Try: A2-A0 = 001
    {0x4E, 0x4F}   // Try: A2-A0 = 111
  };
  
  for(int i = 0; i < 3; i++) {
    uint8_t write_op = test_opcodes[i][0];
    uint8_t read_op = test_opcodes[i][1];
    
    // Try to read IOCON with this opcode
    uint8_t test_tx[3] = {read_op, 0x0A, 0x00};  // Read IOCON register
    uint8_t test_rx[3] = {0};
    
    HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_RESET);
    for(volatile int j = 0; j < 10; j++);
    HAL_SPI_TransmitReceive(&hspi3, test_tx, test_rx, 3, 100);
    for(volatile int j = 0; j < 10; j++);
    HAL_GPIO_WritePin(Mcp_cs_GPIO_Port, Mcp_cs_Pin, GPIO_PIN_SET);
    
    char test_buf[100];
    snprintf(test_buf, sizeof(test_buf), 
      "    Opcode 0x%02X/0x%02X: IOCON = 0x%02X\r\n", 
      write_op, read_op, test_rx[2]);
    Debug_Print(test_buf);
    
    if (test_rx[2] != 0xFF && test_rx[2] != 0x00) {
      snprintf(test_buf, sizeof(test_buf),
        "    SUCCESS: Found working opcode! Use 0x%02X (write) / 0x%02X (read)\r\n\r\n",
        write_op, read_op);
      Debug_Print(test_buf);
      break;
    }
  }
  
  MCP23S17_Init();
  Debug_Print("MCP23S17 configured\r\n");
  
  // Initialize button states
  Debug_Print("Initializing 4x4 button matrix...\r\n");
  for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
      button_matrix[r][c].current_state = false;
      button_matrix[r][c].previous_state = false;
      button_matrix[r][c].last_change_time = 0;
      button_matrix[r][c].debounce_stable = false;
    }
  }
  
  Debug_Print("Initialization complete!\r\n");
  Debug_Print("Waiting for button presses...\r\n\r\n");

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  uint32_t last_heartbeat = 0;
  
  while (1)
  {
    // Heartbeat LED - blink every second to show code is running
    uint32_t now = HAL_GetTick();
    if (now - last_heartbeat > 1000) {
      BSP_LED_Toggle(LED_GREEN);
      last_heartbeat = now;
    }
    
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
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

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
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
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
  huart2.Init.BaudRate = 115200;
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
  HAL_GPIO_WritePin(GPIOC, Cs_Pin_Pin|Mcp_cs_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : Cs_Pin_Pin Mcp_cs_Pin */
  GPIO_InitStruct.Pin = Cs_Pin_Pin|Mcp_cs_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

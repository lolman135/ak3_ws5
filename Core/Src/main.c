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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "i2s.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint8_t note_freq;    // Frequency register value for CS43L22
    uint16_t duration;    // Duration in milliseconds
} Note_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// CS43L22 I2C Address
#define CS43L22_ADDR        0x94  // 0x4A << 1

// CS43L22 Register Addresses
#define CS43L22_REG_ID              0x01
#define CS43L22_REG_POWER_CTRL1     0x02
#define CS43L22_REG_POWER_CTRL2     0x04
#define CS43L22_REG_CLOCKING_CTRL   0x05
#define CS43L22_REG_INTERFACE_CTRL1 0x06
#define CS43L22_REG_BEEP_FREQ_TIME  0x1C
#define CS43L22_REG_BEEP_VOL_TONE   0x1D
#define CS43L22_REG_BEEP_TONE_CFG   0x1E
#define CS43L22_REG_MASTER_A_VOL    0x20
#define CS43L22_REG_MASTER_B_VOL    0x21
#define CS43L22_REG_HP_A_VOL        0x22
#define CS43L22_REG_HP_B_VOL        0x23

// Beep Generator Frequency Values
#define BEEP_OFF    0x00
#define BEEP_G4     0x5C
#define BEEP_DS4    0x4C
#define BEEP_AS4    0x68
#define BEEP_D5     0x78
#define BEEP_DS5    0x7C
#define BEEP_FS4    0x58
#define BEEP_G5     0x8C
#define BEEP_FS5    0x88
#define BEEP_F5     0x84
#define BEEP_E5     0x80
#define BEEP_GS4    0x60
#define BEEP_CS5    0x74
#define BEEP_C5     0x70
#define BEEP_B4     0x6C
#define BEEP_A4     0x64

// LED pins
#define LED_GREEN   GPIO_PIN_12
#define LED_ORANGE  GPIO_PIN_13
#define LED_RED     GPIO_PIN_14
#define LED_BLUE    GPIO_PIN_15
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;
extern I2S_HandleTypeDef hi2s3;

const Note_t imperial_march[] = {
    {BEEP_G4, 500}, {BEEP_G4, 500}, {BEEP_G4, 500}, {BEEP_DS4, 350}, {BEEP_AS4, 150},
    {BEEP_G4, 500}, {BEEP_DS4, 350}, {BEEP_AS4, 150}, {BEEP_G4, 1000},
    {BEEP_D5, 500}, {BEEP_D5, 500}, {BEEP_D5, 500}, {BEEP_DS5, 350}, {BEEP_AS4, 150},
    {BEEP_FS4, 500}, {BEEP_DS4, 350}, {BEEP_AS4, 150}, {BEEP_G4, 1000}
};

const uint8_t melody_length = sizeof(imperial_march) / sizeof(Note_t);
static uint8_t play_count = 0;
static uint8_t current_note = 0;
static uint32_t note_start_time = 0;
static uint8_t is_playing = 0;
static uint32_t led_last_update = 0;
static uint8_t led_pattern = 0;
static uint8_t melody_finished = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
HAL_StatusTypeDef CS43L22_Init(void);
HAL_StatusTypeDef CS43L22_WriteReg(uint8_t reg, uint8_t value);
uint8_t CS43L22_ReadReg(uint8_t reg);
void CS43L22_PlayBeep(uint8_t frequency, uint8_t volume);
void CS43L22_StopBeep(void);
void Play_Imperial_March(void);
void Update_LED_Show(uint8_t note_frequency);
void Reset_LEDs(void);
/* USER CODE END PFP */

/* Main Function */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2S3_Init();

  /* USER CODE BEGIN 2 */
  // Hardware Reset DAC
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);

  if (CS43L22_Init() != HAL_OK) {
      while(1) { // Blink Red if error
          HAL_GPIO_TogglePin(GPIOD, LED_RED);
          HAL_Delay(100);
      }
  }

  // Brief LED flash to confirm init
  HAL_GPIO_WritePin(GPIOD, LED_GREEN|LED_ORANGE|LED_RED|LED_BLUE, GPIO_PIN_SET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(GPIOD, LED_GREEN|LED_ORANGE|LED_RED|LED_BLUE, GPIO_PIN_RESET);
  HAL_Delay(1000);
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    Play_Imperial_March();
    HAL_Delay(10);
    /* USER CODE END WHILE */
  }
}

/* USER CODE BEGIN 4 */
HAL_StatusTypeDef CS43L22_WriteReg(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return HAL_I2C_Master_Transmit(&hi2c1, CS43L22_ADDR, data, 2, 100);
}

uint8_t CS43L22_ReadReg(uint8_t reg) {
    uint8_t value = 0;
    HAL_I2C_Master_Transmit(&hi2c1, CS43L22_ADDR, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c1, CS43L22_ADDR, &value, 1, 100);
    return value;
}

HAL_StatusTypeDef CS43L22_Init(void) {
    HAL_StatusTypeDef status = HAL_OK;
    status |= CS43L22_WriteReg(CS43L22_REG_POWER_CTRL1, 0x01); // Power Up
    status |= CS43L22_WriteReg(CS43L22_REG_CLOCKING_CTRL, 0x81); // Auto-clock
    status |= CS43L22_WriteReg(CS43L22_REG_INTERFACE_CTRL1, 0x04); // I2S
    status |= CS43L22_WriteReg(CS43L22_REG_MASTER_A_VOL, 0x18);
    status |= CS43L22_WriteReg(CS43L22_REG_MASTER_B_VOL, 0x18);
    status |= CS43L22_WriteReg(CS43L22_REG_BEEP_TONE_CFG, 0x80);
    status |= CS43L22_WriteReg(CS43L22_REG_POWER_CTRL2, 0xAF); // HP/Speakers On
    return status;
}

void CS43L22_PlayBeep(uint8_t frequency, uint8_t volume) {
    CS43L22_WriteReg(CS43L22_REG_BEEP_FREQ_TIME, frequency);
    CS43L22_WriteReg(CS43L22_REG_BEEP_VOL_TONE, (volume & 0x0F) | 0x80);
    CS43L22_WriteReg(CS43L22_REG_BEEP_TONE_CFG, 0xC0);
}

void CS43L22_StopBeep(void) {
    CS43L22_WriteReg(CS43L22_REG_BEEP_TONE_CFG, 0x80);
    Reset_LEDs();
}

void Update_LED_Show(uint8_t note_frequency) {
    uint32_t current_time = HAL_GetTick();
    if (current_time - led_last_update > 100) {
        led_last_update = current_time;
        HAL_GPIO_WritePin(GPIOD, LED_GREEN|LED_ORANGE|LED_RED|LED_BLUE, GPIO_PIN_RESET);
        if (note_frequency != BEEP_OFF) {
            uint16_t pins[] = {LED_GREEN, LED_ORANGE, LED_RED, LED_BLUE};
            HAL_GPIO_WritePin(GPIOD, pins[led_pattern % 4], GPIO_PIN_SET);
            led_pattern++;
        }
    }
}

void Reset_LEDs(void) {
    HAL_GPIO_WritePin(GPIOD, LED_GREEN|LED_ORANGE|LED_RED|LED_BLUE, GPIO_PIN_RESET);
}

void Play_Imperial_March(void) {
    if (melody_finished) return;
    uint32_t current_time = HAL_GetTick();

    if (!is_playing) {
        if (play_count < 3) {
            is_playing = 1; current_note = 0; note_start_time = current_time;
        } else {
            melody_finished = 1; Reset_LEDs(); return;
        }
    }

    if (current_time - note_start_time >= imperial_march[current_note].duration) {
        CS43L22_StopBeep();
        current_note++;
        if (current_note >= melody_length) {
            play_count++; is_playing = 0;
            if (play_count < 3) HAL_Delay(1000);
            return;
        }
        note_start_time = current_time;
    }

    if (imperial_march[current_note].note_freq != BEEP_OFF) {
        CS43L22_PlayBeep(imperial_march[current_note].note_freq, 0x0A);
    }
    Update_LED_Show(imperial_march[current_note].note_freq);
}
/* USER CODE END 4 */

/**
  * @brief System Clock Configuration (Auto-generated)
  */
void SystemClock_Config(void)
{

}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

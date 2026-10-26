/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* Space for your global variables. */

#define MESSAGE_LIMIT 35U
#define MESSAGE_QUEUE_SIZE 32U

typedef struct
{
  char text[MESSAGE_LIMIT];
  letter_count_ count;
} received_message;

volatile uint32_t dropped_messages;
letter_count_ letter_count;
static received_message messages[MESSAGE_QUEUE_SIZE];
static volatile uint8_t message_write;
static volatile uint8_t message_read;
static char tx_buffer[160];


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  USART2_RegisterCallback(proccesDmaData);
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/



  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

      if (!LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7))
      {
          int length = 0;
          if (message_read != message_write)
          {
              received_message *message = &messages[message_read];
              length = snprintf(tx_buffer, sizeof(tx_buffer),
                  "Valid string: %s, lower-case: %u, upper-case: %u\r\n",
                  message->text, (unsigned)message->count.small_letter,
                  (unsigned)message->count.capital_letter);
              __DMB();
              message_read = (message_read + 1U) % MESSAGE_QUEUE_SIZE;
          }
          if (length > 0 && (unsigned)length < sizeof(tx_buffer))
              USART2_PutBuffer((uint8_t *)tx_buffer, (uint8_t)length);
      }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
    static received_message current;
    static uint8_t active;
    static uint8_t length;

    if (sign == '#')
    {
        active = 1;
        length = 0;
        current.count.small_letter = 0;
        current.count.capital_letter = 0;
        return;
    }
    if (!active) return;

    if (sign == '$')
    {
        current.text[length] = '\0';
        letter_count = current.count;
        uint8_t next = (message_write + 1U) % MESSAGE_QUEUE_SIZE;
        if (next != message_read)
        {
            messages[message_write] = current;
            __DMB();
            message_write = next;
        }
        else
            dropped_messages++;
        active = 0;
        return;
    }

    /* The terminator must occur within 35 characters after '#'. */
    if (length == MESSAGE_LIMIT - 1U)
    {
        active = 0;
        return;
    }
    current.text[length++] = (char)sign;
    if (sign >= 'a' && sign <= 'z') current.count.small_letter++;
    if (sign >= 'A' && sign <= 'Z') current.count.capital_letter++;
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

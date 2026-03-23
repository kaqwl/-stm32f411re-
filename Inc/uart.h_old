#ifndef __UART_H
#define __UART_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define UART_RX_BUFFER_SIZE 64

void UART_Init(void);
void UART_SendChar(char c);
void UART_SendString(const char *str);
void UART_SendInt(int num);

void UART_StartReceive(void);
uint8_t UART_HasData(void);
char UART_GetChar(void);
void UART_ProcessInput(void);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif
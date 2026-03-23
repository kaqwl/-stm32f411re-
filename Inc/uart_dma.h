#ifndef UART_DMA_H
#define UART_DMA_H

#include "stm32f4xx_hal.h"

/* Размеры буферов */
#define UART_DMA_BUFFER_SIZE     256    /* Буфер для DMA (должен быть степенью двойки) */
#define UART_RING_BUFFER_SIZE    1024   /* Кольцевой буфер приложения */

/* Внешние переменные */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;

extern uint8_t dma_buffer[UART_DMA_BUFFER_SIZE];
extern uint8_t ring_buffer[UART_RING_BUFFER_SIZE];
extern volatile uint16_t ring_head;
extern volatile uint16_t ring_tail;

/* Публичные функции */
void UART_DMA_Init(void);
uint16_t UART_Read(uint8_t *data, uint16_t max_len);
void UART_Print(const char *str);
void UART_Send(uint8_t *data, uint16_t len);
void UART_Process(void);
void UART_ProcessLines(void);
void UART_SetCallback(void (*callback)(uint8_t data));

#endif
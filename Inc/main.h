#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

// Объявление глобального UART handle
extern UART_HandleTypeDef huart1;

// Функция обработки ошибок
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
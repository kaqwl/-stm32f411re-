#ifndef __ALARM_SYSTEM_H
#define __ALARM_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "stdint.h"
#include "stdbool.h"
#include "stm32f4xx_hal.h"        // ДОБАВЛЕНО: для UART_HandleTypeDef

// Состояния системы
typedef enum {
    ALARM_STATE_IDLE = 0,
    ALARM_STATE_TRIGGERED,
    ALARM_STATE_SENDING,
    ALARM_STATE_ERROR
} AlarmState_t;

// Структура для конфигурации системы
typedef struct {
    char wifi_ssid[32];
    char wifi_password[32];
    char bot_token[64];
    char chat_id[32];
    uint32_t cooldown_ms;
    uint8_t max_retries;
} AlarmConfig_t;

// Глобальные переменные (extern)
extern SemaphoreHandle_t xMotionSemaphore;
extern QueueHandle_t xUartRxQueue;
extern volatile AlarmState_t currentAlarmState;

// Функции инициализации
void AlarmSystem_Init(AlarmConfig_t *config);
void AlarmTask_Init(void);
void MotionTask_Init(void);

// Функции для прерываний
void Alarm_EXTI_Callback(uint16_t GPIO_Pin);
void Alarm_UART_Callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __ALARM_SYSTEM_H */
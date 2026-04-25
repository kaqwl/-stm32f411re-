#include "alarm_system.h"
#include "esp8266.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "string.h"
#include "stdio.h"

// Глобальные переменные
SemaphoreHandle_t xMotionSemaphore = NULL;
QueueHandle_t xUartRxQueue = NULL;
volatile AlarmState_t currentAlarmState = ALARM_STATE_IDLE;

// Локальные переменные
static TaskHandle_t xMotionTaskHandle = NULL;
static TaskHandle_t xAlarmTaskHandle = NULL;
static AlarmConfig_t alarmConfig;
static uint8_t uartRxByte;

// Прототипы локальных функций
static void MotionTask(void *pvParameters);
static void AlarmTask(void *pvParameters);
static void ProcessAlarm(void);

// Правильные определения для Nucleo-F411RE
#define LED2_GPIO_PORT GPIOA
#define LED2_PIN       GPIO_PIN_5

/**
 * @brief Инициализация системы сигнализации
 */
void AlarmSystem_Init(AlarmConfig_t *config)
{
    // Копируем конфигурацию
    memcpy(&alarmConfig, config, sizeof(AlarmConfig_t));
    
    // Создаём бинарный семафор для обработки прерываний
    xMotionSemaphore = xSemaphoreCreateBinary();
    if(xMotionSemaphore == NULL) {
        // Вместо Error_Handler() используем бесконечный цикл
        while(1) {
            // Мигаем светодиодом для индикации ошибки
            HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
            HAL_Delay(100);
        }
    }
    
    // Создаём очередь для данных UART (256 байт)
    xUartRxQueue = xQueueCreate(256, sizeof(uint8_t));
    if(xUartRxQueue == NULL) {
        while(1) {
            HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
            HAL_Delay(200);
        }
    }
    
    // Инициализируем состояние
    currentAlarmState = ALARM_STATE_IDLE;
}

/**
 * @brief Создание задачи обработки движения
 */
void MotionTask_Init(void)
{
    BaseType_t result = xTaskCreate(
        MotionTask,
        "MotionTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xMotionTaskHandle
    );
    
    if(result != pdPASS) {
        while(1) {
            HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
            HAL_Delay(500);
        }
    }
}

/**
 * @brief Создание задачи тревоги
 */
void AlarmTask_Init(void)
{
    BaseType_t result = xTaskCreate(
        AlarmTask,
        "AlarmTask",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        tskIDLE_PRIORITY + 1,
        &xAlarmTaskHandle
    );
    
    if(result != pdPASS) {
        while(1) {
            HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
            HAL_Delay(500);
        }
    }
}

/**
 * @brief Задача обработки движения (ждёт семафор)
 */
static void MotionTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;) {
        // Ждём семафор от прерывания (бесконечно)
        if(xSemaphoreTake(xMotionSemaphore, portMAX_DELAY) == pdTRUE) {
            // Проверяем, не находимся ли уже в состоянии тревоги
            if(currentAlarmState == ALARM_STATE_IDLE) {
                currentAlarmState = ALARM_STATE_TRIGGERED;
                
                // Защита от ложных срабатываний (антидребезг)
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}

/**
 * @brief Основная задача тревоги (отправка уведомлений)
 */
static void AlarmTask(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;) {
        switch(currentAlarmState) {
            case ALARM_STATE_IDLE:
                // Спокойное состояние - светодиод выключен
                HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_RESET);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case ALARM_STATE_TRIGGERED:
                // Получен сигнал тревоги
                currentAlarmState = ALARM_STATE_SENDING;
                HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_PIN, GPIO_PIN_SET);
                break;
                
            case ALARM_STATE_SENDING:
                // Отправка уведомления
                ProcessAlarm();
                
                // Возвращаемся в спокойное состояние
                currentAlarmState = ALARM_STATE_IDLE;
                
                // Пауза между срабатываниями
                vTaskDelay(pdMS_TO_TICKS(alarmConfig.cooldown_ms));
                break;
                
            case ALARM_STATE_ERROR:
                // Обработка ошибок - мигаем светодиодом
                for(int i = 0; i < 5; i++) {
                    HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                currentAlarmState = ALARM_STATE_IDLE;
                break;
        }
    }
}

/**
 * @brief Обработка тревоги (отправка сообщения)
 */
static void ProcessAlarm(void)
{
    char message[128];
    ESP_Status_t status;
    uint8_t retry_count = 0;
    
    // Формируем сообщение
    snprintf(message, sizeof(message), 
             "ALARM! Motion detected at %lu ms", 
             HAL_GetTick());
    
    do {
        // Пытаемся отправить сообщение
        status = ESP_SendTelegramMessage(
            (char*)alarmConfig.bot_token,
            (char*)alarmConfig.chat_id,
            message
        );
        
        retry_count++;
        
        if(status != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
    } while(status != ESP_OK && retry_count < alarmConfig.max_retries);
    
    if(status != ESP_OK) {
        currentAlarmState = ALARM_STATE_ERROR;
    }
}

/**
 * @brief Callback для прерывания по пину PA4 (датчик движения)
 */
void Alarm_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_5) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // Отдаём семафор из прерывания
        xSemaphoreGiveFromISR(xMotionSemaphore, &xHigherPriorityTaskWoken);
        
        // Переключаем контекст если нужно
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief Callback для прерывания UART (приём данных от ESP-01)
 */
void Alarm_UART_Callback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Отправляем принятый байт в очередь
    xQueueSendFromISR(xUartRxQueue, &uartRxByte, &xHigherPriorityTaskWoken);
    
    // Перезапускаем приём
    HAL_UART_Receive_IT(huart, &uartRxByte, 1);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#ifndef __ESP8266_H
#define __ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "stdint.h"
#include "stdbool.h"

// Статусы работы ESP8266
typedef enum {
    ESP_OK = 0,
    ESP_ERROR,
    ESP_TIMEOUT,
    ESP_BUSY,
    ESP_NO_RESPONSE
} ESP_Status_t;

// Структура для хранения состояния ESP
typedef struct {
    UART_HandleTypeDef *huart;
    bool is_connected;
    bool is_wifi_connected;
    char ip_address[16];
    uint32_t last_command_time;
} ESP_Handle_t;

// Инициализация и базовые функции
void ESP_Init(UART_HandleTypeDef *huart);
ESP_Status_t ESP_TestConnection(void);
ESP_Status_t ESP_Reset(void);

// WiFi функции
ESP_Status_t ESP_SetMode(uint8_t mode);
ESP_Status_t ESP_ConnectWiFi(char *ssid, char *password);
ESP_Status_t ESP_DisconnectWiFi(void);
ESP_Status_t ESP_GetIP(char *ip_buffer);

// TCP/IP функции
ESP_Status_t ESP_ConnectTCP(char *host, uint16_t port);
ESP_Status_t ESP_SendData(char *data, uint32_t len);
ESP_Status_t ESP_CloseConnection(void);

// Высокоуровневые функции
ESP_Status_t ESP_SendTelegramMessage(char *bot_token, char *chat_id, char *message);
ESP_Status_t ESP_SendHTTPRequest(char *host, char *path, char *data);

// Вспомогательные функции
void ESP_SetReceiveCallback(void (*callback)(uint8_t));
ESP_Status_t ESP_WaitForResponse(char *expected, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_H */
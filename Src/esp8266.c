#include "esp8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "string.h"
#include "stdio.h"

// Внешняя очередь (определена в alarm_system.c)
extern QueueHandle_t xUartRxQueue;

// Локальные переменные
static UART_HandleTypeDef *esp_huart = NULL;
static char tx_buffer[512];
static char rx_buffer[1024];

/**
 * @brief Отправка команды ESP8266
 */
static void ESP_SendCommand(char *cmd)
{
    snprintf(tx_buffer, sizeof(tx_buffer), "%s\r\n", cmd);
    HAL_UART_Transmit(esp_huart, (uint8_t*)tx_buffer, strlen(tx_buffer), 1000);
}

/**
 * @brief Инициализация ESP8266
 */
void ESP_Init(UART_HandleTypeDef *huart)
{
    esp_huart = huart;
}

/**
 * @brief Проверка связи с ESP8266
 */
ESP_Status_t ESP_TestConnection(void)
{
    ESP_SendCommand("AT");
    return ESP_WaitForResponse("OK", 2000);
}

/**
 * @brief Ожидание ответа от ESP8266
 */
ESP_Status_t ESP_WaitForResponse(char *expected, uint32_t timeout_ms)
{
    uint32_t start_tick = xTaskGetTickCount();
    uint32_t rx_index = 0;
    uint8_t rx_byte;
    
    memset(rx_buffer, 0, sizeof(rx_buffer));
    
    while((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms)) {
        if(xQueueReceive(xUartRxQueue, &rx_byte, pdMS_TO_TICKS(100)) == pdTRUE) {
            rx_buffer[rx_index++] = rx_byte;
            
            // Проверяем на наличие ожидаемой строки
            if(strstr(rx_buffer, expected) != NULL) {
                return ESP_OK;
            }
            
            // Проверяем на ошибку
            if(strstr(rx_buffer, "ERROR") != NULL) {
                return ESP_ERROR;
            }
            
            // Защита от переполнения буфера
            if(rx_index >= sizeof(rx_buffer) - 1) {
                rx_index = 0;
            }
        }
    }
    
    return ESP_TIMEOUT;
}

/**
 * @brief Установка режима ESP8266
 */
ESP_Status_t ESP_SetMode(uint8_t mode)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", mode);
    ESP_SendCommand(cmd);
    return ESP_WaitForResponse("OK", 2000);
}

/**
 * @brief Подключение к WiFi
 */
ESP_Status_t ESP_ConnectWiFi(char *ssid, char *password)
{
    char cmd[128];
    ESP_Status_t status;
    
    // Устанавливаем режим станции
    status = ESP_SetMode(1);
    if(status != ESP_OK) return status;
    
    // Подключаемся к WiFi
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    ESP_SendCommand(cmd);
    
    // Ждём подтверждения подключения
    status = ESP_WaitForResponse("OK", 10000);
    if(status != ESP_OK) return status;
    
    // Даём время на получение IP
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    return ESP_OK;
}

/**
 * @brief Подключение к TCP серверу
 */
ESP_Status_t ESP_ConnectTCP(char *host, uint16_t port)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    ESP_SendCommand(cmd);
    
    ESP_Status_t status = ESP_WaitForResponse("CONNECT", 5000);
    if(status != ESP_OK) return status;
    
    return ESP_WaitForResponse("OK", 1000);
}

/**
 * @brief Отправка данных через установленное соединение
 */
ESP_Status_t ESP_SendData(char *data, uint32_t len)
{
    char cmd[32];
    
    // Команда на отправку данных
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%lu", len);
    ESP_SendCommand(cmd);
    
    // Ждём приглашение ">"
    ESP_Status_t status = ESP_WaitForResponse(">", 2000);
    if(status != ESP_OK) return status;
    
    // Отправляем сами данные
    HAL_UART_Transmit(esp_huart, (uint8_t*)data, len, 2000);
    
    // Ждём подтверждение отправки
    return ESP_WaitForResponse("SEND OK", 5000);
}

/**
 * @brief Закрытие TCP соединения
 */
ESP_Status_t ESP_CloseConnection(void)
{
    ESP_SendCommand("AT+CIPCLOSE");
    return ESP_WaitForResponse("OK", 2000);
}

/**
 * @brief Отправка сообщения в Telegram
 */
ESP_Status_t ESP_SendTelegramMessage(char *bot_token, char *chat_id, char *message)
{
    ESP_Status_t status;
    char http_request[512];
    char encoded_message[256];
    
    // Простое кодирование пробелов
    char *src = message;
    char *dst = encoded_message;
    while(*src) {
        if(*src == ' ') {
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '0';
        } else if(*src == '\n') {
            *dst++ = '%';
            *dst++ = '0';
            *dst++ = 'A';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    // Формируем HTTP GET запрос
    snprintf(http_request, sizeof(http_request),
        "GET /bot%s/sendMessage?chat_id=%s&text=%s HTTP/1.1\r\n"
        "Host: api.telegram.org\r\n"
        "Connection: close\r\n"
        "\r\n",
        bot_token, chat_id, encoded_message);
    
    // Подключаемся к серверу Telegram
    status = ESP_ConnectTCP("api.telegram.org", 80);
    if(status != ESP_OK) return status;
    
    // Отправляем HTTP запрос
    status = ESP_SendData(http_request, strlen(http_request));
    if(status != ESP_OK) {
        ESP_CloseConnection();
        return status;
    }
    
    // Закрываем соединение
    ESP_CloseConnection();
    
    return ESP_OK;
}
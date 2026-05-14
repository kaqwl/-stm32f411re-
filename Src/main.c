#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

volatile uint8_t motion_detected = 0;
char uart_rx_buffer[512];
uint16_t uart_rx_index = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void ESP_SendCommand(char *cmd);
int ESP_WaitFor(char *expected, uint32_t timeout_ms);
int ESP_Test(void);
int ESP_ConnectWiFi(char *ssid, char *pass);
int SendNtfy(char *topic, char *text);

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
    return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_6) {
        static uint32_t last_time = 0;
        uint32_t now = HAL_GetTick();
        if(now - last_time > 500) {
            motion_detected = 1;
            printf("[PIR] Motion detected!\r\n");
            last_time = now;
        }
    }
}

void ESP_SendCommand(char *cmd)
{
    printf("[ESP] TX: %s\r\n", cmd);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s\r", cmd);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 2000);
}

int ESP_WaitFor(char *expected, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while((HAL_GetTick() - start) < timeout_ms) {
        if(USART1->SR & USART_SR_RXNE) {
            uint8_t byte = USART1->DR;
            if(uart_rx_index < sizeof(uart_rx_buffer) - 1) {
                uart_rx_buffer[uart_rx_index++] = byte;
                uart_rx_buffer[uart_rx_index] = '\0';
            }
            if(strstr(uart_rx_buffer, expected) != NULL) {
                printf("[ESP] RX: %s\r\n", uart_rx_buffer);
                uart_rx_index = 0;
                memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
                return 1;
            }
        }
    }
    printf("[ESP] Timeout: %s. Buffer: %s\r\n", expected, uart_rx_buffer);
    uart_rx_index = 0;
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    return 0;
}

int ESP_Test(void)
{
    printf("[INIT] Testing ESP-01...\r\n");
    uart_rx_index = 0;
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    ESP_SendCommand("AT");
    return ESP_WaitFor("OK", 2000);
}

int ESP_ConnectWiFi(char *ssid, char *pass)
{
    char cmd[128];
    uart_rx_index = 0;
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));

    printf("[WiFi] Connecting to: %s\r\n", ssid);
    ESP_SendCommand("AT+CWMODE=1");
    HAL_Delay(500);
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, pass);
    ESP_SendCommand(cmd);
    if(ESP_WaitFor("OK", 15000)) {
        printf("[WiFi] Connected!\r\n");
        HAL_Delay(2000);
        return 1;
    }
    printf("[WiFi] Failed!\r\n");
    return 0;
}

int SendNtfy(char *topic, char *text)
{
    uart_rx_index = 0;
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+NTFY=%s,%s", topic, text);
    ESP_SendCommand(cmd);
    
    if(ESP_WaitFor("OK", 10000)) {
        printf("[ALARM] Ntfy sent!\r\n");
        return 1;
    }
    printf("[ALARM] Ntfy failed\r\n");
    return 0;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    printf("\r\n========================================\r\n");
    printf("  STM32F411 IoT ALARM SYSTEM v3.0 (Ntfy)\r\n");
    printf("========================================\r\n");
    printf("System Clock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("----------------------------------------\r\n");
    
    char *wifi_ssid = "Galaxy";
    char *wifi_pass = "qpalzmthou102";
    char *ntfy_topic = "myalarm123";
    
    HAL_Delay(500);
    
    printf("[INIT] Testing ESP-01...\r\n");
    if(ESP_Test())
        printf("[INIT] ESP-01: ONLINE\r\n");
    else
        printf("[INIT] ESP-01: OFFLINE\r\n");
    
    printf("----------------------------------------\r\n");
    printf("  SYSTEM READY. Waiting for motion...\r\n");
    printf("========================================\r\n\r\n");
    
    while(1) {
        if(motion_detected) {
            motion_detected = 0;
            printf("[ALARM] Processing...\r\n");
            
            if(ESP_ConnectWiFi(wifi_ssid, wifi_pass)) {
                char msg[64];
                snprintf(msg, sizeof(msg), "ALARM! Motion at %lums", HAL_GetTick());
                if(SendNtfy(ntfy_topic, msg))
                    printf("[ALARM] Sent OK!\r\n");
                else
                    printf("[ALARM] Send failed\r\n");
            }
            
            printf("[ALARM] Waiting 7s cooldown...\r\n");
            HAL_Delay(7000);
            printf("[ALARM] Ready.\r\n");
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 200;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Pin = GPIO_PIN_6;
    g.Mode = GPIO_MODE_IT_RISING;
    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_15;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FAST;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &g);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
    SET_BIT(USART1->CR1, USART_CR1_RE);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_2;
    g.Mode = GPIO_MODE_AF_PP;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

void Error_Handler(void) { while(1) {} }
int _close(int f) { return -1; }
int _fstat(int f, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int f) { return 1; }
int _lseek(int f, int p, int d) { return 0; }
int _read(int f, char *p, int l) { return 0; }
void _exit(int s) { while(1); }
int _kill(int p, int s) { return -1; }
int _getpid(void) { return 1; }
void _init(void) {}
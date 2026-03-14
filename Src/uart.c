#include "uart.h"

UART_HandleTypeDef huart2;

// Кольцевой буфер для приёма
volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;
volatile uint8_t rx_data = 0;

char cmd_buffer[UART_RX_BUFFER_SIZE];
volatile uint8_t cmd_index = 0;

void UART_Init(void)
{
    // 1. Включаем тактирование GPIOA и USART2
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    // 2. Настраиваем PA2 (TX) и PA3 (RX)
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2; // Альтернативная функция 7
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. Настраиваем USART2
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 57600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);

    // 4. Включаем прерывания для UART2 в NVIC
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void UART_StartReceive(void)
{
    /*
     Эта функция дает команду UART-периферии начать прием данных в фоновом режиме.
     Она "говорит" микроконтроллеру: "Начинай слушать порт и складывай все, что придет, в этот буфер.
     Как только наберется нужное количество байт — прерви меня, чтобы я мог с этим данными что-то сделать."
     */
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_data, 1);
}

void UART_SendChar(char c)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, 10);
}

void UART_SendString(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

void UART_SendInt(int num)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", num);
    UART_SendString(buf);
}

uint8_t UART_HasData(void)
{
    return (rx_head != rx_tail);
}

char UART_GetChar(void)
{
    if (rx_head == rx_tail)
        return 0;

    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;
    return c;
}

void UART_ProcessInput(void)
{
    while (UART_HasData())
    {
        char c = UART_GetChar();

        // Эхо: отправляем обратно то, что получили
        UART_SendChar(c);

        // Обработка конца строки (Enter = \r или \n)
        if (c == '\r' || c == '\n')
        {
            if (cmd_index > 0)
            {
                cmd_buffer[cmd_index] = '\0';
                UART_SendString("\r\n[CMD] ");
                UART_SendString(cmd_buffer);
                UART_SendString("\r\n");

                // === ОБРАБОТКА КОМАНД ===

                // Команда: LED
                if (strcmp(cmd_buffer, "LED") == 0)
                {
                    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
                    UART_SendString("[OK] LED toggled\r\n");
                }
                // Команда: HELP
                else if (strcmp(cmd_buffer, "HELP") == 0)
                {
                    UART_SendString("=== Available Commands ===\r\n");
                    UART_SendString("  LED   - Toggle onboard LED\r\n");
                    UART_SendString("  HELP  - Show this help\r\n");
                    UART_SendString("  RESET - Reset the board\r\n");
                    UART_SendString("  INFO  - Show system info\r\n");
                    UART_SendString("=========================\r\n");
                }
                // Команда: RESET
                else if (strcmp(cmd_buffer, "RESET") == 0)
                {
                    UART_SendString("Resetting in 1s...\r\n");
                    HAL_Delay(1000);
                    NVIC_SystemReset();
                }
                // Команда: INFO
                else if (strcmp(cmd_buffer, "INFO") == 0)
                {
                    char info[128];
                    snprintf(info, sizeof(info),
                             "SYSCLK: %ld Hz\r\n"
                             "HCLK:   %ld Hz\r\n"
                             "Uptime: %lu ms\r\n",
                             HAL_RCC_GetHCLKFreq(),
                             HAL_RCC_GetHCLKFreq(),
                             HAL_GetTick());
                    UART_SendString(info);
                }
                else if (strcmp(cmd_buffer, "CLK") == 0)
                {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "SYSCLK: %ld MHz\r\n"
                             "HCLK:   %ld MHz\r\n"
                             "PCLK1:  %ld MHz (APB1)\r\n"
                             "PCLK2:  %ld MHz (APB2)\r\n",
                             HAL_RCC_GetSysClockFreq() / 1000000,
                             HAL_RCC_GetHCLKFreq() / 1000000,
                             HAL_RCC_GetPCLK1Freq() / 1000000,
                             HAL_RCC_GetPCLK2Freq() / 1000000);
                    UART_SendString(buf);
                }
                // Неизвестная команда
                else
                {
                    UART_SendString("[ERR] Unknown command. Type HELP\r\n");
                }

                // Сброс буфера команд
                cmd_index = 0;
            }
            else
            {
                UART_SendString("\r\n> ");
            }
        }
        // Накопление символов команды
        else if (cmd_index < (UART_RX_BUFFER_SIZE - 1))
        {
            cmd_buffer[cmd_index++] = c;
        }
    }
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        // Сохраняем байт в кольцевой буфер
        rx_buffer[rx_head] = rx_data;
        rx_head = (rx_head + 1) % UART_RX_BUFFER_SIZE;

        // Запускаем приём следующего байта
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_data, 1);
    }
}

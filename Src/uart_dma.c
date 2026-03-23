#include "uart_dma.h"
#include <string.h>

/* UART и DMA handles */
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* Буферы */
uint8_t dma_buffer[UART_DMA_BUFFER_SIZE];
uint8_t ring_buffer[UART_RING_BUFFER_SIZE];
volatile uint16_t ring_head = 0;
volatile uint16_t ring_tail = 0;

/* Включить IDLE detection */
void UART_EnableIdleDetection(void)
{
    /* Включаем прерывание по IDLE линии */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

/* Инициализация UART2 с DMA */
void UART_DMA_Init(void)
{
    /* 1. Включаем тактирование */
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* 2. Настраиваем пины PA2 (TX) и PA3 (RX) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 3. Настраиваем USART2 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 57600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);

    /* 4. Настраиваем DMA для USART2_RX */
    hdma_usart2_rx.Instance = DMA1_Stream5;
    hdma_usart2_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode = DMA_CIRCULAR;
    hdma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_usart2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart2_rx);

    /* 5. Связываем DMA с UART */
    __HAL_LINKDMA(&huart2, hdmarx, hdma_usart2_rx);

    /* 6. Настраиваем приоритеты прерываний */
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

    /* 7. Запускаем DMA прием в круговом режиме */
    HAL_UART_Receive_DMA(&huart2, dma_buffer, UART_DMA_BUFFER_SIZE);

    UART_EnableIdleDetection();
}

/* Callback при полузаполнении DMA буфера */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* Копируем первую половину DMA буфера в кольцевой буфер */
        for (uint16_t i = 0; i < UART_DMA_BUFFER_SIZE / 2; i++)
        {
            ring_buffer[ring_head] = dma_buffer[i];
            ring_head = (ring_head + 1) % UART_RING_BUFFER_SIZE;
        }
    }
}

/* Callback при полном заполнении DMA буфера */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* Копируем вторую половину DMA буфера в кольцевой буфер */
        for (uint16_t i = UART_DMA_BUFFER_SIZE / 2; i < UART_DMA_BUFFER_SIZE; i++)
        {
            ring_buffer[ring_head] = dma_buffer[i];
            ring_head = (ring_head + 1) % UART_RING_BUFFER_SIZE;
        }
    }
}

/* Обработчик ошибок UART */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* При ошибке перезапускаем DMA */
        HAL_UART_DMAStop(&huart2);
        HAL_UART_Receive_DMA(&huart2, dma_buffer, UART_DMA_BUFFER_SIZE);
    }
}

/* Чтение данных из кольцевого буфера */
uint16_t UART_Read(uint8_t *data, uint16_t max_len)
{
    uint16_t count = 0;

    while (ring_head != ring_tail && count < max_len)
    {
        data[count] = ring_buffer[ring_tail];
        ring_tail = (ring_tail + 1) % UART_RING_BUFFER_SIZE;
        count++;
    }

    return count;
}

/* Отправка строки (блокирующая) */
void UART_Print(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

/* Отправка данных (блокирующая) */
void UART_Send(uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, data, len, 100);
}

/* Статическая переменная для callback-функции */
static void (*user_callback)(uint8_t data) = NULL;

/* Установка пользовательского callback */
void UART_SetCallback(void (*callback)(uint8_t data))
{
    user_callback = callback;
}

/* Функция для обработки данных в main loop */
void UART_Process(void)
{
    uint8_t data;

    /* Обрабатываем все доступные байты */
    while (ring_head != ring_tail)
    {
        data = ring_buffer[ring_tail];
        ring_tail = (ring_tail + 1) % UART_RING_BUFFER_SIZE;

        /* Если установлен callback, вызываем его */
        if (user_callback != NULL)
        {
            user_callback(data);
        }
        /* Иначе данные просто читаются и теряются */
    }
}

/* Обработчик прерывания UART (нужно добавить в USART2_IRQHandler) */
void UART_IdleCallback(void)
{
    /* Останавливаем DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* Получаем количество принятых байт */
    uint16_t received = UART_DMA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    if (received > 0)
    {
        /* Копируем полученные данные в кольцевой буфер */
        for (uint16_t i = 0; i < received; i++)
        {
            ring_buffer[ring_head] = dma_buffer[i];
            ring_head = (ring_head + 1) % UART_RING_BUFFER_SIZE;
        }
    }
    
    /* Запускаем DMA заново */
    HAL_UART_Receive_DMA(&huart2, dma_buffer, UART_DMA_BUFFER_SIZE);
}

void UART_ProcessLines(void)
{
    static uint8_t line_buffer[256];
    static uint16_t line_index = 0;
    uint8_t data;
    
    /* Обрабатываем все доступные байты */
    while (ring_head != ring_tail)
    {
        data = ring_buffer[ring_tail];
        ring_tail = (ring_tail + 1) % UART_RING_BUFFER_SIZE;
        
        /* Проверяем конец строки */
        if (data == '\n' || data == '\r')
        {
            if (line_index > 0)
            {
                line_buffer[line_index] = '\0';
                /* Обрабатываем строку */
                UART_Print("Line: ");
                UART_Print((char*)line_buffer);
                UART_Print("\r\n");
                line_index = 0;
            }
        }
        else if (line_index < sizeof(line_buffer) - 1)
        {
            line_buffer[line_index++] = data;
        }
    }
}

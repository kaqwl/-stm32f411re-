#include "adc_dma.h"
#include "stm32f4xx_hal.h"

// ============================================
// Статические переменные
// ============================================

volatile uint16_t adc_buffer[ADC_BUFFER_SIZE] __attribute__((aligned(4)));
volatile uint8_t adc_data_ready = 0;
volatile uint8_t adc_is_running = 0;

static ADC_HandleTypeDef s_AdcHandle;
static DMA_HandleTypeDef s_DmaHandle;
static TIM_HandleTypeDef s_TimHandle;

// ============================================
// Обработчик ошибок
// ============================================

__attribute__((weak)) void Error_Handler(void)
{
    while (1)
    {
        __WFI();
    }
}

// ============================================
// Статические функции
// ============================================

static void ADC_ClockEnable(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
}

static void ADC_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_0;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init);
}

static void ADC_DMA_Configure(void)
{
    // HAL_DMA_DeInit(&s_DmaHandle);

    s_DmaHandle.Instance = DMA2_Stream0;
    s_DmaHandle.Init.Channel = DMA_CHANNEL_0;
    s_DmaHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    s_DmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
    s_DmaHandle.Init.MemInc = DMA_MINC_ENABLE;
    s_DmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_DmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    s_DmaHandle.Init.Mode = DMA_CIRCULAR;
    s_DmaHandle.Init.Priority = DMA_PRIORITY_HIGH;

    if (HAL_DMA_Init(&s_DmaHandle) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_LINKDMA(&s_AdcHandle, DMA_Handle, s_DmaHandle);

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

static void ADC_Timer_Init(void)
{
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq() * 2; // Частота APB1 (50 МГц)
    uint32_t period = (timer_clock / TIMER_FREQUENCY) - 1;

    s_TimHandle.Instance = TIM2;
    s_TimHandle.Init.Prescaler = 0;
    s_TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_TimHandle.Init.Period = period;
    s_TimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&s_TimHandle) != HAL_OK)
    {
        Error_Handler();
    }

    // Настройка: при событии обновления таймера генерировать TRGO
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE; // TRGO = событие обновления
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&s_TimHandle, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

static void ADC_Peripheral_Init(void)
{
    ADC_ChannelConfTypeDef adc_channel = {0};

    HAL_ADC_DeInit(&s_AdcHandle);

    s_AdcHandle.Instance = ADC1;
    s_AdcHandle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    s_AdcHandle.Init.Resolution = ADC_RESOLUTION_12B;
    s_AdcHandle.Init.ScanConvMode = DISABLE;
    s_AdcHandle.Init.ContinuousConvMode = DISABLE; // Работаем по таймеру
    s_AdcHandle.Init.DiscontinuousConvMode = DISABLE;

    s_AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING; // По фронту
    s_AdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    // s_AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    // s_AdcHandle.Init.ExternalTrigConv = ADC_SOFTWARE_START;

    s_AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    s_AdcHandle.Init.NbrOfConversion = 1;
    s_AdcHandle.Init.DMAContinuousRequests = ENABLE;
    s_AdcHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&s_AdcHandle) != HAL_OK)
    {
        Error_Handler();
    }

    adc_channel.Channel = ADC_CHANNEL;
    adc_channel.Rank = 1;
    adc_channel.SamplingTime = ADC_SAMPLING_TIME;
    adc_channel.Offset = 0;

    if (HAL_ADC_ConfigChannel(&s_AdcHandle, &adc_channel) != HAL_OK)
    {
        Error_Handler();
    }
}

// Калибровка не требуется в этой версии библиотеки
// Функция ADC_Calibrate удалена

// ============================================
// Публичные функции
// ============================================

void ADC_DMA_Init(void)
{
    ADC_ClockEnable();
    ADC_GPIO_Init();
    ADC_DMA_Configure();
    ADC_Timer_Init();
    ADC_Peripheral_Init();
    // Калибровка не выполняется
}

void ADC_DMA_Start(void)
{
    if (!adc_is_running)
    {
        if (HAL_ADC_Start_DMA(&s_AdcHandle, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE) == HAL_OK)
        {
            adc_is_running = 1;
            adc_data_ready = 0;

            // Запускаем таймер
            HAL_TIM_Base_Start(&s_TimHandle);
        }
    }
}

void ADC_DMA_Stop(void)
{
    if (adc_is_running)
    {
        HAL_TIM_Base_Stop(&s_TimHandle);
        HAL_ADC_Stop_DMA(&s_AdcHandle);
        adc_is_running = 0;
        adc_data_ready = 0;
    }
}

uint16_t ADC_GetAverageValue(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        sum += adc_buffer[i];
    }
    return (uint16_t)(sum / ADC_BUFFER_SIZE);
}

uint32_t ADC_GetVoltage_mV(void)
{
    uint16_t avg_value = ADC_GetAverageValue();
    return ((uint32_t)avg_value * 3300) / 4095;
}

uint16_t ADC_GetRawValue(void)
{
    return adc_buffer[ADC_BUFFER_SIZE - 1];
}

// ============================================
// HAL Callback функции
// ============================================

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    adc_data_ready = 1;
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    adc_is_running = 0;
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_DmaHandle);
}
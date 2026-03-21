#include "stm32f411xe.h"
#include "stm32f4xx_hal.h"
#include "timer4_time.h"
#include "stm32f4xx_hal_tim.h"

static volatile uint32_t timer4_ticks = 0;
static TIM_HandleTypeDef htim4 = {0};

/**
 * @brief Обработчик прерывания TIM4
 */
void TIM4_IRQHandler(void)
{
    /* Проверяем флаг обновления (переполнения) */
    if (__HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
        timer4_ticks++;  // увеличиваем счетчик
    }
    
    /* Вызываем HAL-обработчик */
    HAL_TIM_IRQHandler(&htim4);
}

/**
 * @brief Инициализация TIM4 как системного таймера
 * @param prescaler Предделитель (значение в регистр PSC)
 * @param period Период автоперезагрузки (значение в регистр ARR)
 * @note Частота прерываний = TIM4_CLK / (prescaler+1) / (period+1)
 */
void TIM4_Time_Init(uint32_t prescaler, uint32_t period)
{
    /* 1. Включаем тактирование TIM4 */
    __HAL_RCC_TIM4_CLK_ENABLE();
    
    /* 2. Настраиваем таймер */
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = prescaler;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = period;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    HAL_TIM_Base_Init(&htim4);
    
    /* 3. Настраиваем прерывание */
    HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);  // приоритет 2 (ниже SysTick)
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    
    /* 4. Запускаем таймер с прерываниями */
    HAL_TIM_Base_Start_IT(&htim4);
}

/**
 * @brief Получить текущее время в тиках TIM4
 * @return Количество тиков с момента запуска
 */
uint32_t TIM4_Time_GetTicks(void)
{
    uint32_t ticks;
    
    /* Защита от несогласованного чтения (если прерывание изменит значение) */
    __disable_irq();
    ticks = timer4_ticks;
    __enable_irq();
    
    return ticks;
}

/**
 * @brief Получить текущее время в миллисекундах
 * @return Количество миллисекунд (если таймер настроен на 1 мс)
 */
uint32_t TIM4_Time_GetMs(void)
{
    /* Предполагаем, что таймер настроен на 1 мс на тик */
    return TIM4_Time_GetTicks();
}

/**
 * @brief Проверить, прошел ли указанный интервал
 * @param start_time Время начала в тиках
 * @param interval_ticks Длительность интервала в тиках
 * @return 1 если интервал прошел, иначе 0
 */
uint8_t TIM4_Time_IsElapsed(uint32_t start_time, uint32_t interval_ticks)
{
    uint32_t current_time = TIM4_Time_GetTicks();
    /* Беззнаковая арифметика корректно обрабатывает переполнение */
    return (current_time - start_time) >= interval_ticks;
}

/**
 * @brief Получить текущее значение счетчика TIM4 (аппаратное, не наше)
 * @return Текущее значение аппаратного счетчика TIM4 (от 0 до period)
 */
uint32_t TIM4_Time_GetCounter(void)
{
    return __HAL_TIM_GET_COUNTER(&htim4);
}

/**
 * @brief Сбросить счетчик тиков в 0
 */
void TIM4_Time_Reset(void)
{
    __disable_irq();
    timer4_ticks = 0;
    __enable_irq();
}

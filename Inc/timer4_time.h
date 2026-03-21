#ifndef TIMER4_TIME_H
#define TIMER4_TIME_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Инициализация TIM4 как системного таймера
 * @param prescaler Предделитель (TIM4_CLK / (prescaler+1) = частота счета)
 * @param period Период автоперезагрузки (количество тиков до прерывания)
 * @note Частота прерываний = TIM4_CLK / ((prescaler+1) * (period+1))
 */
void TIM4_Time_Init(uint32_t prescaler, uint32_t period);

/**
 * @brief Получить текущее время в тиках TIM4
 * @return Количество тиков с момента запуска (переполняется через ~)
 */
uint32_t TIM4_Time_GetTicks(void);

/**
 * @brief Получить текущее время в миллисекундах (если настроено на 1 мс)
 * @return Количество миллисекунд
 */
uint32_t TIM4_Time_GetMs(void);

/**
 * @brief Проверить, прошел ли указанный интервал
 * @param start_time Время начала в тиках
 * @param interval_ticks Длительность интервала в тиках
 * @return 1 если интервал прошел, иначе 0
 */
uint8_t TIM4_Time_IsElapsed(uint32_t start_time, uint32_t interval_ticks);

#endif // TIMER4_TIME_H
#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <stdint.h>

/**
 * @brief Получить текущее время в миллисекундах с момента запуска
 * @return Количество миллисекунд (счетчик переполнится через ~49 дней)
 */
uint32_t SystemTime_GetMs(void);

/**
 * @brief Проверить, прошел ли указанный интервал
 * @param start_time Время начала в миллисекундах
 * @param interval_ms Длительность интервала в миллисекундах
 * @return 1 если интервал прошел, иначе 0
 */
uint8_t SystemTime_IsElapsed(uint32_t start_time, uint32_t interval_ms);

#endif // SYSTEM_TIME_H
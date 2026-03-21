#include "system_time.h"
#include "stm32f4xx_hal.h" // для HAL_RCC_GetSysClockFreq и SysTick_Config

uint32_t SystemTime_GetMs(void)
{
    return HAL_GetTick();
}

uint8_t SystemTime_IsElapsed(uint32_t start_time, uint32_t interval_ms)
{
    uint32_t current_time = SystemTime_GetMs();
    /* Корректно обрабатывает переполнение счетчика */
    return (current_time - start_time) >= interval_ms;
}

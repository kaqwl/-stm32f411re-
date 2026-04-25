#include "main.h"
#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);
extern UART_HandleTypeDef huart1;

void NMI_Handler(void) { }
void HardFault_Handler(void) { while(1); }
void MemManage_Handler(void) { while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void) { while(1); }
void DebugMon_Handler(void) { }

// SVC_Handler и PendSV_Handler УДАЛЕНЫ — их обрабатывает FreeRTOS в port.c

void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
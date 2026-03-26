#ifndef ADC_DMA_H
#define ADC_DMA_H

#include <stdint.h>

// ============================================
// Настройки
// ============================================

#define ADC_BUFFER_SIZE     16
#define ADC_SAMPLING_TIME    ADC_SAMPLETIME_56CYCLES
#define ADC_CHANNEL          ADC_CHANNEL_0

#define TIMER_FREQUENCY     10

// ============================================
// Глобальные переменные
// ============================================

extern volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];
extern volatile uint8_t adc_data_ready;
extern volatile uint8_t adc_is_running;

// ============================================
// Публичные функции
// ============================================

void ADC_DMA_Init(void);
void ADC_DMA_Start(void);
void ADC_DMA_Stop(void);
uint16_t ADC_GetAverageValue(void);
uint32_t ADC_GetVoltage_mV(void);
uint16_t ADC_GetRawValue(void);

#endif /* ADC_DMA_H */
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
    // ===== ИНИЦИАЛИЗАЦИЯ DMA =====
    // DMA (Direct Memory Access) будет автоматически переносить данные
    // из регистра ADC1->DR в наш буфер adc_buffer без участия процессора
    
    // HAL_DMA_DeInit(&s_DmaHandle);  // ЗАКОММЕНТИРОВАНО
    // Эта строка вызывала Hard Fault, потому что структура s_DmaHandle
    // не была инициализирована. Теперь мы просто заполняем структуру
    // и вызываем HAL_DMA_Init, что безопаснее.
    
    // ===== ВЫБОР ПОТОКА DMA =====
    // Указываем, какой поток DMA используем.
    // DMA2_Stream0 — первый поток второго DMA контроллера.
    // На STM32F411 ADC1 подключен к DMA2 Stream0, Channel 0.
    // Выбор правильного потока критичен!
    s_DmaHandle.Instance = DMA2_Stream0;
    
    // ===== КАНАЛ DMA =====
    // Канал DMA внутри выбранного потока.
    // DMA_CHANNEL_0 — канал 0, который соответствует ADC1.
    // Таблица соответствия (из Reference Manual):
    //   DMA2 Stream0 → Channel 0 → ADC1
    //   DMA2 Stream1 → Channel 0 → ADC2
    //   и т.д.
    s_DmaHandle.Init.Channel = DMA_CHANNEL_0;
    
    // ===== НАПРАВЛЕНИЕ ПЕРЕДАЧИ =====
    // DMA_PERIPH_TO_MEMORY — данные идут из периферии (АЦП) в память (буфер).
    // Альтернативы:
    //   DMA_MEMORY_TO_PERIPH — из памяти в периферию (например, для передачи данных)
    //   DMA_MEMORY_TO_MEMORY — из памяти в память (копирование массивов)
    s_DmaHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    
    // ===== ИНКРЕМЕНТ АДРЕСА ПЕРИФЕРИИ =====
    // DMA_PINC_DISABLE — адрес периферии НЕ увеличивается после каждой передачи.
    // Почему? Потому что данные всегда берутся из одного и того же регистра ADC1->DR.
    // Если бы мы читали из нескольких регистров (например, FIFO), включили бы инкремент.
    s_DmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
    
    // ===== ИНКРЕМЕНТ АДРЕСА ПАМЯТИ =====
    // DMA_MINC_ENABLE — адрес памяти УВЕЛИЧИВАЕТСЯ после каждой передачи.
    // Нужно, чтобы заполнять массив последовательно:
    //   adc_buffer[0] ← первое значение
    //   adc_buffer[1] ← второе значение
    //   adc_buffer[2] ← третье значение
    //   и так далее...
    s_DmaHandle.Init.MemInc = DMA_MINC_ENABLE;
    
    // ===== РАЗРЯДНОСТЬ ДАННЫХ (ПЕРИФЕРИЯ) =====
    // DMA_PDATAALIGN_HALFWORD — данные периферии имеют размер 16 бит (полуслово).
    // Регистр ADC1->DR 16-битный (12 бит данных + 4 бита резерв).
    // Варианты:
    //   DMA_PDATAALIGN_BYTE     — 8 бит
    //   DMA_PDATAALIGN_HALFWORD — 16 бит ← наш случай
    //   DMA_PDATAALIGN_WORD     — 32 бит
    s_DmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    
    // ===== РАЗРЯДНОСТЬ ДАННЫХ (ПАМЯТЬ) =====
    // DMA_MDATAALIGN_HALFWORD — данные в памяти имеют размер 16 бит.
    // Наш буфер adc_buffer объявлен как uint16_t (16 бит).
    // Должно совпадать с разрядностью периферии!
    s_DmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    
    // ===== РЕЖИМ РАБОТЫ DMA =====
    // DMA_CIRCULAR — циклический режим.
    // Когда DMA дойдет до конца буфера, он автоматически начнет запись с начала.
    // Это обеспечивает непрерывный сбор данных без остановки.
    // Альтернативы:
    //   DMA_NORMAL — один проход (остановится после заполнения буфера)
    //   DMA_CIRCULAR — бесконечный цикл ← наш выбор
    s_DmaHandle.Init.Mode = DMA_CIRCULAR;
    
    // ===== ПРИОРИТЕТ DMA =====
    // DMA_PRIORITY_HIGH — высокий приоритет.
    // Определяет, какой поток DMA получит доступ к шине первым,
    // если несколько потоков активны одновременно.
    // Варианты:
    //   DMA_PRIORITY_LOW      — низкий
    //   DMA_PRIORITY_MEDIUM   — средний
    //   DMA_PRIORITY_HIGH     — высокий ← наш выбор
    //   DMA_PRIORITY_VERY_HIGH — очень высокий
    // Ставим высокий приоритет, чтобы не пропустить данные АЦП.
    s_DmaHandle.Init.Priority = DMA_PRIORITY_HIGH;
    
    // ===== ИНИЦИАЛИЗАЦИЯ DMA =====
    // HAL_DMA_Init() применяет все настройки к DMA контроллеру:
    //   1. Настраивает регистр DMA_SxCR (Control Register)
    //   2. Настраивает регистр DMA_SxNDTR (Number of Data Transfer)
    //   3. Настраивает регистр DMA_SxPAR (Peripheral Address)
    //   4. Настраивает регистр DMA_SxM0AR (Memory Address)
    //   5. Включает DMA (устанавливает бит EN в DMA_SxCR)
    // Возвращает HAL_OK при успехе, иначе ошибку.
    if (HAL_DMA_Init(&s_DmaHandle) != HAL_OK)
    {
        // Если инициализация не удалась, вызываем обработчик ошибок.
        Error_Handler();
    }
    
    // ===== СВЯЗЫВАНИЕ DMA С АЦП =====
    // __HAL_LINKDMA — это макрос, который связывает DMA хендл с АЦП хендлом.
    // Он сохраняет указатель на s_DmaHandle в поле DMA_Handle структуры s_AdcHandle.
    // Теперь HAL будет знать, какой DMA канал использовать для АЦП.
    // Это нужно для корректной работы HAL_ADC_Start_DMA() и колбэков.
    __HAL_LINKDMA(&s_AdcHandle, DMA_Handle, s_DmaHandle);
    
    // ===== НАСТРОЙКА ПРЕРЫВАНИЙ DMA =====
    // Устанавливаем приоритет прерывания для DMA2 Stream0.
    // HAL_NVIC_SetPriority(IRQn, PreemptPriority, SubPriority)
    //   IRQn = DMA2_Stream0_IRQn — номер прерывания
    //   PreemptPriority = 2 — вытесняющий приоритет (чем меньше число, тем выше приоритет)
    //   SubPriority = 0 — подприоритет (второстепенный)
    // Ставим приоритет 2, чтобы UART (приоритет 0) мог прерывать DMA при необходимости.
    // Это важно для работы UART без зависаний!
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
    
    // Включаем прерывание DMA2 Stream0 в NVIC (Nested Vectored Interrupt Controller).
    // Теперь при возникновении прерывания от DMA, процессор вызовет обработчик.
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

static void ADC_Timer_Init(void)
{
    // ===== РАСЧЕТ ПЕРИОДА ТАЙМЕРА =====
    
    // Получаем частоту шины APB1 (PCLK1).
    // На STM32F411 при SYSCLK=100 МГц, APB1 = 50 МГц.
    // Умножаем на 2, потому что таймеры на APB1 работают на удвоенной частоте!
    // Реальная частота таймера TIM2 = 50 МГц × 2 = 100 МГц.
    // Это особенность архитектуры: таймеры всегда стремятся работать на максимальной частоте.
    uint32_t timer_clock = HAL_RCC_GetPCLK1Freq() * 2;
    
    // Расчет периода автоперезагрузки (AutoReload Register - ARR).
    // Формула: period = (timer_clock / target_freq) - 1.
    // Пример: при timer_clock = 100 000 000 Гц, target_freq = 1000 Гц:
    //   period = (100 000 000 / 1000) - 1 = 100 000 - 1 = 99999.
    // Таймер считает от 0 до period, затем генерирует событие обновления (update).
    uint32_t period = (timer_clock / TIMER_FREQUENCY) - 1;
    
    // ===== НАСТРОЙКА СТРУКТУРЫ ТАЙМЕРА =====
    
    // Указываем, какой экземпляр таймера используем.
    // TIM2 - 32-битный таймер, находится на шине APB1.
    // Доступные альтернативы: TIM1, TIM3, TIM4, TIM5, TIM6, TIM7, TIM9, TIM10, TIM11.
    s_TimHandle.Instance = TIM2;
    
    // Предделитель (Prescaler - PSC).
    // Делит тактовую частоту таймера: timer_clock / (PSC + 1).
    // 0 означает деление на 1 (частота таймера = timer_clock = 100 МГц).
    // Если period получается больше 65535 (16-битный лимит), нужно увеличить предделитель.
    s_TimHandle.Init.Prescaler = 0;
    
    // Режим счета.
    // TIM_COUNTERMODE_UP - счет от 0 до Period (вверх).
    // Альтернативы: TIM_COUNTERMODE_DOWN, TIM_COUNTERMODE_CENTERALIGNED.
    s_TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    
    // Период автоперезагрузки (AutoReload Register - ARR).
    // Счетчик считает от 0 до этого значения, затем сбрасывается в 0.
    // При достижении Period генерируется событие обновления (Update Event).
    s_TimHandle.Init.Period = period;
    
    // Делитель тактового сигнала для цифрового фильтра (ClockDivision - CKD).
    // TIM_CLOCKDIVISION_DIV1 - делитель 1 (без деления).
    // Влияет только на фильтр входов (TIMx_ETR, TIMx_IC), не влияет на частоту счета.
    // Для запуска АЦП это не критично, оставляем DIV1.
    s_TimHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    
    // Режим предзагрузки регистра автоперезагрузки (AutoReload Preload - ARPE).
    // TIM_AUTORELOAD_PRELOAD_ENABLE - изменения Period применяются при следующем событии обновления.
    // Это безопасно: новые настройки вступят в силу после завершения текущего цикла.
    // Альтернатива: DISABLE - изменения применяются немедленно (может вызвать глитчи).
    s_TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    // ===== ИНИЦИАЛИЗАЦИЯ ТАЙМЕРА =====
    
    // Применяем все настройки к таймеру.
    // HAL_TIM_Base_Init() выполняет:
    //   1. Включает тактирование таймера (если не включено)
    //   2. Настраивает регистры TIMx_CR1 (Counter Mode, ARPE, CKD)
    //   3. Настраивает TIMx_PSC (Prescaler)
    //   4. Настраивает TIMx_ARR (AutoReload)
    //   5. Сбрасывает флаги прерываний
    // Возвращает HAL_OK при успехе, иначе ошибку.
    if (HAL_TIM_Base_Init(&s_TimHandle) != HAL_OK)
    {
        // Если инициализация не удалась, вызываем обработчик ошибок.
        Error_Handler();
    }
    
    // ===== НАСТРОЙКА ГЕНЕРАЦИИ TRGO =====
    
    // Структура для настройки синхронизации таймера.
    // Нужна, чтобы настроить, какой сигнал таймер будет отправлять наружу.
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    // Выбор источника сигнала Master Output Trigger (TRGO).
    // TIM_TRGO_UPDATE - TRGO генерируется при событии обновления таймера (overflow).
    // Это сигнал, который мы отправим в АЦП для запуска преобразования.
    // Альтернативы:
    //   TIM_TRGO_ENABLE - по сигналу Enable
    //   TIM_TRGO_OC1REF - по совпадению канала 1
    //   TIM_TRGO_OC2REF - по совпадению канала 2
    //   и другие...
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    
    // Режим Master/Slave.
    // TIM_MASTERSLAVEMODE_DISABLE - отключаем режим подчинения.
    // Если бы таймер синхронизировался с другим таймером, включили бы ENABLE.
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    
    // Применяем настройки синхронизации к таймеру.
    // HAL_TIMEx_MasterConfigSynchronization() настраивает:
    //   1. TIMx_CR2: MMS (Master Mode Selection) - биты 4-6
    //   2. TIMx_SMCR: MSM (Master Slave Mode) - бит 7
    // Эта функция называется "Ex", потому что это расширенная функция HAL.
    if (HAL_TIMEx_MasterConfigSynchronization(&s_TimHandle, &sMasterConfig) != HAL_OK)
    {
        // Если настройка не удалась, вызываем обработчик ошибок.
        Error_Handler();
    }
}

static void ADC_Peripheral_Init(void)
{
    // Структура для настройки канала АЦП.
    // Содержит: номер канала, время выборки, смещение (offset) и т.д.
    ADC_ChannelConfTypeDef adc_channel = {0};

    // Сбрасывает настройки АЦП в состояние по умолчанию.
    // Очищает регистры, отключает прерывания, сбрасывает флаги.
    // Нужно для "чистого" старта перед новой инициализацией.
    HAL_ADC_DeInit(&s_AdcHandle);

    // Указываем, какой экземпляр АЦП используем.
    // На STM32F411 есть два АЦП: ADC1 и ADC2.
    // ADC1 - основной, доступен на многих пинах.
    s_AdcHandle.Instance = ADC1;

    // Настройка частоты тактирования АЦП.
    // ADC_CLOCK_SYNC_PCLK_DIV4 - делитель от APB2 (PCLK2).
    // При PCLK2 = 100 МГц, ADCCLK = 100 / 4 = 25 МГц.
    // Это оптимальная частота для стабильной работы АЦП.
    s_AdcHandle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;

    // Разрешение АЦП.
    // ADC_RESOLUTION_12B = 12 бит (значения от 0 до 4095).
    // Можно уменьшить для увеличения скорости (10, 8, 6 бит).
    s_AdcHandle.Init.Resolution = ADC_RESOLUTION_12B;

    // Режим сканирования.
    // DISABLE - опрашиваем только один канал.
    // ENABLE - последовательно опрашиваем несколько каналов (из группы).
    s_AdcHandle.Init.ScanConvMode = DISABLE;

    // Непрерывный режим.
    // DISABLE - АЦП делает одно преобразование и останавливается.
    // ENABLE - после преобразования сразу запускается следующее (бесконечно).
    // У нас DISABLE, потому что запуск будет по таймеру.
    s_AdcHandle.Init.ContinuousConvMode = DISABLE;

    // Прерывистый режим (discontinuous).
    // DISABLE - отключен. При сканировании группы каналов,
    // если включен, прерывает сканирование после каждого канала.
    s_AdcHandle.Init.DiscontinuousConvMode = DISABLE;

    // Край внешнего триггера, по которому запускается АЦП.
    // ADC_EXTERNALTRIGCONVEDGE_RISING - по фронту (0→1).
    // Альтернативы: NONE (программный пуск), FALLING, BOTH_EDGES.
    s_AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;

    // Источник внешнего триггера.
    // ADC_EXTERNALTRIGCONV_T2_TRGO - запуск от сигнала TRGO таймера TIM2.
    // TRGO генерируется при событии обновления таймера (overflow).
    s_AdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;

    // Выравнивание данных в регистре результата.
    // ADC_DATAALIGN_RIGHT - младшие биты справа (обычный режим).
    // ADC_DATAALIGN_LEFT - старшие биты слева (для 8-битных режимов).
    s_AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;

    // Количество преобразований в регулярной группе.
    // При ScanConvMode = DISABLE, должно быть 1.
    // При сканировании нескольких каналов, ставим количество каналов.
    s_AdcHandle.Init.NbrOfConversion = 1;

    // Непрерывные запросы DMA.
    // ENABLE - DMA работает постоянно, запрашивая новые данные.
    // DISABLE - DMA срабатывает только один раз.
    // В циклическом режиме DMA (CIRCULAR) нужно включить этот флаг.
    s_AdcHandle.Init.DMAContinuousRequests = ENABLE;

    // Выбор момента генерации флага EOC (End Of Conversion).
    // ADC_EOC_SINGLE_CONV - флаг устанавливается после каждого преобразования.
    // ADC_EOC_SEQ_CONV - флаг после завершения всей последовательности.
    s_AdcHandle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    // Применяем все настройки к АЦП.
    // HAL_ADC_Init() записывает параметры в регистры ADC_CR1, ADC_CR2 и др.
    // Возвращает HAL_OK при успехе, иначе ошибку.
    if (HAL_ADC_Init(&s_AdcHandle) != HAL_OK)
    {
        // Если инициализация не удалась, вызываем обработчик ошибок.
        // Обычно уходит в бесконечный цикл с индикацией.
        Error_Handler();
    }

    // ===== Настройка канала АЦП =====
    
    // Номер канала. ADC_CHANNEL - макрос, например ADC_CHANNEL_0 (PA0).
    adc_channel.Channel = ADC_CHANNEL;
    
    // Позиция канала в последовательности сканирования.
    // При NbrOfConversion = 1, Rank всегда 1.
    adc_channel.Rank = 1;
    
    // Время выборки (sampling time) — время заряда внутреннего конденсатора.
    // ADC_SAMPLING_TIME - макрос, например ADC_SAMPLETIME_56CYCLES.
    // Чем выше сопротивление источника, тем больше нужно циклов.
    // При резисторе 10 кОм, 56 циклов — хороший выбор.
    adc_channel.SamplingTime = ADC_SAMPLING_TIME;
    
    // Смещение (offset) — вычитается из результата преобразования.
    // Используется для калибровки или работы с отрицательными напряжениями.
    // 0 — отключает смещение.
    adc_channel.Offset = 0;

    // Применяем настройки канала к АЦП.
    // HAL_ADC_ConfigChannel() записывает параметры в регистры ADC_SQR и ADC_SMPR.
    if (HAL_ADC_ConfigChannel(&s_AdcHandle, &adc_channel) != HAL_OK)
    {
        // Если настройка канала не удалась, вызываем обработчик ошибок.
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
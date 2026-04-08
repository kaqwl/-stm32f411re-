TARGET = nucleo-blink
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Пути
SRC_DIR = Src
BUILD_DIR = build
CMSIS_DIR = libs/STM32CubeF4/Drivers
FREE_RTOS_SRC = libs/STM32CubeF4/Middlewares/Third_Party/FreeRTOS/Source

# Пути к HAL
HAL_DIR = $(CMSIS_DIR)/STM32F4xx_HAL_Driver
HAL_INC = $(HAL_DIR)/Inc
HAL_SRC = $(HAL_DIR)/Src

# Пути к заголовочным файлам
INCLUDES = -I./Inc \
           -I$(CMSIS_DIR)/CMSIS/Include \
           -I$(CMSIS_DIR)/BSP/STM32F4xx-Nucleo \
           -I$(CMSIS_DIR)/CMSIS/Device/ST/STM32F4xx/Include \
           -I$(HAL_DIR)/Inc \
           -I$(FREE_RTOS_SRC)/include \
           -I$(FREE_RTOS_SRC)/CMSIS_RTOS_V2 \
           -I$(FREE_RTOS_SRC)/portable/GCC/ARM_CM4F


# Файлы
# SOURCES = $(SRC_DIR)/main.c \
#           $(SRC_DIR)/system_stm32f4xx.c \
#           $(SRC_DIR)/stm32f4xx_it.c \
#           $(SRC_DIR)/stub.c
SOURCES = $(wildcard $(SRC_DIR)/*.c) \
          $(wildcard $(SRC_DIR)/*/*.c) \
          $(wildcard $(SRC_DIR)/*/*/*.c)

# Файлы HAL (подключаем только нужные модули!)
HAL_SOURCES = $(HAL_SRC)/stm32f4xx_hal.c \
              $(HAL_SRC)/stm32f4xx_hal_cortex.c \
              $(HAL_SRC)/stm32f4xx_hal_rcc.c \
              $(HAL_SRC)/stm32f4xx_hal_rcc_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_gpio.c \
              $(HAL_SRC)/stm32f4xx_hal_pwr.c \
              $(HAL_SRC)/stm32f4xx_hal_pwr_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_uart.c \
              $(HAL_SRC)/stm32f4xx_hal_usart.c \
              $(HAL_SRC)/stm32f4xx_hal_dma.c \
              $(HAL_SRC)/stm32f4xx_hal_dma_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_tim.c \
              $(HAL_SRC)/stm32f4xx_hal_tim_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_adc.c \
              $(HAL_SRC)/stm32f4xx_hal_adc_ex.c

FREERTOS_SOURCES = $(FREE_RTOS_SRC)/tasks.c \
                   $(FREE_RTOS_SRC)/queue.c \
                   $(FREE_RTOS_SRC)/list.c \
                   $(FREE_RTOS_SRC)/timers.c \
                   $(FREE_RTOS_SRC)/event_groups.c \
                   $(FREE_RTOS_SRC)/portable/GCC/ARM_CM4F/port.c \
                   $(FREE_RTOS_SRC)/portable/MemMang/heap_4.c
#                    $(FREE_RTOS_SRC)/CMSIS_RTOS_V2/cmsis_os2.c

STARTUP_FILE = $(CMSIS_DIR)/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s

# # USB пути
# USB_CORE_DIR = libs/STM32CubeF4/Middlewares/ST/STM32_USB_Device_Library/Core
# USB_CLASS_DIR = libs/STM32CubeF4/Middlewares/ST/STM32_USB_Device_Library/Class/CDC

# # USB исходники
# USB_SOURCES = $(USB_CORE_DIR)/Src/usbd_core.c \
#               $(USB_CORE_DIR)/Src/usbd_ctlreq.c \
#               $(USB_CORE_DIR)/Src/usbd_ioreq.c \
#               $(USB_CLASS_DIR)/Src/usbd_cdc.c \
#               $(USB_CLASS_DIR)/Src/usbd_cdc_if.c

# Объединяем всё вместе
ALL_SOURCES = $(SOURCES) $(HAL_SOURCES) $(FREERTOS_SOURCES) $(STARTUP_FILE)

# Объектные файлы
# OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(ALL_SOURCES:.c=.o)))
OBJECTS = $(addprefix $(BUILD_DIR)/, $(ALL_SOURCES:.c=.o))
OBJECTS := $(OBJECTS:.s=.o)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Флаги компилятора
CFLAGS = -mcpu=cortex-m4 \
         -mthumb \
         -mfloat-abi=hard \
         -mfpu=fpv4-sp-d16 \
         -DSTM32F411xE \
         -DUSE_HAL_DRIVER \
         $(INCLUDES) \
         -O0 \
         -g3 \
         -Wall \
         -ffunction-sections \
         -fdata-sections

LDFLAGS = -TSTM32F411RETX_FLASH.ld \
          --specs=nosys.specs \
          --specs=nano.specs \
          -nostartfiles \
          -Wl,--gc-sections


# Сборка
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) | $(BUILD_DIR)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	$(OBJCOPY) -O ihex $@ $(BUILD_DIR)/$(TARGET).hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(HAL_SRC)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 5. Конкретное правило для startup (чтобы найти его в глубине папок)
# $(BUILD_DIR)/startup_stm32f411xe.o: $(STARTUP_FILE) | $(BUILD_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

.PHONY: clean flash

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32f4x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).bin 0x08000000 verify reset exit"

flash-elf: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg \
	        -f target/stm32f4x.cfg \
	        -c "program $< verify reset exit"

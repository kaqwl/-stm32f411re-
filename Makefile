TARGET = nucleo-blink
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

SRC_DIR = Src
BUILD_DIR = build
CMSIS_DIR = libs/STM32CubeF4/Drivers

HAL_DIR = $(CMSIS_DIR)/STM32F4xx_HAL_Driver
HAL_SRC = $(HAL_DIR)/Src

INCLUDES = -I./Inc \
           -I$(CMSIS_DIR)/CMSIS/Include \
           -I$(CMSIS_DIR)/CMSIS/Device/ST/STM32F4xx/Include \
           -I$(HAL_DIR)/Inc

SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/stm32f4xx_it.c \
          $(SRC_DIR)/system_stm32f4xx.c

HAL_SOURCES = $(HAL_SRC)/stm32f4xx_hal.c \
              $(HAL_SRC)/stm32f4xx_hal_cortex.c \
              $(HAL_SRC)/stm32f4xx_hal_rcc.c \
              $(HAL_SRC)/stm32f4xx_hal_rcc_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_gpio.c \
              $(HAL_SRC)/stm32f4xx_hal_pwr.c \
              $(HAL_SRC)/stm32f4xx_hal_pwr_ex.c \
              $(HAL_SRC)/stm32f4xx_hal_uart.c \
              $(HAL_SRC)/stm32f4xx_hal_dma.c \
              $(HAL_SRC)/stm32f4xx_hal_exti.c

STARTUP_FILE = $(CMSIS_DIR)/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f411xe.s

ALL_SOURCES = $(SOURCES) $(HAL_SOURCES)

OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(ALL_SOURCES:.c=.o)))
OBJECTS += $(BUILD_DIR)/startup_stm32f411xe.o

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

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) | $(BUILD_DIR)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	$(OBJCOPY) -O ihex $@ $(BUILD_DIR)/$(TARGET).hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $@

$(BUILD_DIR)/startup_stm32f411xe.o: $(STARTUP_FILE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(HAL_SRC)/%.c | $(BUILD_DIR)
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
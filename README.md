Установить:
openocd
    openocd --version
arm-none-eabi-gcc
    arm-none-eabi-gcc --version
gdb-arm-none-eabi
    gdb-multiarch --version


# 1. Добавьте submodule
git submodule add https://github.com/STMicroelectronics/STM32CubeF4.git libs/STM32CubeF4

# 2. Подтяните LFS-файлы
cd libs/STM32CubeF4
git lfs install
git lfs fetch --all
git lfs checkout
cd ../..

# 3. Зафиксируйте изменения
git add .gitmodules libs/STM32CubeF4
git commit -m "Add STM32CubeF4 submodule (fresh)"

# 1. Проверьте файлы
ls libs/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f4xx.h

# 2. Проверьте статус submodule
git submodule status
# Ожидаемый вывод: хеш коммита без префиксов - или +

# 3. Проверьте .gitmodules
cat .gitmodules

# Должна быть запись про libs/STM32CubeF4 !!!!
git submodule update --init --recursive libs/STM32CubeF4


# Установи в VS Code расширение Cortex-Debug — оно добавляет:



# arm-none-eabi

# arm-none-eabi-nm
# Все символы, отсортированные по адресу
arm-none-eabi-nm -n build/nucleo-blink.elf
# Только определённые символы (исключить undefined)
arm-none-eabi-nm -n --defined-only build/nucleo-blink.elf
# С размером символа (для переменных)
arm-none-eabi-nm -S -n build/nucleo-blink.elf

# arm-none-eabi-objdump

# Полная дизассемблеризация с исходным кодом
arm-none-eabi-objdump -d -S build/nucleo-blink.elf
arm-none-eabi-objdump -h build/nucleo-blink.elf | grep isr_vector


# arm-none-eabi-readelf
# Детали ELF-файла

# Более подробная информация о структуре ELF, чем у objdump.
arm-none-eabi-readelf -S build/nucleo-blink.elf
arm-none-eabi-readelf -s build/nucleo-blink.elf
arm-none-eabi-readelf -h build/nucleo-blink.elf

# Посмотреть, какие секции загружаются в память:
arm-none-eabi-readelf -l build/nucleo-blink.elf



# arm-none-eabi-strings
# Поиск строк

# Все ASCII-строки длиннее 4 символов
arm-none-eabi-strings -n 4 build/nucleo-blink.bin

# Только из секции .rodata
arm-none-eabi-strings -n 4 -a build/nucleo-blink.elf

arm-none-eabi-strings build/nucleo-blink.bin | grep "Error"
# → Найти все строки с текстом ошибки



Сводная таблица инструментов
┌────────────┬───────────────────────────┬──────────────────────────────────────────────────────┐
│ Инструмент │ Основное применение       │ Ключевые флаги                                       │
├────────────┼───────────────────────────┼──────────────────────────────────────────────────────┤
│ nm         │ Символы и адреса          │ -n (сортировка), -S (размер), --defined-only         │
│ objdump    │ Дизассемблер, анализ кода │ -d (disasm), -S (с исходниками), -h (headers)        │
│ readelf    │ Детали ELF-структуры      │ -h (header), -l (segments), -s (symbols)             │
│ objcopy    │ Конвертация форматов      │ -O binary/ihex/srec, --strip-debug                   │
│ strings    │ Поиск текстовых констант  │ -n (мин. длина), -a (все секции)                     │
│ addr2line  │ Адрес → исходный код      │ -f (функция), -p (pretty), -e (elf)                  │
│ size       │ Размер секций             │ -A (подробно), -B (berkelian)                        │
│ ar         │ Работа с .a библиотеками  │ -t (list), -x (extract), -tv (verbose)               │
└────────────┴───────────────────────────┴──────────────────────────────────────────────────────┘

# Подключится к порту 115200 или 57600
picocom -b 115200 /dev/ttyACM0

# Разпиновка
https://os.mbed.com/platforms/ST-Nucleo-F411RE/
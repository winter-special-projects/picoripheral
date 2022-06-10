cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH "/home/pi/C/pico/pico-sdk")

include(../common_import.cmake)

project(picoscope C CXX ASM)

pico_sdk_init()

add_executable(picoscope picoscope.c)

pico_generate_pio_header(picoscope ${CMAKE_CURRENT_LIST_DIR}/timer.pio)
pico_generate_pio_header(picoscope ${CMAKE_CURRENT_LIST_DIR}/delay.pio)

pico_set_program_name(picoscope "picoscope")
pico_set_program_version(picoscope "0.1")

# stdout -> UART
pico_enable_stdio_uart(picoscope 1)
pico_enable_stdio_usb(picoscope 0)

# we using a lot of different hardware here
target_link_libraries(picoscope pico_stdlib)
target_link_libraries(picoscope hardware_i2c)
target_link_libraries(picoscope hardware_pio)
target_link_libraries(picoscope hardware_adc)
target_link_libraries(picoscope hardware_dma)
target_link_libraries(picoscope hardware_spi)

pico_add_extra_outputs(picoscope)

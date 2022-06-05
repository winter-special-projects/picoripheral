cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH "/home/pi/C/pico/pico-sdk")

include(../common_import.cmake)

project(picounter C CXX ASM)

pico_sdk_init()

add_executable(picounter picounter.c)

pico_generate_pio_header(picounter ${CMAKE_CURRENT_LIST_DIR}/counter.pio)
pico_generate_pio_header(picounter ${CMAKE_CURRENT_LIST_DIR}/clock.pio)

pico_set_program_name(picounter "picounter")
pico_set_program_version(picounter "0.1")

pico_enable_stdio_uart(picounter 1)
pico_enable_stdio_usb(picounter 0)

target_link_libraries(picounter pico_stdlib)
target_link_libraries(picounter hardware_pio)
target_link_libraries(picounter hardware_i2c)
target_link_libraries(picounter hardware_spi)
target_link_libraries(picounter hardware_dma)

pico_add_extra_outputs(picounter)


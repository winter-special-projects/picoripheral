cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH "/home/pi/C/pico/pico-sdk")

include(../common_import.cmake)

project(pio_clock C CXX ASM)

pico_sdk_init()

add_executable(pio_clock pio_clock.c)

pico_generate_pio_header(pio_clock ${CMAKE_CURRENT_LIST_DIR}/counter.pio)
pico_generate_pio_header(pio_clock ${CMAKE_CURRENT_LIST_DIR}/clock.pio)

pico_set_program_name(pio_clock "pio_clock")
pico_set_program_version(pio_clock "0.1")

pico_enable_stdio_uart(pio_clock 1)
pico_enable_stdio_usb(pio_clock 0)

target_link_libraries(pio_clock pico_stdlib hardware_pio)

pico_add_extra_outputs(pio_clock)


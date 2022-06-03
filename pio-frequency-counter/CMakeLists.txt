cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_PATH "/home/pi/C/pico/pico-sdk")

include(../common_import.cmake)

project(pio_counter C CXX ASM)

pico_sdk_init()

add_executable(pio_counter pio_counter.c)

pico_generate_pio_header(pio_counter ${CMAKE_CURRENT_LIST_DIR}/counter.pio)
pico_generate_pio_header(pio_counter ${CMAKE_CURRENT_LIST_DIR}/clock.pio)

pico_set_program_name(pio_counter "pio_counter")
pico_set_program_version(pio_counter "0.1")

pico_enable_stdio_uart(pio_counter 1)
pico_enable_stdio_usb(pio_counter 0)

target_link_libraries(pio_counter pico_stdlib)
target_link_libraries(pio_counter hardware_pio)
target_link_libraries(pio_counter hardware_i2c)

pico_add_extra_outputs(pio_counter)


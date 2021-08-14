# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pi/pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(yaesa_rp2040 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(yaesa_rp2040 yaesa_rp2040.c )

pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/f007t_manchwithdelay.pio)
pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/wh1080_pwmpulsebits.pio)
pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/ds18b20_1w.pio)

pico_set_program_name(yaesa_rp2040 "yaesa_rp2040")
pico_set_program_version(yaesa_rp2040 "0.1")

target_sources(yaesa_rp2040 PRIVATE
        queues_for_msgs_and_bits.c
        output_format.c
        uart_io.c
        f007t_decode_bits.c
        wh1080_decode_bits.c
        bme280_spi.c
        ds18b20_1w.c
        )

pico_enable_stdio_uart(yaesa_rp2040 0)
pico_enable_stdio_usb(yaesa_rp2040 1)

# Add the standard library to the build
target_link_libraries(yaesa_rp2040 
        pico_stdlib
        pico_multicore
        )

# Add any user requested libraries
target_link_libraries(yaesa_rp2040
        hardware_spi
        hardware_pio
        hardware_timer
        )

pico_add_extra_outputs(yaesa_rp2040)


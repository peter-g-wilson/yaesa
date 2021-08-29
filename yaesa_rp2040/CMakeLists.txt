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

add_custom_command(TARGET yaesa_rp2040 POST_BUILD
COMMAND ${CMAKE_OBJDUMP} -h yaesa_rp2040${CMAKE_EXECUTABLE_SUFFIX} >yaesa_rp2040.dis.src
COMMAND ${CMAKE_OBJDUMP} -dSl yaesa_rp2040${CMAKE_EXECUTABLE_SUFFIX} >>yaesa_rp2040.dis.src
)

if (DEFINED ENV{PICO_BOARD})
    if( $ENV{PICO_BOARD} STREQUAL "pimoroni_tiny2040")
        # modified linker script from 2 MB to 8 MB flash
        # original from ...\pico-sdk\src\rp2_common\pico_standard_link\memmap_default.ld
        ##  MEMORY
        ##  {
        ## -    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
        ## +    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 8192k
        ## ...
        ##  }
        # (?perhaps script a copy and sed?)
        set(FLASH_8MB_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/memmap_flash_8mb.ld")
        message("Using linker scipt ${FLASH_8MB_LINKER_SCRIPT}")
        pico_set_linker_script(yaesa_rp2040 ${FLASH_8MB_LINKER_SCRIPT})
        unset(FLASH_8MB_LINKER_SCRIPT)
    endif()
endif()

target_sources(yaesa_rp2040 PRIVATE
        queues_for_msgs_and_bits.c
        output_format.c
        serial_io.c
        f007t_decode_bits.c
        wh1080_decode_bits.c
        bme280_spi.c
        ds18b20_1w.c
        led_control.c
        sched_ms.c
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
        hardware_pwm
        hardware_sync
        )

pico_add_extra_outputs(yaesa_rp2040)


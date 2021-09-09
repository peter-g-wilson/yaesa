# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pi/pico/pico-sdk")

if (DEFINED ENV{PROJ_BOARD_COPY_TO_RAM})
    message("Environment variable PROJ_BOARD_COPY_TO_RAM set to $ENV{PROJ_BOARD_COPY_TO_RAM}")
    message("setting PICO_COPY_TO_RAM to 1")
    set(PICO_COPY_TO_RAM 1)
endif()

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(yaesa_rp2040 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(yaesa_rp2040 yaesa_rp2040.c )
pico_set_program_name(yaesa_rp2040 "yaesa_rp2040")
pico_set_program_version(yaesa_rp2040 "0.1")

if (DEFINED ENV{PROJ_BOARD_FLASH_SIZE})
    message("Environment variable PROJ_BOARD_FLASH_SIZE set to $ENV{PROJ_BOARD_FLASH_SIZE}")

    set(LINKER_SCRIPT_SRC_DIR "${PICO_SDK_PATH}/src/rp2_common/pico_standard_link")
    if (PICO_NO_FLASH)
        message("PROJ_BOARD_FLASH_SIZE was found set but variable PICO_NO_FLASH is also set !")
    elseif (PICO_USE_BLOCKED_RAM)
        set(LINKER_SCRIPT_SRC_NAME "memmap_blocked_ram")
    elseif (PICO_COPY_TO_RAM)
        set(LINKER_SCRIPT_SRC_NAME "memmap_copy_to_ram")
    else()
        set(LINKER_SCRIPT_SRC_NAME "memmap_default")
    endif()
    if (LINKER_SCRIPT_SRC_NAME)
        set(LINKER_SCRIPT_SRC_FULL_NAME "${LINKER_SCRIPT_SRC_DIR}/${LINKER_SCRIPT_SRC_NAME}.ld")
        message("Creating custom linker script based on ${LINKER_SCRIPT_SRC_FULL_NAME}")
    
        set(FLASH_SED_SEARCH "'FLASH(rx)'\ ':'\ 'ORIGIN'\ '='\ '0x10000000,'\ 'LENGTH'\ '='\ ")
        set(FLASH_SED_ARGS "s/${FLASH_SED_SEARCH}2048k/${FLASH_SED_SEARCH}$ENV{PROJ_BOARD_FLASH_SIZE}/")
        set(LINKER_SCRIPT_CUSTOM_NAME "${LINKER_SCRIPT_SRC_NAME}_$ENV{PROJ_BOARD_FLASH_SIZE}")
        set(LINKER_SCRIPT_CUSTOM_FULL_NAME "${CMAKE_CURRENT_BINARY_DIR}/${LINKER_SCRIPT_CUSTOM_NAME}.ld")
    
        set(LINKER_SCRIPT_CUSTOM_GEN_TARGET "yaesa_rp2040_linker_script_custom_gen_target")
        add_custom_target(${LINKER_SCRIPT_CUSTOM_GEN_TARGET} DEPENDS ${LINKER_SCRIPT_CUSTOM_FULL_NAME})
        add_custom_command( OUTPUT ${LINKER_SCRIPT_CUSTOM_FULL_NAME}
            COMMAND ${CMAKE_COMMAND} -E copy  ${LINKER_SCRIPT_SRC_FULL_NAME} ${LINKER_SCRIPT_CUSTOM_FULL_NAME}
            COMMAND sed -i ${FLASH_SED_ARGS} ${LINKER_SCRIPT_CUSTOM_FULL_NAME}
        )
        add_dependencies(yaesa_rp2040 ${LINKER_SCRIPT_CUSTOM_GEN_TARGET})
        message("Setting custom linker script to ${LINKER_SCRIPT_CUSTOM_FULL_NAME}")
        pico_set_linker_script(yaesa_rp2040 ${LINKER_SCRIPT_CUSTOM_FULL_NAME})
    endif()
endif()

pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/f007t_manchwithdelay.pio)
pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/wh1080_pwmpulsebits.pio)
pico_generate_pio_header(yaesa_rp2040 ${CMAKE_CURRENT_LIST_DIR}/ds18b20_1w.pio)

add_custom_command(TARGET yaesa_rp2040 POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -h yaesa_rp2040${CMAKE_EXECUTABLE_SUFFIX} >yaesa_rp2040.dis.src
    COMMAND ${CMAKE_OBJDUMP} -dSl yaesa_rp2040${CMAKE_EXECUTABLE_SUFFIX} >>yaesa_rp2040.dis.src
)

# target_compile_options(yaesa_rp2040 PRIVATE --save-temps)

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

target_compile_definitions(yaesa_rp2040 PRIVATE PICO_DEFAULT_UART=0)
if (DEFINED ENV{PICO_BOARD})
    if( $ENV{PICO_BOARD} STREQUAL "pimoroni_tiny2040")
        set(PIMORONI_BOARD_TINY2040 1)
    endif()
endif()
if (DEFINED ENV{PROJ_BOARD_STDIO_UART})
    if (PIMORONI_BOARD_TINY2040)
        message("BOARD = pimoroni_tiny2040 : STDIO on UART 0 with TX 28, RX 29")
        target_compile_definitions(yaesa_rp2040 PRIVATE PICO_DEFAULT_UART_TX_PIN=28)
        target_compile_definitions(yaesa_rp2040 PRIVATE PICO_DEFAULT_UART_RX_PIN=29)
    else()
        message("BOARD = default pico : STDIO on UART 0 with TX 21, RX 22")
        target_compile_definitions(yaesa_rp2040 PRIVATE PICO_DEFAULT_UART_TX_PIN=21)
        target_compile_definitions(yaesa_rp2040 PRIVATE PICO_DEFAULT_UART_RX_PIN=22)
    endif()    
    target_compile_definitions(yaesa_rp2040 PRIVATE PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1)
    pico_enable_stdio_uart(yaesa_rp2040 1)
    pico_enable_stdio_usb(yaesa_rp2040 0)
else()
    if (PIMORONI_BOARD_TINY2040)
        message("BOARD = pimoroni_tiny2040 : STDIO on USB")
    else()
        message("BOARD = default pico : STDIO on USB")
    endif()    
    pico_enable_stdio_uart(yaesa_rp2040 0)
    pico_enable_stdio_usb(yaesa_rp2040 1)
endif()

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


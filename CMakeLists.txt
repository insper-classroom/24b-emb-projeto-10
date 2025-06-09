# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in FreeRTOS
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos/FreeRTOS-Kernel)
include(freertos/FreeRTOS_Kernel_import.cmake)

# Project name
project(pico-freertos)

pico_sdk_init()

# Configuration projects
add_executable(pico-freertos
    main/main.c
)

target_include_directories(pico-freertos PRIVATE
    freertos/
)

target_link_libraries(pico-freertos PRIVATE
    pico_stdlib
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    hardware_pwm
    hardware_gpio
    hardware_irq
    hardware_timer
)

target_compile_options(pico-freertos PRIVATE -Wall)

# Run the entire project in SRAM
# pico_set_binary_type(pico-freertos copy_to_ram)

pico_add_extra_outputs(pico-freertos)

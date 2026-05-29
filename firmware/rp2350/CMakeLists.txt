set(PROJECT centipede-watcher)
cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD solderparty_rp2350_stamp_xl)
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_RP2350A 0)
set(PICO_RP2350B 1)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PROJECT} C CXX ASM)
pico_sdk_init()

# Regression target: Disk BASIC ROM (Bonobo upstream behavior)
add_executable(${PROJECT} watcher-march.cpp)
target_sources(${PROJECT} PRIVATE
    watcher-march.cpp
    disk11_rom.c
    cross-core.h
)
target_link_libraries(${PROJECT} PRIVATE
    hardware_pio
    pico_stdlib
    pico_multicore
    cmsis_core
)
pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)
pico_set_binary_type(${PROJECT} copy_to_ram)

# CoPico shipping target: X-BIOS on the Bonobo bus engine
set(COPICO_XBIOS_DIR ${CMAKE_CURRENT_LIST_DIR}/../bios)
set(COPICO_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/../../lib/shared)
add_executable(copico-watcher watcher-march.cpp)
target_sources(copico-watcher PRIVATE
    watcher-march.cpp
    cross-core.h
    copico_hat_config.cpp
    copico_esp32_bridge.cpp
    copico_flash_rom.cpp
)
target_compile_definitions(copico-watcher PRIVATE COPICO_XBIOS_ROM=1)
target_include_directories(copico-watcher PRIVATE ${COPICO_XBIOS_DIR} ${COPICO_SHARED_DIR})
target_link_libraries(copico-watcher PRIVATE
    hardware_pio
    hardware_flash
    hardware_spi
    pico_stdlib
    pico_multicore
    cmsis_core
)
pico_add_extra_outputs(copico-watcher)
pico_enable_stdio_usb(copico-watcher 1)
pico_enable_stdio_uart(copico-watcher 0)
pico_set_binary_type(copico-watcher copy_to_ram)

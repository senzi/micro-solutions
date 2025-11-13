# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(once C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. 默认入口还是 once.c，外加你的驱动源文件
add_executable(once
        once.c
        drivers/lcd_pcf8576.c
)

pico_set_program_name(once "once")
pico_set_program_version(once "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(once 0)
pico_enable_stdio_usb(once 1)

# Add the standard library to the build
# 再加上你要用到的硬件库
target_link_libraries(once
        pico_stdlib
        hardware_gpio
        hardware_i2c
        hardware_timer
        hardware_clocks
)

# Add the standard include files to the build
# 把 drivers 目录也加进 include 搜索路径里，这样能找到 board.h、lcd_pcf8576.h
target_include_directories(once PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/drivers
)

# Generate UF2, bin, etc.
pico_add_extra_outputs(once)

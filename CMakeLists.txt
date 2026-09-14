# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(encoder_reader C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(encoder_reader
    main.c
)

target_link_libraries(encoder_reader
    pico_stdlib
)

# USB CDC:
# printf(), puts(), putchar() -> USB serial connection.
pico_enable_stdio_usb(encoder_reader 1)

# Hardware UART is not used in this first version.
pico_enable_stdio_uart(encoder_reader 0)

# Generate UF2, ELF, BIN and related output files.
pico_add_extra_outputs(encoder_reader)
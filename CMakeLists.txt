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
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(simvibe_botonera_v2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(simvibe_botonera_v2 main.cpp )

pico_set_program_name(simvibe_botonera_v2 "simvibe_botonera_v2")
pico_set_program_version(simvibe_botonera_v2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(simvibe_botonera_v2 0)
pico_enable_stdio_usb(simvibe_botonera_v2 0)

# Add the sources to the build
target_sources(simvibe_botonera_v2 PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.cpp
        )

# Add the standard include files to the build
target_include_directories(simvibe_botonera_v2 PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(simvibe_botonera_v2 PUBLIC pico_stdlib pico_unique_id tinyusb_device tinyusb_board)

pico_add_extra_outputs(simvibe_botonera_v2)
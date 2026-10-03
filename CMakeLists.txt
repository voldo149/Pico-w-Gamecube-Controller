cmake_minimum_required(VERSION 3.25.1)

# Placa por defecto: Pico W (para Pico normal: -DPICO_BOARD=pico, con -DSWITCH_BLUETOOTH=OFF)
if(NOT PICO_BOARD)
  set(PICO_BOARD pico_w)
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

project(retro_pico_switch C CXX ASM)

pico_sdk_init()

include($ENV{PICO_SDK_PATH}/tools/CMakeLists.txt)

add_subdirectory(src)
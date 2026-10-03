# Tipo de control: "Gamecube" o "N64"
set(CONTROLLER_TYPE "Gamecube" CACHE STRING "Tipo de control: Gamecube o N64")
add_compile_definitions(CONTROLLER_TYPE="${CONTROLLER_TYPE}")

# Conexion a la Switch: ON = Bluetooth (requiere Pico W), OFF = USB (Pico o Pico W)
option(SWITCH_BLUETOOTH "Conectar a la Switch por Bluetooth (solo Pico W)" ON)
if(SWITCH_BLUETOOTH)
  add_compile_definitions(SWITCH_BLUETOOTH)
endif()

# LOW_POWER_MODE=ON: version de bateria (arranca dormido y se apaga solo)
option(LOW_POWER_MODE "Version de bateria: arranque dormido y apagado automatico" OFF)
if(LOW_POWER_MODE)
  add_compile_definitions(LOW_POWER_MODE=1)
endif()

add_subdirectory(otherController)
add_subdirectory(switchController)

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} pico_stdlib switchBluetooth switchUsb n64 gamecube)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

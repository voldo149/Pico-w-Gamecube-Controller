# Tipo de control: "Gamecube" o "N64"
set(CONTROLLER_TYPE "Gamecube" CACHE STRING "Tipo de control: Gamecube o N64")
add_compile_definitions(CONTROLLER_TYPE="${CONTROLLER_TYPE}")

# XINPUT=ON: control de Xbox 360 por USB para PC (solo Gamecube), con el mapeo
# y la capa Z de include/MapeoXInput.h. Ignora SWITCH_BLUETOOTH.
option(XINPUT "Control de Xbox 360 (XInput) por USB para PC" OFF)
if(XINPUT)
  add_compile_definitions(XINPUT)
  set(SWITCH_BLUETOOTH OFF CACHE BOOL "" FORCE)
endif()

# XBOX_BT=ON: control de Xbox (Series) por Bluetooth LE para PC (solo Pico W y
# Gamecube), con el mismo mapeo y capa Z que XINPUT.
option(XBOX_BT "Control de Xbox por Bluetooth LE para PC (Pico W)" OFF)
if(XBOX_BT)
  add_compile_definitions(XBOX_BT)
  set(SWITCH_BLUETOOTH OFF CACHE BOOL "" FORCE)
endif()

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

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(XINPUT)
  add_subdirectory(xinputController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib xinputUsb n64 gamecube)
elseif(XBOX_BT)
  add_subdirectory(xboxBleController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib xboxBle n64 gamecube)
else()
  add_subdirectory(switchController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib switchBluetooth switchUsb n64 gamecube)
endif()

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

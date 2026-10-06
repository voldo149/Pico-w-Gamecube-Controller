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

# GC_PC=ON: firmware de PC en un solo .uf2 (solo Pico W y Gamecube):
# cable USB a una PC = Xbox 360 por USB; sin cable = Xbox por Bluetooth LE.
# Mismo mapeo y capa Z que XINPUT (include/MapeoXInput.h).
option(GC_PC "Firmware de PC: USB (Xbox 360) + Bluetooth (Xbox) en uno (Pico W)" OFF)
if(GC_PC)
  add_compile_definitions(GC_PC)
  set(SWITCH_BLUETOOTH OFF CACHE BOOL "" FORCE)
endif()

# MAPEO_CLASICO=ON (con GC_PC o XINPUT): Z = RB; Start toque = Start, mantener = Back,
# Start + D-pad arriba = boton Xbox. OFF = mapeo con capa Z. Ver include/MapeoXInput.h.
option(MAPEO_CLASICO "Mapeo clasico (Z = RB, Start toque/mantener)" OFF)
if(MAPEO_CLASICO)
  add_compile_definitions(MAPEO_CLASICO)
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
if(GC_PC)
  add_subdirectory(xinputController)
  add_subdirectory(xboxBleController)
  add_subdirectory(pcController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib modoPc n64 gamecube)
elseif(XINPUT)
  add_subdirectory(xinputController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib xinputUsb n64 gamecube)
else()
  add_subdirectory(switchController)
  target_link_libraries(${PROJECT_NAME} pico_stdlib switchBluetooth switchUsb n64 gamecube)
endif()

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})

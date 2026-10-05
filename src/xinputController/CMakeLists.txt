# Remapeo compartido por el modo USB y el Bluetooth
add_library(remapeo Remapeo.cpp)
target_include_directories(remapeo PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_library(xinputUsb XInputUsb.cpp)
target_link_libraries(xinputUsb pico_stdlib pico_bootrom hardware_pio hardware_watchdog tinyusb_device gamecube remapeo)
target_include_directories(xinputUsb PUBLIC ${PROJECT_SOURCE_DIR}/include)

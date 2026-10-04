add_library(xinputUsb XInputUsb.cpp Remapeo.cpp)
target_link_libraries(xinputUsb pico_stdlib hardware_pio tinyusb_device gamecube)
target_include_directories(xinputUsb PUBLIC ${PROJECT_SOURCE_DIR}/include)

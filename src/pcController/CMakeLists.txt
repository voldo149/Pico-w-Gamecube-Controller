add_library(modoPc ModoPc.cpp)
target_link_libraries(modoPc pico_stdlib hardware_watchdog gamecube xinputUsb xboxBle)
target_include_directories(modoPc PUBLIC ${PROJECT_SOURCE_DIR}/include)

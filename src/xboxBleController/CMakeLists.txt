add_library(xboxBle XboxBle.cpp ${CMAKE_CURRENT_LIST_DIR}/../xinputController/Remapeo.cpp)
target_link_libraries(xboxBle
  pico_stdlib
  pico_bootrom
  pico_unique_id
  hardware_watchdog
  hardware_pio
  gamecube
  pico_btstack_ble
  pico_btstack_cyw43
  pico_btstack_hci_transport_cyw43
  pico_cyw43_arch_threadsafe_background
  )
target_compile_definitions(xboxBle PUBLIC CYW43_LWIP=0)
target_include_directories(xboxBle PUBLIC ${PROJECT_SOURCE_DIR}/include)
pico_btstack_make_gatt_header(xboxBle PRIVATE "${CMAKE_CURRENT_LIST_DIR}/xbox_ble.gatt")

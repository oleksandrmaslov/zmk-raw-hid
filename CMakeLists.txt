if (CONFIG_RAW_HID)
  zephyr_library()
  zephyr_library_link_libraries(subsys__bluetooth__host)

  zephyr_library_sources(src/events.c)
  zephyr_library_sources(src/usb_hid.c)

  if (CONFIG_ZMK_BLE)
    zephyr_library_sources(src/hog.c)
    zephyr_library_sources(src/now_playing_forwarder.c)
  endif()
  
if (CONFIG_ZMK_SPLIT AND NOT CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(src/now_playing_client.c)
  zephyr_library_link_libraries(bluetooth_gatt)
endif()

  zephyr_include_directories(include)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()

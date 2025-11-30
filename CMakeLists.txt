if (CONFIG_RAW_HID)
  zephyr_library()

  zephyr_library_sources(src/events.c)
  zephyr_library_sources(src/usb_hid.c)

  if (CONFIG_ZMK_BLE)
    zephyr_library_sources(src/hog.c)
  endif()

  if (CONFIG_RAW_HID_FORWARD_TO_PERIPHERAL AND CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_library_sources(src/split_bridge.c)
  endif()

  zephyr_include_directories(include)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()

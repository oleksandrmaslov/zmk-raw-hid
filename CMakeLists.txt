zephyr_library()

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources_ifdef(CONFIG_ZMK_RAW_HID src/raw-hid/raw_hid.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_RAW_HID src/raw-hid/usb_hid.c)

  if (CONFIG_ZMK_BLE)
    zephyr_library_sources_ifdef(CONFIG_ZMK_RAW_HID src/raw-hid/hog.c)
  endif()

  zephyr_include_directories(include)
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
endif()

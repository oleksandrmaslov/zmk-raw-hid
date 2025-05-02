cmake_minimum_required(VERSION 3.13.1)

# Pull in Zephyr/ZMK boilerplate
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zmk-raw-hid)

if (CONFIG_RAW_HID)
  # Declare this as a Zephyr library
  zephyr_library()

  # Core Raw HID sources
  zephyr_library_sources(
    src/events.c
    src/usb_hid.c
  )

  # Now-Playing forwarder: only build on the central role
  if (CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_library_sources(
      src/now_playing_forwarder.c
    )
  endif()

  # BLE HID over GATT (central-side) support
  if (CONFIG_ZMK_BLE)
    zephyr_library_sources(
      src/hog.c
    )
  endif()

  # Expose this module’s public headers
  zephyr_include_directories(
    include
  )

  # Expose any app-level headers you may have in your config
  zephyr_include_directories(
    ${APPLICATION_SOURCE_DIR}/include
  )

  # ==== NEW: Expose the relay module’s headers ====
  # Adjust the relative path if your workspace layout is different.
  zephyr_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../../zmk-split-peripheral-output-relay/include
  )
endif()


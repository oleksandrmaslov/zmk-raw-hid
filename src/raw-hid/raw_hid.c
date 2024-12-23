#include <zmk/raw-hid/raw_hid.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(raw_hid, CONFIG_ZMK_RAW_HID_LOG_LEVEL);

__attribute__((weak)) void process_raw_hid_data(uint8_t *data, uint8_t length) {
    LOG_WRN("display_process_raw_hid_data is not overriden");

    return;
}

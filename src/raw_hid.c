#include <raw_hid/raw_hid.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

__attribute__((weak)) void process_raw_hid_data(uint8_t *data) {
    LOG_WRN("process_raw_hid_data is not overriden");

    return;
}

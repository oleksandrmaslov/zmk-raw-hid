#include <raw_hid/events.h>
#include <zmk/event_manager.h>
#include <zmk/split/output-relay/event.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int raw_hid_split_forward_listener(const zmk_event_t *eh) {
    const struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t payload_size =
        MIN((uint8_t)event->length, (uint8_t)ZMK_SPLIT_PERIPHERAL_OUTPUT_PAYLOAD_MAX);
    if (payload_size == 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (event->length > payload_size) {
        LOG_WRN("Raw HID payload truncated from %u to %u bytes", event->length, payload_size);
    }

    int err = zmk_split_bt_invoke_output_channel(CONFIG_RAW_HID_SPLIT_RELAY_CHANNEL,
                                                 event->data[0], event->data, payload_size);
    if (err) {
        LOG_ERR("Failed to forward Raw HID payload to peripheral (err %d)", err);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_split_forward, raw_hid_split_forward_listener);
ZMK_SUBSCRIPTION(raw_hid_split_forward, raw_hid_received_event);

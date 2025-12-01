#include <raw_hid/events.h>
#include <zmk/event_manager.h>
#include <zmk/split/output-relay/event.h>

#include <zephyr/bluetooth/att.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int raw_hid_split_forward_listener(const zmk_event_t *eh) {
    const struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Use the real payload length, not the padded raw HID frame length. */
    /* Default ATT MTU is 23 unless negotiated; assume 23 to keep writes safe. */
#ifdef BT_ATT_DEFAULT_LE_MTU
    const uint8_t att_payload_max = BT_ATT_DEFAULT_LE_MTU - 3; /* ATT header is 3 bytes */
#else
    const uint8_t att_payload_max = 20; /* 23 - 3 */
#endif
    const uint8_t safe_payload_cap =
        MIN((uint8_t)ZMK_SPLIT_PERIPHERAL_OUTPUT_PAYLOAD_MAX, att_payload_max);

    uint8_t payload_size = MIN((uint8_t)event->length, safe_payload_cap);

    /* If the message carries its own length (media/title/artist), respect it. */
    if (payload_size > 2 && event->data[1] > 0) {
        uint8_t declared = event->data[1] + 2; /* type + len + data */
        payload_size = MIN(payload_size, declared);
    }

    if (payload_size == 0) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t forward_buf[ZMK_SPLIT_PERIPHERAL_OUTPUT_PAYLOAD_MAX] = {0};
    memcpy(forward_buf, event->data, payload_size);

    /* Adjust declared length if we truncated; avoid oversized strings on the peripheral. */
    if (payload_size > 2) {
        uint8_t declared_data_len = forward_buf[1];
        uint8_t max_declared = payload_size - 2;
        if (declared_data_len > max_declared) {
            forward_buf[1] = max_declared;
        }
    }

    if (event->length > payload_size) {
        LOG_WRN("Raw HID payload truncated from %u to %u bytes", event->length, payload_size);
    }

    int err = zmk_split_bt_invoke_output_channel(CONFIG_RAW_HID_SPLIT_RELAY_CHANNEL,
                                                 forward_buf[0], forward_buf, payload_size);
    if (err) {
        LOG_ERR("Failed to forward Raw HID payload to peripheral (err %d)", err);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_split_forward, raw_hid_split_forward_listener);
ZMK_SUBSCRIPTION(raw_hid_split_forward, raw_hid_received_event);

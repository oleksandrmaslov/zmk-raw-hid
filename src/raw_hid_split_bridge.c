#include <raw_hid/events.h>
#include <zmk/split/output-relay/event.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int raw_hid_bridge_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *ev = as_raw_hid_received_event(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_split_bt_output_relay_event relay = {
        .relay_channel = CONFIG_RAW_HID_SPLIT_CHANNEL,
        .payload_len = ev->length,
    };
    if (relay.payload_len > MAX_PAYLOAD_LEN) {
        relay.payload_len = MAX_PAYLOAD_LEN;
    }
    memcpy(relay.payload, ev->data, relay.payload_len);

    const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(now_playing_dev));
    if (dev) {
        zmk_split_bt_invoke_output(dev, relay);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_split_bridge, raw_hid_bridge_listener);
ZMK_SUBSCRIPTION(raw_hid_split_bridge, raw_hid_received_event);
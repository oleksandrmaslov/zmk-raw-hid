#include <raw_hid/events.h>
#include <zmk/split/output-relay/event.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_SIZE)
#define RELAY_SIZE_FIELD payload_size
#elif IS_ENABLED(CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_LEN)
#define RELAY_SIZE_FIELD payload_len
#else
#error "RAW_HID output relay field not configured"
#endif

static int raw_hid_bridge_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *ev = as_raw_hid_received_event(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_split_bt_output_relay_event relay = {
        .relay_channel = CONFIG_RAW_HID_SPLIT_CHANNEL,
        .RELAY_SIZE_FIELD = ev->length,
    };
#ifdef MAX_PAYLOAD_LEN
    if (relay.RELAY_SIZE_FIELD > MAX_PAYLOAD_LEN) {
        relay.RELAY_SIZE_FIELD = MAX_PAYLOAD_LEN;
    }
    memcpy(relay.payload, ev->data, relay.RELAY_SIZE_FIELD);
#else
    ARG_UNUSED(relay);
    ARG_UNUSED(ev);
#endif

 const struct device *dev = device_get_binding(DT_LABEL(DT_NODELABEL(nice_view_adapter)));
    if (dev) {
        zmk_split_bt_invoke_output(dev, relay);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_split_bridge, raw_hid_bridge_listener);
ZMK_SUBSCRIPTION(raw_hid_split_bridge, raw_hid_received_event);

#include <zephyr/kernel.h>            // if you need any Zephyr APIs
#include <zmk/event_manager.h>        // for ZMK_LISTENER / ZMK_SUBSCRIPTION
#include <raw_hid/raw_hid.h>         // ⟵ raw_hid_received_event + as_raw_hid_received_event
#include <zmk/split/transport/central.h>          // defines raw_hid_received_event

static int raw_hid_forwarder(const zmk_event_t *eh) {
    const struct raw_hid_received_event *evt = as_raw_hid_received_event(eh);
    if (!evt) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    struct zmk_split_bt_output_relay_event out_ev = {
        .relay_channel = 1,       // match your DTS relay-channel
        .value         = evt->data[0],
    };
    zmk_split_bt_invoke_output(NULL, out_ev);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(raw_hid_split_forwarder, raw_hid_forwarder);
ZMK_SUBSCRIPTION(raw_hid_split_forwarder, raw_hid_received_event);

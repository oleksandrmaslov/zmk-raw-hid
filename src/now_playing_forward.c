#include <zmk/split/transport/central.h>
#include <raw_hid/raw_hid.h>
#include <zmk/event_manager.h>

#if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static int forward_raw_hid(const zmk_event_t *eh) {
    const struct raw_hid_received_event *evt = as_raw_hid_received_event(eh);
    if (!evt) return ZMK_EV_EVENT_BUBBLE;

    struct zmk_split_transport_central_command cmd = {
        .type = ZMK_SPLIT_TRANSPORT_CENTRAL_CMD_TYPE_USER, // pick an unused command type
        .data = { .user_data = { /* copy your 32 bytes here */ } }
    };

    extern struct zmk_split_transport_central *active_transport;
    active_transport->api->send_command(0, cmd); // 0 for the first peripheral
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_forward, forward_raw_hid);
ZMK_SUBSCRIPTION(raw_hid_forward, raw_hid_received_event);
#endif

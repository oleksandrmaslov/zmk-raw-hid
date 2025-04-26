#include <zmk/event_manager.h>
#include <raw_hid/events.h>
#include <zmk/split/transport/central.h>
#include <string.h>                    /* <- for memcpy  */

#if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static int forward_raw_hid(const zmk_event_t *eh) {
    const struct raw_hid_received_event *evt = as_raw_hid_received_event(eh);
    if (!evt) return ZMK_EV_EVENT_BUBBLE;

    /* build and send the transport command */
  /* pass the transport pointer, not NULL */
if (active_transport) {
      active_transport->api->send_command(active_transport, cmd);
}
    cmd.type = ZMK_SPLIT_TRANSPORT_CENTRAL_CMD_TYPE_RAW_HID;
    memcpy(cmd.data.raw_hid.data, evt->data, evt->length);

    extern struct zmk_split_transport_central *active_transport;
    active_transport->api->send_command(0, cmd);

    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(raw_hid_forward, forward_raw_hid);
ZMK_SUBSCRIPTION(raw_hid_forward, raw_hid_received_event);
#endif  /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

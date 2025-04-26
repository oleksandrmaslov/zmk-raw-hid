#include <zephyr/sys/util.h>                 /* STRUCT_SECTION_FOREACH */
#include <zmk/event_manager.h>
#include <raw_hid/events.h>
#include <zmk/split/transport/central.h>     /* transport registry     */
#include <raw_hid/raw_hid.h>
#include <string.h>

#if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void forward_raw_hid(const struct raw_hid_received_event *ev)
{
    struct zmk_split_transport_central_command cmd = {
        .type = ZMK_SPLIT_TRANSPORT_CENTRAL_CMD_TYPE_RAW_HID,
    };
    memcpy(cmd.data.raw_hid.data, ev->data,
           MIN(ev->length, sizeof(cmd.data.raw_hid.data)));

    /* Iterate over every registered central transport and send */
    struct zmk_split_transport_central *transport;
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, transport) {
        transport->api->send_command(transport, cmd);
    }
}

ZMK_LISTENER(raw_hid_forward, forward_raw_hid)
ZMK_SUBSCRIPTION(raw_hid_forward, raw_hid_received_event)
#endif  /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

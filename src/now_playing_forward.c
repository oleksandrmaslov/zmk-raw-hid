#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>                   /* STRUCT_SECTION_FOREACH */
#include <zmk/split/transport/central.h>       /* transport registry     */
#include <raw_hid/raw_hid.h>
#include <string.h>

LOG_MODULE_REGISTER(raw_hid_forward, CONFIG_ZMK_LOG_LEVEL);

/* Event-manager callback must accept zmk_event_t* and return int */
static int forward_raw_hid(const zmk_event_t *eh)
{
    const struct raw_hid_received_event *ev = as_raw_hid_received_event(eh);
    if (!ev) { return 0; }                          /* not our event */

    struct zmk_split_transport_central_command cmd = {
        .type = ZMK_SPLIT_TRANSPORT_CENTRAL_CMD_TYPE_RAW_HID,
    };
    memcpy(cmd.data.raw_hid.data, ev->data,
           MIN(ev->length, sizeof(cmd.data.raw_hid.data)));

    LOG_DBG("forward RAW-HID id 0x%02X len %d", ev->data[0], ev->length);

    /* Broadcast to *all* registered central transports (there’s only one) */
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, transport) {
        transport->api->send_command(0, cmd);       /* source-id = 0 */
    }
    return 0;
}

ZMK_LISTENER(raw_hid_forward, forward_raw_hid)
ZMK_SUBSCRIPTION(raw_hid_forward, raw_hid_received_event)

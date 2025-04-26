#include <raw_hid/events.h>          /* <-- add this */
#include <zmk/event_manager.h>               /* zmk_event_t, listener macros */
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>                 /* STRUCT_SECTION_FOREACH()     */
#include <zmk/split/transport/central.h>     /* transport registry           */
#include <raw_hid/raw_hid.h>
#include <string.h>

LOG_MODULE_REGISTER(raw_hid_forward, CONFIG_ZMK_LOG_LEVEL);

/* correct prototype: returns int, arg is const zmk_event_t* */
static int forward_raw_hid(const zmk_event_t *eh)
{
    const struct raw_hid_received_event *ev = as_raw_hid_received_event(eh);
    if (!ev) {
        return 0;        /* not for us */
    }

    struct zmk_split_transport_central_command cmd = {
        .type = ZMK_SPLIT_TRANSPORT_CENTRAL_CMD_TYPE_RAW_HID,
    };
    memcpy(cmd.data.raw_hid.data, ev->data,
           MIN(ev->length, sizeof(cmd.data.raw_hid.data)));

    LOG_DBG("forward RAW-HID id 0x%02X len %d", ev->data[0], ev->length);

    /* broadcast to every registered central transport */
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, t) {
        t->api->send_command(0 /* src-id */, cmd);
    }
    return 0;
}

ZMK_LISTENER(raw_hid_forward, forward_raw_hid)
ZMK_SUBSCRIPTION(raw_hid_forward, raw_hid_received_event)

#include <zephyr/device.h>
#include <raw-hid/raw_hid.h>
#include <zmk/split/output-relay/event.h>
#include <string.h>

#define ARTIST_CH 10
#define TRACK_CH  11
#define STR_SZ    32

static int np_forward_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *raw = as_raw_hid_received_event(eh);
    if (!raw) return ZMK_EV_EVENT_BUBBLE;

    char artist[STR_SZ+1] = {0}, track[STR_SZ+1] = {0};
    memcpy(artist, raw->data, STR_SZ);
    memcpy(track,  raw->data + STR_SZ, STR_SZ);

    zmk_split_peripheral_output_relay_send(ARTIST_CH,
                                           (uint8_t *)artist,
                                           strlen(artist));
    zmk_split_peripheral_output_relay_send(TRACK_CH,
                                           (uint8_t *)track,
                                           strlen(track));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(now_playing_forward, np_forward_listener);
ZMK_SUBSCRIPTION(now_playing_forward, raw_hid_received_event);

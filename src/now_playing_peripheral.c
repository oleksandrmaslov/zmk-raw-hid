#include <zephyr/device.h>
#include <zmk/split/output-relay/event.h>
#include <nice_view_hid/hid.h>
#include <string.h>

#define ARTIST_CH 0xAD  // must match your relay-channel for artist (0xAD)
#define TITLE_CH  0xAE  // and for title (0xAE)

static int np_periph_listener(const struct zmk_split_bt_output_relay_event *ev) {
    if (ev->relay_channel == ARTIST_CH) {
        struct media_artist_notification notif = {0};
        memcpy(notif.artist, ev->payload, MIN(ev->len, sizeof(notif.artist)-1));
        ZMK_EVENT_RAISE(notif);
    }
    else if (ev->relay_channel == TITLE_CH) {
        struct media_title_notification notif = {0};
        memcpy(notif.title, ev->payload, MIN(ev->len, sizeof(notif.title)-1));
        ZMK_EVENT_RAISE(notif);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(np_periph, np_periph_listener);
ZMK_SUBSCRIPTION(np_periph, zmk_split_bt_output_relay_event);

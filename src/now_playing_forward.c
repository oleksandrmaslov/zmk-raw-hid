#include <raw_hid/events.h>
#include <zmk/split/output-relay/event.h>  // for zmk_split_bt_invoke_output
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <nice_view_hid/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define NOW_PLAYING_CHANNEL 1

static char curr_artist[64];
static char curr_title[64];

/* Called on raw HID data (from companion app) */
static int now_playing_hid_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *r = as_raw_hid_received_event(eh);
    if (!r) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    uint8_t *data = r->data;
    uint8_t type = data[0];

    if (type == _MEDIA_ARTIST) {
        /* Copy artist name (assuming ASCII & null-terminated or fixed-length) */
        size_t len = strnlen((char *)&data[1], sizeof(curr_artist)-1);
        memcpy(curr_artist, &data[1], len);
        curr_artist[len] = '\0';
    }
    else if (type == _MEDIA_TITLE) {
        /* Copy title and then send combined packet */
        size_t len = strnlen((char *)&data[1], sizeof(curr_title)-1);
        memcpy(curr_title, &data[1], len);
        curr_title[len] = '\0';

        /* Prepare payload: [artist]\0[title]\0 */
        uint8_t payload[128];
        size_t artist_len = strlen(curr_artist);
        size_t title_len = strlen(curr_title);
        if (artist_len + 1 + title_len + 1 > sizeof(payload)) {
            /* Truncate if too long */
            title_len = sizeof(payload) - artist_len - 2;
        }
        memcpy(payload, curr_artist, artist_len);
        payload[artist_len] = '\0';
        memcpy(&payload[artist_len+1], curr_title, title_len);
        payload[artist_len+1+title_len] = '\0';
        size_t total_len = artist_len + 1 + title_len + 1;

        /* Build the relay event */
        const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(now_playing_dev));
        struct zmk_split_bt_output_relay_event ev;
        ev.relay_channel = NOW_PLAYING_CHANNEL;
        ev.payload_len = total_len;
        memcpy(&ev.payload, payload, total_len);

LOG_DBG("NowPlaying→Split: chan=%d len=%d artist=\"%s\" title=\"%s\"",
            ev.relay_channel,
            ev.payload_len,
            curr_artist,
            curr_title);

        /* Enqueue the event to send via BLE */
        zmk_split_bt_invoke_output(dev, ev);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

/* Register listener */
ZMK_LISTENER(now_playing_listener, now_playing_hid_listener);
ZMK_SUBSCRIPTION(now_playing_listener, raw_hid_received_event);

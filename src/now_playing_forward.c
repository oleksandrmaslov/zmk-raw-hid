#include <raw_hid/events.h>
#include <zmk/split/output-relay/event.h>
#include <device.h>
#include <string.h>

#define NOW_PLAYING_CHANNEL 1

static char curr_artist[24] = {0};
static char curr_title[24] = {0};

static int now_playing_hid_listener(const zmk_event_t *eh) {
    const struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (!event) return ZMK_EV_EVENT_BUBBLE;

    const uint8_t *data = event->data;
    uint8_t type = data[0];

    if (type == _MEDIA_ARTIST) {
        size_t len = strnlen((char *)&data[1], sizeof(curr_artist)-1);
        memcpy(curr_artist, &data[1], len);
        curr_artist[len] = '\0';
    } else if (type == _MEDIA_TITLE) {
        size_t len = strnlen((char *)&data[1], sizeof(curr_title)-1);
        memcpy(curr_title, &data[1], len);
        curr_title[len] = '\0';

        // Prepare payload: [artist]\0[title]\0
        uint8_t payload[MAX_PAYLOAD_LEN];
        size_t artist_len = strlen(curr_artist);
        size_t title_len = strlen(curr_title);

        // Ensure both fit in 32 bytes
        if (artist_len + 1 + title_len + 1 > MAX_PAYLOAD_LEN) {
            if (artist_len > 15) artist_len = 15;
            if (title_len > 15) title_len = 15;
        }

        memcpy(payload, curr_artist, artist_len);
        payload[artist_len] = '\0';
        memcpy(&payload[artist_len+1], curr_title, title_len);
        payload[artist_len+1+title_len] = '\0';
        size_t total_len = artist_len + 1 + title_len + 1;

        struct zmk_split_bt_output_relay_event ev = {
            .relay_channel = NOW_PLAYING_CHANNEL,
            .payload_len = total_len,
        };
        memcpy(ev.payload, payload, total_len);

        const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(now_playing_dev));
        zmk_split_bt_invoke_output(dev, ev);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(now_playing_listener, now_playing_hid_listener);
ZMK_SUBSCRIPTION(now_playing_listener, raw_hid_received_event);

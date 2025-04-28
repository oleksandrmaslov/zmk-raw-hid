// File: now_playing_forwarder.c (central)
#include <zmk/event_manager.h>
#include <raw_hid/raw_hid.h>
#include <string.h>

#define HID_TYPE_MEDIA_ARTIST 0xAD  // QMK HID companion media artist tag&#8203;:contentReference[oaicite:3]{index=3}
#define HID_TYPE_MEDIA_TITLE  0xAE  // QMK HID companion media title tag&#8203;:contentReference[oaicite:4]{index=4}

static char curr_artist[32] = {0};
static char curr_title[32]  = {0};
static char last_artist[32] = {0};
static char last_title[32]  = {0};

// Listener for raw HID reports from host
static int now_playing_forwarder(const zmk_event_t *eh) {
    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (!event) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t *data = event->data;
    uint8_t length = event->length;  // should be 32 (CONFIG_RAW_HID_REPORT_SIZE)&#8203;:contentReference[oaicite:5]{index=5}

    // Parse incoming media artist/title strings
    if (data[0] == HID_TYPE_MEDIA_ARTIST) {
        // Copy artist string (data[1..length-1]) and ensure null termination
        size_t len = strnlen((char *)&data[1], length-1);
        memcpy(curr_artist, &data[1], len);
        curr_artist[len] = '\0';
    } else if (data[0] == HID_TYPE_MEDIA_TITLE) {
        size_t len = strnlen((char *)&data[1], length-1);
        memcpy(curr_title, &data[1], len);
        curr_title[len] = '\0';
    } else {
        // Not a media packet; ignore
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Only forward when we have both artist and title, and either changed
    if (curr_artist[0] != '\0' && curr_title[0] != '\0') {
        if (strcmp(curr_artist, last_artist) != 0 ||
            strcmp(curr_title,  last_title)  != 0) {
            // Format "Title – Artist" into a 32-byte packet
            char packet[32] = {0};
            snprintf(packet, sizeof(packet), "%s - %s", curr_title, curr_artist);
            // Send 32-byte raw HID report to peripheral
            raw_hid_send((uint8_t *)packet, sizeof(packet));

            // Save as last sent track
            strncpy(last_artist, curr_artist, sizeof(last_artist)-1);
            strncpy(last_title,  curr_title,  sizeof(last_title)-1);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

// Register listener for raw HID events&#8203;:contentReference[oaicite:6]{index=6}
ZMK_LISTENER(npf_forward, now_playing_forwarder);
ZMK_SUBSCRIPTION(npf_forward, raw_hid_received_event);

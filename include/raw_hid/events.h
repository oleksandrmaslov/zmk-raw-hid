#pragma once

#include <zmk/event_manager.h>

struct raw_hid_received_event {
    uint8_t data[32];   /* ← fixed payload */
    uint8_t length;     /* always 32 for now */
};
ZMK_EVENT_DECLARE(raw_hid_received_event);

struct raw_hid_sent_event {
    uint8_t data[32];
    uint8_t length;
};
ZMK_EVENT_DECLARE(raw_hid_sent_event);

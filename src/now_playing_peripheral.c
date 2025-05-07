#include <zmk/events/output_event.h>
#include <zmk/widgets/nice_view.h>

static int np_periph_listener(const zmk_event_t *eh) {
    struct zmk_output_event *ev = as_zmk_output_event(eh);
    if (!ev) return ZMK_EV_EVENT_BUBBLE;

    if (ev->source == 10) {
        nice_view_set_artist((char *)ev->data, ev->len);
    } else if (ev->source == 11) {
        nice_view_set_track((char *)ev->data, ev->len);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(np_periph, np_periph_listener);
ZMK_SUBSCRIPTION(np_periph, zmk_output_event);

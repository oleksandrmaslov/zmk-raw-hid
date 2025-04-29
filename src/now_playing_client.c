#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>
#include <zmk/event_manager.h>
#include <raw_hid/events.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// UUIDs for HID service and report characteristic
static struct bt_uuid_16 uuid_hids    = BT_UUID_INIT_16(BT_UUID_HIDS_VAL);
static struct bt_uuid_16 uuid_report  = BT_UUID_INIT_16(BT_UUID_HIDS_REPORT_VAL);

// GATT discovery and subscription params
static struct bt_gatt_discover_params disc_params;
static struct bt_gatt_subscribe_params sub_params;

// Notification callback: convert to raw_hid_received_event
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (!data || length == 0) {
        return BT_GATT_ITER_STOP;
    }

    raise_raw_hid_received_event((struct raw_hid_received_event){
        .data   = (uint8_t *)data,
        .length = length,
    });

    // Keep listening
    return BT_GATT_ITER_CONTINUE;
}

// Discovery callback: once we find the Input Report characteristic, subscribe
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    if (!attr) {
        LOG_ERR("HID service not found");
        return BT_GATT_ITER_STOP;
    }

    uint16_t value_handle = bt_gatt_attr_value_handle(attr);

    sub_params.notify        = notify_cb;
    sub_params.value         = BT_GATT_CCC_NOTIFY;
    sub_params.value_handle  = value_handle;
    sub_params.ccc_handle    = value_handle + 1;  // CCC descriptor is right after the value

    int err = bt_gatt_subscribe(conn, &sub_params);
    if (!err) {
        LOG_INF("Subscribed to HID notifications");
    } else {
        LOG_ERR("Failed to subscribe (%d)", err);
    }

    return BT_GATT_ITER_STOP;
}

// Kick off discovery when split-peripheral connects
static void on_split_status(const struct zmk_event_split_peripheral_status_changed *ev)
{
    if (!ev->connected) {
        return;
    }

    // Discover HID service → report char
    disc_params.uuid         = &uuid_hids.uuid;
    disc_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;
    disc_params.start_handle = 0x0001;
    disc_params.end_handle   = 0xffff;
    disc_params.func         = discover_cb;

    bt_gatt_discover(bt_conn_ref(ev->conn), &disc_params);
}

ZMK_SUBSCRIPTION(on_split_status, raw_hid_received_event);  // or use split_peripheral_status_changed

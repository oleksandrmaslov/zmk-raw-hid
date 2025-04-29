// File: src/now_playing_client.c

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <raw_hid/events.h>

LOG_MODULE_REGISTER(zmk, CONFIG_ZMK_LOG_LEVEL);

// Static subscription parameters
static struct bt_gatt_subscribe_params sub_params;

// Forward declarations
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length);
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *dp);
static void connected_cb(struct bt_conn *conn, uint8_t err);
static void disconnected_cb(struct bt_conn *conn, uint8_t reason);

// 1) Notification handler: fire raw_hid_received_event when data arrives
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (data && length) {
        raise_raw_hid_received_event((struct raw_hid_received_event){
            .data   = (uint8_t *)data,
            .length = length,
        });
    }
    return BT_GATT_ITER_CONTINUE;
}

// 2) Discovery callback: look for the HID Report characteristic
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *dp)
{
    if (!attr) {
        LOG_ERR("HID Report characteristic not found");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (bt_uuid_cmp(chrc->uuid, BT_UUID_HIDS_REPORT) == 0) {
        sub_params.value_handle = chrc->value_handle;
        sub_params.ccc_handle   = chrc->value_handle + 1;
        sub_params.notify       = notify_cb;
        sub_params.value        = BT_GATT_CCC_NOTIFY;

        int err = bt_gatt_subscribe(conn, &sub_params);
        if (err) {
            LOG_ERR("Subscribe failed: %d", err);
        } else {
            LOG_INF("Subscribed to Now Playing HID reports");
        }
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

// 3) Connection callback: kick off GATT discovery when link comes up
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    static struct bt_gatt_discover_params dp;
    dp.uuid         = BT_UUID_HIDS_REPORT;
    dp.func         = discover_cb;
    dp.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    dp.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    dp.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

    int d_err = bt_gatt_discover(conn, &dp);
    if (d_err) {
        LOG_ERR("Discover failed: %d", d_err);
    } else {
        LOG_INF("Discovering Now Playing HID report");
    }
}

// 4) Disconnection callback: reset our subscribe state
static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    memset(&sub_params, 0, sizeof(sub_params));
    LOG_INF("Disconnected (reason %u)", reason);
}

// 5) Register our BLE connection callbacks
static struct bt_conn_cb conn_callbacks = {
    .connected    = connected_cb,
    .disconnected = disconnected_cb,
};
BT_CONN_CB_DEFINE(conn_callbacks);

// File: src/now_playing_client.c

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <raw_hid/events.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct bt_conn *current_conn;
static struct bt_gatt_discover_params disc_params;
static struct bt_gatt_subscribe_params subscribe_params;

// Notify callback
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length) {
    if (data && length) {
        raise_raw_hid_received_event((struct raw_hid_received_event){
            .data   = (uint8_t *)data,
            .length = length,
        });
    }
    return BT_GATT_ITER_CONTINUE;
}

// Descriptor discovery callback
static uint8_t discover_desc_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *dp) {
    if (!attr) {
        LOG_ERR("CCCD not found");
        return BT_GATT_ITER_STOP;
    }
    LOG_INF("Found CCCD handle: 0x%04x", attr->handle);

    subscribe_params.ccc_handle = attr->handle;
    subscribe_params.notify     = notify_cb;
    subscribe_params.value      = BT_GATT_CCC_NOTIFY;

    int err = bt_gatt_subscribe(conn, &subscribe_params);
    if (err && err != -EALREADY) {
        LOG_ERR("Subscribe failed: %d", err);
    } else {
        LOG_INF("Subscribed to Raw HID notifications");
    }
    return BT_GATT_ITER_STOP;
}

// Characteristic discovery callback
static uint8_t discover_char_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *dp) {
    if (!attr) {
        LOG_ERR("HID Report char not found");
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *chrc = attr->user_data;
    subscribe_params.value_handle = chrc->value_handle;

    // Kick off CCCD discovery
    static struct bt_gatt_discover_params dp_desc;
    dp_desc.uuid         = BT_UUID_GATT_CCC;
    dp_desc.func         = discover_desc_cb;
    dp_desc.start_handle = chrc->value_handle + 1;
    dp_desc.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    dp_desc.type         = BT_GATT_DISCOVER_DESCRIPTOR;
    bt_gatt_discover(conn, &dp_desc);
    return BT_GATT_ITER_STOP;
}

// Connection callback
static void connected_cb(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connect failed (err %u)", err);
        return;
    }
    current_conn = bt_conn_ref(conn);

    // Start characteristic discovery
    disc_params.uuid         = BT_UUID_HIDS_REPORT;
    disc_params.func         = discover_char_cb;
    disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    disc_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    disc_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;
    bt_gatt_discover(conn, &disc_params);
    LOG_INF("Discovering HID report characteristic");
}

// Disconnection callback
static void disconnected_cb(struct bt_conn *conn, uint8_t reason) {
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    memset(&subscribe_params, 0, sizeof(subscribe_params));
    LOG_INF("Disconnected (reason %u)", reason);
}

static struct bt_conn_cb conn_callbacks = {
    .connected    = connected_cb,
    .disconnected = disconnected_cb,
};
BT_CONN_CB_DEFINE(conn_callbacks);

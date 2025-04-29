#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <raw_hid/events.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Subscription parameters (zero-init)
static struct bt_gatt_subscribe_params sub_params = {0};

// When we see a GATT Report characteristic, subscribe to it
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *dp)
{
    if (!attr) {
        LOG_ERR("Report characteristic not found");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (bt_uuid_cmp(chrc->uuid, BT_UUID_HIDS_REPORT) == 0) {
        sub_params.ccc_handle   = chrc->value_handle + 1;
        sub_params.value_handle = chrc->value_handle;
        sub_params.notify       = [](struct bt_conn *c, struct bt_gatt_subscribe_params *p,
                                    const void *data, uint16_t len) {
            if (data && len) {
                raise_raw_hid_received_event((struct raw_hid_received_event){
                    .data   = (uint8_t *)data,
                    .length = len
                });
            }
            return BT_GATT_ITER_CONTINUE;
        };
        int err = bt_gatt_subscribe(conn, &sub_params);
        if (err) {
            LOG_ERR("Subscribe failed: %d", err);
        } else {
            LOG_INF("Subscribed to Now Playing reports");
        }
        return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_CONTINUE;
}

// Called by Zephyr on every new connection
static void bt_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    // Kick off GATT discovery for the HID Report char
    static struct bt_gatt_discover_params dp;
    dp.uuid        = BT_UUID_HIDS_REPORT;
    dp.func        = discover_cb;
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

// Register our connection callbacks
static struct bt_conn_cb conn_callbacks = {
    .connected    = bt_connected,
    .disconnected = [](struct bt_conn *conn, uint8_t reason) {
        memset(&sub_params, 0, sizeof(sub_params));
        LOG_INF("Disconnected (reason %u)", reason);
    },
};
BT_CONN_CB_DEFINE(conn_callbacks);

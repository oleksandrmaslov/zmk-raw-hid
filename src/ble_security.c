#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_security, LOG_LEVEL_INF);

// Forward these to your existing raw_hid_discover_cb & on_disconnected
extern uint8_t raw_hid_discover_cb(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   struct bt_gatt_discover_params *dp);
extern void on_disconnected(struct bt_conn *conn, uint8_t reason);

// Request encryption as soon as we connect
static void sec_on_connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("BLE connect failed: %u", err);
        return;
    }
    int rc = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (rc) {
        LOG_ERR("Requesting encryption failed: %d", rc);
    } else {
        LOG_INF("Requested Security Level 2");
    }
}

/* Make a real, static bt_uuid_16 object */
static struct bt_uuid_16 raw_hid_uuid = BT_UUID_INIT_16(BT_UUID_HIDS_REPORT_VAL);

/* GATT discovery params — no initializer here */
static struct bt_gatt_discover_params dp;

static void sec_on_security_changed(struct bt_conn *conn,
                                    bt_security_t level,
                                    enum bt_security_err err) {
    if (err) {
        LOG_ERR("Encryption failed: %d", err);
        return;
    }
    if (level < BT_SECURITY_L2) {
        // Not encrypted yet; wait for next callback
        return;
    }
    /* Fill dp at runtime so we only use compile-time constants above */
    dp.uuid         = &raw_hid_uuid.uuid;
    dp.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    dp.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    dp.type         = BT_GATT_DISCOVER_CHARACTERISTIC;
    dp.func         = raw_hid_discover_cb;
    int rc = bt_gatt_discover(conn, &dp);
    if (rc) {
        LOG_ERR("HID discovery failed: %d", rc);
    }
}

// Install our callbacks
static struct bt_conn_cb sec_conn_cbs = {
    .connected        = sec_on_connected,
    .security_changed = sec_on_security_changed,
    .disconnected     = on_disconnected,
};
BT_CONN_CB_DEFINE(sec_conn_cbs);

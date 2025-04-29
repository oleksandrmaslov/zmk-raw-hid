// File: app/src/now_playing_client.c
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>
#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>  
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/ble.h>
#include <raw_hid/events.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Compile only in the peripheral half:
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

// HID Service and Report Characteristic UUIDs
static struct bt_uuid_16 hid_svc_uuid  = BT_UUID_INIT_16(BT_UUID_HIDS_VAL);
static struct bt_uuid_16 report_uuid   = BT_UUID_INIT_16(BT_UUID_HIDS_REPORT_VAL);

static struct bt_gatt_subscribe_params subscribe_params = {0};

/* Notification callback */
static uint8_t notify_cb(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (!data || length == 0) {
        return BT_GATT_ITER_CONTINUE;
    }

    raise_raw_hid_received_event((struct raw_hid_received_event){
        .data   = (uint8_t *)data,
        .length = length,
    });
    return BT_GATT_ITER_CONTINUE;
}

/* Discovery callback: subscribe once we find the Report characteristic */
static uint8_t discover_cb(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    if (!attr) {
        LOG_ERR("HID Report characteristic not found");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *chrc = attr->user_data;
    if (!bt_uuid_cmp(chrc->uuid, &report_uuid.uuid)) {
        uint16_t value_handle = chrc->value_handle;

        subscribe_params.notify        = notify_cb;
        subscribe_params.value         = BT_GATT_CCC_NOTIFY;
        subscribe_params.value_handle  = value_handle;
        subscribe_params.ccc_handle    = value_handle + 1;

        int err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err) {
            LOG_ERR("Subscribe failed: %d", err);
        } else {
            LOG_INF("Subscribed to Now Playing HID reports");
        }
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

/* Start discovery on split-peripheral connect */
static void on_split_status(const struct zmk_split_peripheral_status_changed *ev)
{
    if (!ev->connected) {
        return;
    }

    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!conn) {
        LOG_ERR("No active BLE connection");
        return;
    }

    static struct bt_gatt_discover_params disc_params = {0};
    disc_params.uuid         = &report_uuid.uuid;
    disc_params.func         = discover_cb;
    disc_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    disc_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

    int err = bt_gatt_discover(conn, &disc_params);
    if (err) {
        LOG_ERR("Discovery failed: %d", err);
    } else {
        LOG_INF("Discovering Now Playing HID report");
    }
}

ZMK_LISTENER(npf_client, on_split_status);
ZMK_SUBSCRIPTION(npf_client, zmk_split_peripheral_status_changed);

#endif /* peripheral-only guard */

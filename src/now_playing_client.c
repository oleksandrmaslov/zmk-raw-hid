/*
 * now_playing_client.c — "Now Playing" HID client for split peripherals
 *
 * Peripheral-side module for a split-keyboard setup. Listens for BLE
 * connections from the central half, discovers and subscribes to the
 * HID-Over-GATT Report characteristic that carries "Now Playing"
 * media packets, and raises raw_hid_received_event events for display
 * widgets to render.
 *
 * Build guard: Included only when CONFIG_ZMK_SPLIT is enabled and
 * CONFIG_ZMK_SPLIT_ROLE_CENTRAL is disabled.
 */

 #include <zephyr/logging/log.h>
 #include <zephyr/bluetooth/conn.h>
 #include <zephyr/bluetooth/gatt.h>
 #include <raw_hid/events.h>
 
 LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
 
 #if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
 
 /* UUID: HID-Over-GATT Report characteristic for "Now Playing" packets */
 static struct bt_uuid_16 report_uuid = BT_UUID_INIT_16(BT_UUID_HIDS_REPORT_VAL);
 
 /* GATT subscription parameters */
 static struct bt_gatt_subscribe_params subscribe_params = {
    .notify        = notify_cb,
    .value_handle  = 0, // set in discover_cb
    .ccc_handle    = 0, // set in discover_cb (value_handle + 1)
    .value         = BT_GATT_CCC_NOTIFY,
 };
 
 /* GATT discovery parameters */
 static struct bt_gatt_discover_params discover_params;
 
 /* Notification callback: dispatch raw HID received event */
 static uint8_t notify_cb(struct bt_conn *conn,
                          struct bt_gatt_subscribe_params *params,
                          const void *data, uint16_t length)
 {
     if (data && length > 0) {
         raise_raw_hid_received_event((struct raw_hid_received_event){
             .data   = (uint8_t *)data,
             .length = length,
         });
     }
     return BT_GATT_ITER_CONTINUE;
 }
 
 /* Discovery callback: subscribe to the found Report characteristic */
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
         subscribe_params.value_handle = chrc->value_handle;
         subscribe_params.notify       = notify_cb;
 
         int err = bt_gatt_subscribe(conn, &subscribe_params);
         if (err) {
             LOG_ERR("Subscription failed (err %d)", err);
         } else {
             LOG_INF("Subscribed to Now Playing HID reports");
         }
         return BT_GATT_ITER_STOP;
     }
     return BT_GATT_ITER_CONTINUE;
 }
 
 /* Connection callback: initiate discovery when BLE link is established */
 static void connected_cb(struct bt_conn *conn, uint8_t err)
 {
     if (err) {
         LOG_WRN("BLE connection error: %u", err);
         return;
     }
     LOG_INF("Peripheral connected: discovering HID Report characteristic");
 
     discover_params.uuid         = &report_uuid.uuid;
     discover_params.func         = discover_cb;
     discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
     discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
     discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;
 
     err = bt_gatt_discover(conn, &discover_params);
     if (err) {
         LOG_ERR("GATT discovery failed (err %d)", err);
     }
 }
 
 /* Register connection callbacks */
 BT_CONN_CB_DEFINE(conn_callbacks) = {
     .connected = connected_cb,
 };
 
 #endif /* CONFIG_ZMK_SPLIT && !CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

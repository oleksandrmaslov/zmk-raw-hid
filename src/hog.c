/*
 * hog.c — Raw HID over BLE service & forwarding for split CENTRAL only
 *
 * Drop this into modules/zmk-raw-hid/src/hog.c to replace the existing file.
 */

 #include <zephyr/kernel.h>

 #if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && defined(CONFIG_ZMK_BLE)
 
 #include <raw_hid/raw_hid.h>
 #include <raw_hid/events.h>
 #include <zmk/ble.h>
 
 #include <zephyr/bluetooth/gatt.h>
 #include <zephyr/logging/log.h>
 LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
 
 /* HID Service “HIDS” information flags */
 enum {
     HIDS_REMOTE_WAKE          = BIT(0),
     HIDS_NORMALLY_CONNECTABLE = BIT(1),
 };
 
 /* HIDS Information characteristic (read-only) */
struct hids_info {
    uint16_t version;
    uint8_t  code;
    uint8_t  flags;
} __packed;

static const struct hids_info hids_info_data = {
    .version = 0x0000,
    .code    = 0x00,
    .flags   = HIDS_NORMALLY_CONNECTABLE | HIDS_REMOTE_WAKE,
};
 
 /* Report reference descriptor */
 struct hids_report {
     uint8_t id;
     uint8_t type;
 } __packed;
 
 static const struct hids_report report_input = { .id = 0x00, .type = 0x01 };
 static const struct hids_report report_output = { .id = 0x00, .type = 0x02 };
 
 /* Read HIDS Info */
 static ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
 {
     return bt_gatt_attr_read(conn, attr, buf, len, offset,
                              &((struct hids_info){0}), sizeof(struct hids_info));
 }
 
 /* Read Report Map (descriptor from raw_hid module) */
 extern const uint8_t raw_hid_report_desc[];
 static ssize_t read_map(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
 {
     return bt_gatt_attr_read(conn, attr, buf, len, offset,
                              raw_hid_report_desc, sizeof(raw_hid_report_desc));
 }
 
 /* Read Report Reference */
 static ssize_t read_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
 {
     return bt_gatt_attr_read(conn, attr, buf, len, offset,
                              attr->user_data, sizeof(struct hids_report));
 }
 
 /* Host writes Raw HID output reports here */
 static ssize_t write_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
 {
     if (offset != 0) {
         return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
     }
     const uint8_t *data = buf;
     LOG_DBG("BT HID ► Raw HID recv len=%u", len);
     LOG_HEXDUMP_DBG(data, len, "Raw HID data");
     struct raw_hid_received_event ev = {0};
     ev.length = len;
     memcpy(ev.data, data, ev.length);
     raise_raw_hid_received_event(ev);
     return len;
 }
 
 /* Control Point (optional) */
 static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
 {
     uint8_t *dst = attr->user_data;
     if (offset + len > sizeof(*dst)) {
         return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
     }
     memcpy(dst + offset, buf, len);
     return len;
 }
 
 /* Define the HID over GATT service */
 BT_GATT_SERVICE_DEFINE(raw_hog_svc,
     BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
 
     /* Info */
     BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO,
                            BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ,
                            read_info, NULL, NULL),
 
     /* Report Map */
     BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP,
                            BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ_ENCRYPT,
                            read_map, NULL, NULL),
 
     /* Input Report (notify to host) */
     BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                            BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                            BT_GATT_PERM_READ_ENCRYPT,
                            NULL, NULL, NULL),
     BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
     BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF,
                        BT_GATT_PERM_READ_ENCRYPT,
                        read_report_ref, NULL, &report_input),
 
     /* Output Report (write from host) */
     BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                            BT_GATT_CHRC_READ |
                            BT_GATT_CHRC_WRITE |
                            BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                            BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                            NULL, write_report, NULL),
     BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF,
                        BT_GATT_PERM_READ_ENCRYPT,
                        read_report_ref, NULL, &report_output),
 
     /* Control Point */
     BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                            BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                            BT_GATT_PERM_WRITE,
                            NULL, write_ctrl_point, NULL)
 );
 
 /* Send a Raw HID INPUT report to the host */
 static void send_report(const uint8_t *data, uint8_t len)
 {
     struct bt_conn *conn = zmk_ble_active_profile_conn();
     if (!conn) {
         LOG_WRN("BT HID ◄ Raw HID send: no conn");
         return;
     }
 
     uint8_t report[CONFIG_RAW_HID_REPORT_SIZE] = {0};
     memcpy(report, data, len);
 
     struct bt_gatt_notify_params params = {
         .attr = &raw_hog_svc.attrs[5],
         .data = &report,
         .len  = CONFIG_RAW_HID_REPORT_SIZE,
     };
 
     int err = bt_gatt_notify_cb(conn, &params);
     if (err == -EPERM) {
         bt_conn_set_security(conn, BT_SECURITY_L2);
     } else if (err) {
         LOG_ERR("BT HID ◄ Raw HID notify err %d", err);
     }
     bt_conn_unref(conn);
 }
 
 /* Listen for ZMK-side sent events and forward over BLE */
 static int on_raw_hid_sent(const zmk_event_t *eh)
 {
     const struct raw_hid_sent_event *ev = as_raw_hid_sent_event(eh);
     if (ev) {
         send_report(ev->data, ev->length);
     }
     return ZMK_EV_EVENT_BUBBLE;
 }
 ZMK_LISTENER(bt_raw_hid_sent, on_raw_hid_sent);
 ZMK_SUBSCRIPTION(bt_raw_hid_sent, raw_hid_sent_event);
 
 /* Work queue for deferred sends */
 K_THREAD_STACK_DEFINE(raw_hog_q_stack, CONFIG_ZMK_BLE_THREAD_STACK_SIZE);
 static struct k_work_q raw_hog_work_q;
 
 static int raw_hog_init(const struct device *dev)
 {
     ARG_UNUSED(dev);
     static const struct k_work_queue_config cfg = { .name = "raw_hog_q" };
     k_work_queue_start(&raw_hog_work_q,
                        raw_hog_q_stack,
                        K_THREAD_STACK_SIZEOF(raw_hog_q_stack),
                        CONFIG_ZMK_BLE_THREAD_PRIORITY,
                        &cfg);
     return 0;
 }
 SYS_INIT(raw_hog_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);
 
 #else  /* peripheral: no-op, nothing to build */
 
  /* empty stub so the file compiles but adds no symbols */
 
 #endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL && CONFIG_ZMK_BLE */

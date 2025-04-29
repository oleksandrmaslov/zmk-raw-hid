#include <raw_hid/raw_hid.h>
#include <raw_hid/events.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// --- GATT service + write callback: always needed on peripheral ---
enum {
    HIDS_REMOTE_WAKE        = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};
struct hids_info { /* … */ } __packed;
struct hids_report { /* … */ } __packed;

// … (read_hids_info, read_hids_report_ref, read_hids_raw_hid_report_map) …

static ssize_t write_hids_raw_hid_report(struct bt_conn *conn,
                                         const struct bt_gatt_attr *attr,
                                         const void *buf, uint16_t len,
                                         uint16_t offset, uint8_t flags) {
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    uint8_t *data = (uint8_t *)buf;
    LOG_INF("BT - Received Raw HID report, %u bytes", len);
    LOG_HEXDUMP_DBG(data, len, "BT - Received Raw HID report");
    raise_raw_hid_received_event((struct raw_hid_received_event){
        .data   = data,
        .length = len,
    });
    return len;
}

BT_GATT_SERVICE_DEFINE(raw_hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    /* … HIDS_INFO, REPORT_MAP, INPUT REPORT (notify) … */
    /* RECEIVE characteristic: write from central → peripheral */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
        NULL, write_hids_raw_hid_report, NULL),
    /* … CTRL_POINT … */
);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

/* 
 * The following section is CENTRAL-ONLY: it pulls in
 * zmk_ble_active_profile_conn(), the notify helper,
 * the raw_hid_sent_event listener, and SYS_INIT.
 */

#include <zmk/ble.h>

static void send_report(const uint8_t *data, uint8_t len) {
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!conn) {
        LOG_ERR("Not connected to active profile");
        return;
    }

    LOG_INF("BT - Sending Raw HID report, %u bytes", len);
    uint8_t report[CONFIG_RAW_HID_REPORT_SIZE] = {0};
    memcpy(report, data, len);

    struct bt_gatt_notify_params params = {
        .attr = &raw_hog_svc.attrs[5],  // notify characteristic index
        .data = report,
        .len  = CONFIG_RAW_HID_REPORT_SIZE,
    };

    int rc = bt_gatt_notify_cb(conn, &params);
    if (rc == -EPERM) {
        bt_conn_set_security(conn, BT_SECURITY_L2);
    } else if (rc) {
        LOG_ERR("Notify error %d", rc);
    }
    bt_conn_unref(conn);
}

static int raw_hid_sent_event_listener(const zmk_event_t *eh) {
    const struct raw_hid_sent_event *evt = as_raw_hid_sent_event(eh);
    if (evt) {
        send_report(evt->data, evt->length);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bt_process_raw_hid_sent, raw_hid_sent_event_listener);
ZMK_SUBSCRIPTION(bt_process_raw_hid_sent, raw_hid_sent_event);

K_THREAD_STACK_DEFINE(raw_hog_q_stack, CONFIG_ZMK_BLE_THREAD_STACK_SIZE);
struct k_work_q raw_hog_work_q;

static int raw_hog_init(const struct device *dev) {
    ARG_UNUSED(dev);
    static const struct k_work_queue_config cfg = {
        .name = "HID Over GATT Send Work",
    };
    k_work_queue_start(&raw_hog_work_q, raw_hog_q_stack,
                       K_THREAD_STACK_SIZEOF(raw_hog_q_stack),
                       CONFIG_ZMK_BLE_THREAD_PRIORITY, &cfg);
    return 0;
}
SYS_INIT(raw_hog_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

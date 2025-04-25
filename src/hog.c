/*
 * hog.c — Raw HID over BLE service & forwarding
 * Only active when this device is the split-central (host-facing) half.
 */

#include <zephyr/kernel.h>

#ifdef CONFIG_ZMK_SPLIT_ROLE_CENTRAL

#include <raw_hid/raw_hid.h>
#include <raw_hid/events.h>
#include <zmk/ble.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

enum {
    HIDS_REMOTE_WAKE         = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
    uint16_t version;
    uint8_t  code;
    uint8_t  flags;
} __packed = {
    .version = 0x0000,
    .code    = 0x00,
    .flags   = HIDS_NORMALLY_CONNECTABLE | HIDS_REMOTE_WAKE,
};

struct hids_report {
    uint8_t id;
    uint8_t type;
} __packed;

static const struct hids_report raw_hid_report_input = {
    .id   = 0x00,
    .type = 0x01,  /* input */
};

static const struct hids_report raw_hid_report_output = {
    .id   = 0x00,
    .type = 0x02,  /* output */
};

extern const uint8_t raw_hid_report_desc[];
static ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             attr->user_data, sizeof(struct hids_info));
}
static ssize_t read_map(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             raw_hid_report_desc, sizeof(raw_hid_report_desc));
}
static ssize_t read_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             attr->user_data, sizeof(struct hids_report));
}
static ssize_t write_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    const uint8_t *data = buf;
    LOG_DBG("BT HID ► Raw HID recv, len=%u", len);
    LOG_HEXDUMP_DBG(data, len, "Raw HID data");
    /* fire the ZMK event for central-side listeners */
    raise_raw_hid_received_event((struct raw_hid_received_event){
        .data   = (uint8_t *)data,
        .length = len,
    });
    return len;
}
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

BT_GATT_SERVICE_DEFINE(raw_hog_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),

    /* HIDS Information */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ,
                           read_info, NULL, &((struct hids_info){0})),

    /* Report Map */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ_ENCRYPT,
                           read_map, NULL, NULL),

    /* Input Report (to host) */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT,
                           NULL, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF,
                       BT_GATT_PERM_READ_ENCRYPT,
                       read_report_ref, NULL, &raw_hid_report_input),

    /* Output Report (from host) */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ |
                               BT_GATT_CHRC_WRITE |
                               BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                           NULL, write_report, NULL),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF,
                       BT_GATT_PERM_READ_ENCRYPT,
                       read_report_ref, NULL, &raw_hid_report_output),

    /* Control Point */
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           NULL, write_ctrl_point, NULL)
);

static void send_report(const uint8_t *data, uint8_t len)
{
    struct bt_conn *conn = zmk_ble_active_profile_conn();
    if (!conn) {
        LOG_WRN("BT HID ◄ Raw HID send: no active profile");
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

static int on_raw_hid_sent(const zmk_event_t *eh)
{
    const struct raw_hid_sent_event *ev = as_raw_hid_sent_event(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    send_report(ev->data, ev->length);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(bt_raw_hid_sent, on_raw_hid_sent);
ZMK_SUBSCRIPTION(bt_raw_hid_sent, raw_hid_sent_event);

K_THREAD_STACK_DEFINE(raw_hog_q_stack, CONFIG_ZMK_BLE_THREAD_STACK_SIZE);
static struct k_work_q raw_hog_work_q;

static int raw_hog_init(void)
{
    static const struct k_work_queue_config cfg = {
        .name = "raw_hog_q",
    };
    k_work_queue_start(&raw_hog_work_q,
                       raw_hog_q_stack,
                       K_THREAD_STACK_SIZEOF(raw_hog_q_stack),
                       CONFIG_ZMK_BLE_THREAD_PRIORITY,
                       &cfg);
    return 0;
}
SYS_INIT(raw_hog_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);

#else  /* peripheral split role: no-op stub */

#include <zephyr/kernel.h>
static int raw_hog_init(void) { return 0; }
SYS_INIT(raw_hog_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);

#endif  /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

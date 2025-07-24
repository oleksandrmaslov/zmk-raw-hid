# Raw HID support for ZMK

This [ZMK module](https://zmk.dev/docs/features/modules) enables Raw HID functionality for ZMK.

## Features

It allows bidirectional communication between host computer and keyboard over HID interface. Both USB and BT connections are supported.

Example usage - [display widget](https://github.com/zzeneg/zmk-nice-view-hid) with HID support, which can accept data from computer using [companion app](https://github.com/zzeneg/qmk-hid-host) and display current time/layout/volume.

## Installation

> [!WARNING]
> Enabling this module alone does not bring any functionality by default. You need another module or custom code to consume events, and companion app to send them.

To use, first add this module to your `config/west.yml` by adding a new entry to `remotes` and `projects`:

```yaml west.yml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: zzeneg # <-- new entry
      url-base: https://github.com/zzeneg
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-raw-hid # <-- new entry
      remote: zzeneg
      revision: main
    - name: zmk-split-peripheral-output-relay # for split forwarding
      remote: zzeneg
      revision: peripheral
    - name: zmk-nice-view-hid # display widget
      remote: zzeneg
      revision: peripheral-2
  self:
    path: config
```

For more information, including instructions for building locally, check out the ZMK docs on [building with modules](https://zmk.dev/docs/features/modules#building-with-modules).

Add the `raw_hid_adapter` as an additional shield to your build, e.g. in `build.yaml`:

```yaml build.yaml
---
include:
  - board: nice_nano_v2
    shield: molekula raw_hid_adapter
```

### Forward Raw HID data to split peripherals

To relay Raw HID packets over the ZMK split link, define an output relay device
in the left/central `board.overlay` and reference it from the right/peripheral
overlay. The relay channel must match the `CONFIG_RAW_HID_SPLIT_CHANNEL` option
(defaults to `1`). An example configuration is shown below:

```dts
/* board_left.overlay - central */
/ {
    now_playing_dev: now_playing_dev {
        compatible = "zmk,output-split-output-relay";
        #binding-cells = <0>;
    };

    now_playing_relay: now_playing_relay {
        compatible = "zmk,split-peripheral-output-relay";
        device = <&now_playing_dev>;
        relay-channel = <1>; /* must match CONFIG_RAW_HID_SPLIT_CHANNEL */
    };
};

/* board_right.overlay - peripheral */
/ {
    now_playing_dev: now_playing_dev {
        compatible = "zmk,output-split-output-relay";
        #binding-cells = <0>;
    };

    now_playing_relay: now_playing_relay {
        compatible = "zmk,split-peripheral-output-relay";
        device = <&now_playing_dev>;
        relay-channel = <1>;
    };
};
```

Ensure these nodes are defined at the root of each overlay. Do **not** place
them under `&split_central` or `&split_peripheral` as those nodes are not
standalone and will cause an "undefined node label 'split'" error.

Depending on the version of `zmk-split-peripheral-output-relay` in use, the
relay event structure may expose the length field as `payload_len` or
`payload_size`. Select the matching option via
`CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_LEN` or
`CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_SIZE`.

When the central half receives Raw HID data (for example from
`qmk-hid-host`), the module will forward the payload over the `now_playing_dev`
relay device. On the peripheral side the
`zmk-split-peripheral-output-relay` module converts the message into a
`raw_hid_received_event`, which can be consumed by widgets such as
`zmk-nice-view-hid` to show artist and track information.

## Adding support in other modules

Subscribe to `raw_hid_received_event` and implement your own listener:

```c
static int raw_hid_received_event_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event) {
        // do something
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(process_raw_hid_event, raw_hid_received_event_listener);
ZMK_SUBSCRIPTION(process_raw_hid_event, raw_hid_received_event);
```

## Configuration

| Name                         | Description                       | Default |
| ---------------------------- | --------------------------------- | ------- |
| `CONFIG_RAW_HID`             | Enable Raw HID module             | n       |
| `CONFIG_RAW_HID_USAGE_PAGE`  | HID Usage Page                    | 0xFF60  |
| `CONFIG_RAW_HID_USAGE`       | HID Usage                         | 0x61    |
| `CONFIG_RAW_HID_REPORT_SIZE` | HID Report size (number of bytes) | 32      |
| `CONFIG_RAW_HID_DEVICE`      | New HID device name               | HID_1   |
| `CONFIG_RAW_HID_SPLIT_CHANNEL` | Output relay channel for Raw HID  | 1       |
| `CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_LEN` | Relay struct uses `payload_len` | y |
| `CONFIG_RAW_HID_OUTPUT_RELAY_FIELD_PAYLOAD_SIZE` | Relay struct uses `payload_size` | n |

## Thanks

This module is heavily based on [badjeff's zmk-hid-io](https://github.com/badjeff/zmk-hid-io).

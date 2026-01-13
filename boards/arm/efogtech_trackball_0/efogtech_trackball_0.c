#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include "zmk/endpoints.h"

#if IS_ENABLED(CONFIG_SHELL)
#define shprint(_sh, _fmt, ...) \
do { \
    if ((_sh) != NULL) \
        shell_print((_sh), _fmt, ##__VA_ARGS__); \
} while (0)

static int cmd_version(const struct shell *sh, const size_t argc, char **argv) {
    shprint(sh, "Firmware version: %d.%d.%d", CONFIG_BOARD_EFOGTECH_0_VER_MAJOR, CONFIG_BOARD_EFOGTECH_0_VER_MINOR, CONFIG_BOARD_EFOGTECH_0_VER_PATCH);
    return 0;
}

static int cmd_output(const struct shell *sh, const size_t argc, char **argv) {
    if (argc < 1) {
        shprint(sh, "Usage: board output [usb|ble]");
        return -EINVAL;
    }

    if (strcmp(argv[1], "usb") == 0) {
        zmk_endpoints_select_transport(ZMK_TRANSPORT_USB);
    } else if (strcmp(argv[1], "ble") == 0) {
        zmk_endpoints_select_transport(ZMK_TRANSPORT_BLE);
    } else {
        if (zmk_endpoints_selected().transport == ZMK_TRANSPORT_USB) {
            shprint(sh, "Output: USB");
        } else {
            shprint(sh, "Output: BLE");
        }

        return 0;
    }

    shprint(sh, "Done.");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_board,
    SHELL_CMD(output, NULL, "Get or set output channel (USB/BLE)", cmd_output),
    SHELL_CMD(version, NULL, "Read firmware version", cmd_version),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(board, &sub_board, "Control the device", NULL);
#endif

static const struct device *p0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

static void set_3v3_en(const bool en) {
    gpio_pin_configure(p1, 0, GPIO_OUTPUT);
    gpio_pin_set(p1, 0, en);
}

static void set_rgb_en(const bool en) {
    gpio_pin_configure(p1, 3, GPIO_OUTPUT);
    gpio_pin_set(p1, 3, en);
}

static void set_bl_en(const bool en) {
    gpio_pin_configure(p0, 20, GPIO_OUTPUT);
    gpio_pin_set(p0, 20, en);
}

static int pinmux_efgtch_trckbl_init(void) {
    set_3v3_en(false);
    set_rgb_en(false);
    set_bl_en(false);
    return 0;
}

SYS_INIT(pinmux_efgtch_trckbl_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
static int usb_conn_chg(const zmk_event_t *eh) {
    if (zmk_usb_get_conn_state() == ZMK_USB_CONN_HID) {
        pm_device_action_run(uart, PM_DEVICE_ACTION_RESUME);
    } else {
        pm_device_action_run(uart, PM_DEVICE_ACTION_SUSPEND);
    }

    return 0;
}

ZMK_SUBSCRIPTION(board_root, zmk_usb_conn_state_changed);
ZMK_LISTENER(board_root, usb_conn_chg)
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/devicetree.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>

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

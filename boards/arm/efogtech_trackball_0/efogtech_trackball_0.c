#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>

#include "zephyr/bluetooth/bluetooth.h"
#include "zmk/endpoints.h"
#include "zmk/settings.h"
#include "zmk/keymap.h"
#include "zmk/studio/core.h"
#include "zmk_adaptive_feedback/adaptive_feedback.h"

#define DT_DRV_COMPAT zmk_endgame
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

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

static int cmd_reboot(const struct shell *sh, const size_t argc, char **argv) {
    shprint(sh, "Rebooting device...");
    k_sleep(K_MSEC(100));
    sys_reboot(SYS_REBOOT_COLD);
}

static int cmd_erase(const struct shell *sh, const size_t argc, char **argv) {
    shprint(sh, "I hope you know what you're doing.");
    k_sleep(K_MSEC(20));
    bt_unpair(BT_ID_DEFAULT, NULL);

    for (int i = 0; i < 8; i++) {
        char setting_name[15];
        snprintf(setting_name, sizeof(setting_name), "ble/profiles/%d", i);

        const int err = settings_delete(setting_name);
        if (err) {
            LOG_ERR("Failed to delete setting: %d", err);
        }
    }

    for (int i = 0; i < 8; i++) {
        char setting_name[32];
        snprintf(setting_name, sizeof(setting_name), "ble/peripheral_addresses/%d", i);

        const int err = settings_delete(setting_name);
        if (err) {
            LOG_ERR("Failed to delete setting: %d", err);
        }
    }

    const int rc = zmk_settings_erase();
    if (rc < 0) {
        LOG_ERR("Failed to erase settings: %d", rc);
    } else {
        shprint(sh, "Done.");
    }

    return rc;
}

static int cmd_layers(const struct shell *sh, const size_t argc, char **argv) {
    for (zmk_keymap_layer_id_t i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        const char *name = zmk_keymap_layer_name(i);
        shprint(sh, "%d: %s", i, name ? name : "(unnamed)");
    }
    return 0;
}

#define BACKUP_CHUNK_SIZE 32
#define RESTORE_BUFFER_SIZE 16384
#define STORAGE_ADDR 0x0006c000
#define STORAGE_SIZE 0x00008000

static uint8_t crc8_checksum(const uint8_t *data, const size_t len);

static struct {
    uint32_t start_addr;
    uint32_t total_size;
    uint32_t current_offset;
    uint8_t buffer[RESTORE_BUFFER_SIZE];
    uint32_t buffer_len;
    bool in_progress;
    const struct device *flash_dev;
    int saved_prio;
} restore_state;

static int flush_restore_buffer(const struct shell *sh) {
    if (restore_state.buffer_len == 0) {
        return 0;
    }

    const uint32_t write_addr = restore_state.start_addr + restore_state.current_offset;
    const uint32_t write_len = restore_state.buffer_len;

    /* Lock the scheduler for the entire erase+write sequence so no other
     * thread can preempt between the two operations and leave flash in an
     * inconsistent state. */
    k_sched_lock();

    struct flash_pages_info info;
    uint32_t erase_addr = write_addr;
    while (erase_addr < write_addr + write_len) {
        int rc = flash_get_page_info_by_offs(restore_state.flash_dev, erase_addr, &info);
        if (rc < 0) {
            k_sched_unlock();
            shprint(sh, "Failed to get flash page info at 0x%08x", erase_addr);
            return rc;
        }
        if (erase_addr == info.start_offset) {
            rc = flash_erase(restore_state.flash_dev, info.start_offset, info.size);
            if (rc < 0) {
                k_sched_unlock();
                shprint(sh, "Failed to erase flash at 0x%08x", (uint32_t) info.start_offset);
                return rc;
            }
        }
        erase_addr = info.start_offset + info.size;
    }

    const int rc = flash_write(restore_state.flash_dev, write_addr, restore_state.buffer, write_len);
    k_sched_unlock();

    if (rc < 0) {
        shprint(sh, "Failed to write flash at 0x%08x", write_addr);
        return rc;
    }

    restore_state.current_offset += write_len;
    restore_state.buffer_len = 0;
    return 0;
}

static int cmd_restore(const struct shell *sh, const size_t argc, char **argv) {
    if (argc < 2) {
        return -EINVAL;
    }

    if (argc >= 3 && strcmp(argv[1], "BACKUP") == 0 && strcmp(argv[2], "START") == 0) {
        const enum zmk_studio_core_lock_state lock_state = zmk_studio_core_get_lock_state();
        if (lock_state == ZMK_STUDIO_CORE_LOCK_STATE_LOCKED) {
            shprint(sh, "Unlock ZMK Studio to allow restoration.");
            return -EPERM;
        }

        if (argc < 5) {
            shprint(sh, "Invalid BACKUP START format: need addr and size");
            return -EINVAL;
        }

        const uint32_t addr = strtoul(argv[3], NULL, 16);
        const uint32_t size = strtoul(argv[4], NULL, 16);

        if (addr != STORAGE_ADDR || size != STORAGE_SIZE) {
            shprint(sh, "Address/size mismatch: expected 0x%08x/0x%08x, got 0x%08x/0x%08x", STORAGE_ADDR, STORAGE_SIZE, addr, size);
            return -EINVAL;
        }

        restore_state.flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
        if (!device_is_ready(restore_state.flash_dev)) {
            shprint(sh, "Flash device not ready");
            return -ENODEV;
        }

        restore_state.start_addr = STORAGE_ADDR;
        restore_state.total_size = STORAGE_SIZE;
        restore_state.current_offset = 0;
        restore_state.buffer_len = 0;
        restore_state.in_progress = true;

        /* Elevate to the highest cooperative priority for the entire restore
         * session so no preemptive thread can starve the restore process. */
        restore_state.saved_prio = k_thread_priority_get(k_current_get());
        k_thread_priority_set(k_current_get(), K_HIGHEST_THREAD_PRIO);

        shprint(sh, "Restore started at 0x%08x, size %u", addr, size);
        return 0;
    }

    if (!restore_state.in_progress) {
        shprint(sh, "Restore not in progress");
        return -EAGAIN;
    }

    if (argc >= 3 && strcmp(argv[1], "BACKUP") == 0 && strcmp(argv[2], "END") == 0) {
        const int rc = flush_restore_buffer(sh);
        k_thread_priority_set(k_current_get(), restore_state.saved_prio);
        restore_state.in_progress = false;
        if (rc < 0) {
            return rc;
        }
        shprint(sh, "Restore complete. Rebooting automatically in 3 seconds.");
        k_sleep(K_MSEC(3000));
        sys_reboot(SYS_REBOOT_COLD);
    }

    const char *line = argv[1];
    static char hexdata[BACKUP_CHUNK_SIZE * 2 + 1] = { 0 };
    char *colon = strchr(line, ':');
    char *hash = strchr(line, '#');

    if (!colon || !hash || colon >= hash) {
        shprint(sh, "Invalid data line format");
        LOG_ERR("Unsuccessful data restoration!");
        LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
        return -EINVAL;
    }

    *colon = '\0';
    *hash = '\0';
    const uint32_t offset = strtoul(line, NULL, 16);
    strncpy(hexdata, colon + 1, sizeof(hexdata) - 1);
    hexdata[sizeof(hexdata) - 1] = '\0';
    const uint32_t crc_val = strtoul(hash + 1, NULL, 16);

    if (offset != restore_state.current_offset + restore_state.buffer_len) {
        shprint(sh, "Offset mismatch: expected %08x, got %08x", 
                restore_state.current_offset + restore_state.buffer_len, offset);
        LOG_ERR("Unsuccessful data restoration!");
        LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
        return -EINVAL;
    }

    static uint8_t chunk[BACKUP_CHUNK_SIZE] = { 0 };
    size_t chunk_len = strlen(hexdata) / 2;
    if (chunk_len > BACKUP_CHUNK_SIZE) {
        shprint(sh, "Chunk too large");
        LOG_ERR("Unsuccessful data restoration!");
        LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
        return -EINVAL;
    }

    for (size_t i = 0; i < chunk_len; i++) {
        const char hi = hexdata[i * 2];
        const char lo = hexdata[i * 2 + 1];
        const uint8_t hi_n = (hi >= 'a') ? (hi - 'a' + 10) : (hi >= 'A') ? (hi - 'A' + 10) : (hi - '0');
        const uint8_t lo_n = (lo >= 'a') ? (lo - 'a' + 10) : (lo >= 'A') ? (lo - 'A' + 10) : (lo - '0');
        chunk[i] = (hi_n << 4) | lo_n;
    }

    if (crc8_checksum(chunk, chunk_len) != (uint8_t)crc_val) {
        shprint(sh, "CRC mismatch at offset %08x", offset);
        LOG_ERR("Unsuccessful data restoration!");
        LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
        return -EBADMSG;
    }

    if (restore_state.buffer_len + chunk_len > RESTORE_BUFFER_SIZE) {
        const int rc = flush_restore_buffer(sh);
        if (rc < 0) {
            LOG_ERR("Unsuccessful data restoration!");
            LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
            return rc;
        }
    }

    memcpy(restore_state.buffer + restore_state.buffer_len, chunk, chunk_len);
    restore_state.buffer_len += chunk_len;

    if (restore_state.buffer_len >= RESTORE_BUFFER_SIZE) {
        const int rc = flush_restore_buffer(sh);
        if (rc < 0) {
            LOG_ERR("Unsuccessful data restoration!");
            LOG_ERR("Try again or execute `board erase` — your device won't boot otherwise!");
        }
        return rc;
    }

    shprint(sh, "Ok. Continue.");
    return 0;
}

static uint8_t crc8_checksum(const uint8_t *data, const size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static bool rgb_supported = false;
static bool rgb_override = false;

static int rgb_settings_set(const char *name, size_t len, const settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "override") == 0) {
        bool val;
        const int rc = read_cb(cb_arg, &val, sizeof(val));
        if (rc >= 0) {
            rgb_override = val;
        }
        return rc;
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(board_rgb, "board/rgb", NULL, rgb_settings_set, NULL, NULL);

static int cmd_check_rgb(const struct shell *sh, const size_t argc, char **argv) {
    if (argc == 1) {
        shprint(sh, "RGB support: %s", rgb_supported ? "yes" : "no");
    } else {
        rgb_override = true;
        rgb_supported = true;
        settings_save_one("board/rgb/override", &rgb_override, sizeof(rgb_override));
        shprint(sh, "RGB support overridden.");
    }

    return 0;
}

/**
 * Dumps NVS storage partition contents in HEX format with checksums.
 *
 * Each 32-byte chunk is followed by a CRC-8 checksum, allowing verification
 * that the data wasn't corrupted by log output or transfer issues.
 *
 * Output format:
 *   BACKUP START ADDR SIZE
 *   OFFSET:HEXBYTES#CHECKSUM
 *   OFFSET:HEXBYTES#CHECKSUM
 *   ...
 *   BACKUP END
 *
 * Example:
 *   BACKUP START 0006c000 00008000
 *   00000000:FF00AABBCC11223344556677889900AABB...#3F
 *   00000020:11223344556677889900AABBCC11223344...#7A
 *   ...
 *   BACKUP END
 */
static int cmd_backup(const struct shell *sh, const size_t argc, char **argv) {
    const enum zmk_studio_core_lock_state lock_state = zmk_studio_core_get_lock_state();
    if (lock_state == ZMK_STUDIO_CORE_LOCK_STATE_LOCKED) {
        shprint(sh, "Unlock ZMK Studio to allow backup.");
        return -EPERM;
    }
    
    const uint32_t storage_addr = STORAGE_ADDR;
    const uint32_t storage_size = STORAGE_SIZE;
    const uint32_t saved_level = log_filter_set(NULL, CONFIG_LOG_DOMAIN_ID, 0, LOG_LEVEL_NONE);

    shprint(sh, "");
    shprint(sh, "BACKUP START %08X %08X", storage_addr, storage_size);

    uint8_t buf[BACKUP_CHUNK_SIZE];
    uint32_t offset = 0;
    while (offset < storage_size) {
        size_t chunk = (offset + BACKUP_CHUNK_SIZE <= storage_size) ? BACKUP_CHUNK_SIZE : (storage_size - offset);
        memcpy(buf, (void *)(storage_addr + offset), chunk);
        const uint8_t chk = crc8_checksum(buf, chunk);
        printf("%08X:", offset);
        for (size_t i = 0; i < chunk; i++) {
            printf("%02X", buf[i]);
        }
        printf("#%02X\n", chk);
        offset += chunk;
    }

    shprint(sh, "BACKUP END");
    shprint(sh, "");

    log_filter_set(NULL, CONFIG_LOG_DOMAIN_ID, 0, saved_level);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_board,
    SHELL_CMD(output, NULL, "Get or set output channel (USB/BLE)", cmd_output),
    SHELL_CMD(reboot, NULL, "Reboot the device", cmd_reboot),
    SHELL_CMD(erase, NULL, "Erase all settings", cmd_erase),
    SHELL_CMD(version, NULL, "Read firmware version", cmd_version),
    SHELL_CMD(layers, NULL, "List all layers", cmd_layers),
    SHELL_CMD(backup, NULL, "Backup NVS partition", cmd_backup),
    SHELL_CMD(restore, NULL, "Restore from backup", cmd_restore),
    SHELL_CMD(rgb, NULL, "Check RGB support", cmd_check_rgb),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(board, &sub_board, "Control the device", NULL);
#endif

static const struct device *p0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));

static int16_t settings_log_source_id = -1;
static uint32_t settings_log_saved_level;

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

static void rgb_hw_check_work_handler(struct k_work *work) {
    settings_load_subtree("board/rgb");

    if (rgb_override) {
        rgb_supported = true;
    } else {
        gpio_pin_configure(p0, 11, GPIO_INPUT | GPIO_PULL_DOWN);
        gpio_pin_configure(p0, 15, GPIO_INPUT | GPIO_PULL_DOWN);

        if (gpio_pin_get(p0, 11) && gpio_pin_get(p0, 15)) {
            zaf_set_rgb_not_supported();
            LOG_WRN("RGB not supported on this hardware!");
        } else {
            rgb_supported = true;
        }

        gpio_pin_configure(p0, 11, GPIO_DISCONNECTED);
        gpio_pin_configure(p0, 15, GPIO_DISCONNECTED);
    }

    if (settings_log_source_id >= 0) {
        log_filter_set(NULL, CONFIG_LOG_DOMAIN_ID, settings_log_source_id, settings_log_saved_level);
        settings_log_source_id = -1;
    }
}

static K_WORK_DELAYABLE_DEFINE(rgb_hw_check_work, rgb_hw_check_work_handler);

static int pinmux_efgtch_trckbl_init(void) {
    pm_device_action_run(uart, PM_DEVICE_ACTION_SUSPEND);
    pm_device_action_run(uart, PM_DEVICE_ACTION_TURN_OFF);

    set_3v3_en(false);
    set_rgb_en(false);
    set_bl_en(false);

    const uint32_t src_cnt = log_src_cnt_get(CONFIG_LOG_DOMAIN_ID);
    for (uint32_t i = 0; i < src_cnt; i++) {
        if (strcmp(log_source_name_get(CONFIG_LOG_DOMAIN_ID, i), "settings") == 0) {
            settings_log_source_id = (int16_t)i;
            settings_log_saved_level = log_filter_set(NULL, CONFIG_LOG_DOMAIN_ID, settings_log_source_id, LOG_LEVEL_NONE);
            break;
        }
    }

    k_work_schedule(&rgb_hw_check_work, K_MSEC(100));
    return 0;
}

SYS_INIT(pinmux_efgtch_trckbl_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
static int usb_conn_chg(const zmk_event_t *eh) {
    if (zmk_usb_get_conn_state() == ZMK_USB_CONN_HID) {
        pm_device_action_run(uart, PM_DEVICE_ACTION_TURN_ON);
        pm_device_action_run(uart, PM_DEVICE_ACTION_RESUME);
    } else {
        pm_device_action_run(uart, PM_DEVICE_ACTION_SUSPEND);
        pm_device_action_run(uart, PM_DEVICE_ACTION_TURN_OFF);
    }

    return 0;
}

ZMK_SUBSCRIPTION(board_root, zmk_usb_conn_state_changed);
ZMK_LISTENER(board_root, usb_conn_chg)
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

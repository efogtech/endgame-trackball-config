# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ZMK firmware configuration for the **Endgame Trackball** - a dual-sensor trackball (PMW3610) with 8 buttons, 2 rotary encoders, and RGB underglow running on an nRF52833 (ARM Cortex-M4).

## Build System

Firmware is built via GitHub Actions (`.github/workflows/build.yml`). Push to `main` triggers a release with the `.uf2` artifact. The build uses the ZMK reusable workflow from `zmkfirmware/zmk@v0.3.0`.

**Version** is defined in `boards/arm/efogtech_trackball_0/efogtech_trackball_0_defconfig`:
```
CONFIG_BOARD_EFOGTECH_0_VER_MAJOR=0
CONFIG_BOARD_EFOGTECH_0_VER_MINOR=3
CONFIG_BOARD_EFOGTECH_0_VER_PATCH=10
```

No local build commands - all builds run in CI. To test changes, push to a branch and check the Actions workflow.

## Architecture

### West Manifest (`config/west.yml`)
Pinned ZMK v0.3.0 plus efogdev modules:
- `zmk-pmw3610-driver` - dual trackball sensor driver
- `zmk-pointer-2s-mixer` - combines two sensors into one pointer
- `zmk-axis-clamper`, `zmk-acceleration-curves` - pointer processing
- `zmk-report-rate-limit` - BLE report rate limiting
- `zmk-ec11-ish-driver` - rotary encoder driver
- `zmk-auto-hold`, `zmk-behavior-follower`, `zmk-trigger-on-boot` - behavior extensions
- `zmk-keymap-shell` - shell commands for keymap

### Key Files

| File | Purpose |
|------|---------|
| `config/efogtech_trackball_0.keymap` | Keymap with 6 layers (Default, Extras, Device, Scroll, Snipe, User) |
| `config/efogtech_trackball_0.conf` | User-level Kconfig overrides |
| `boards/arm/efogtech_trackball_0/efogtech_trackball_0.dts` | Main devicetree - hardware definition, behaviors, physical layout |
| `boards/arm/efogtech_trackball_0/pointer.dtsi` | Dual PMW3610 sensors on SPI0/SPI1, mixer config |
| `boards/arm/efogtech_trackball_0/efogtech_trackball_0_defconfig` | Board Kconfig - version, features, BLE/USB params |
| `boards/arm/efogtech_trackball_0/efogtech_trackball_0.c` | Shell commands: `board output`, `board reboot`, `board erase`, `board version` |

### Hardware Layout

- **MCU**: Nordic nRF52833 (BLE + USB HID)
- **Sensors**: 2x PMW3610 on separate SPI buses, positioned for twist detection
- **Buttons**: 8 buttons via GPIO direct scan
- **Encoders**: 2 rotary encoders (EC11-ish)
- **RGB**: WS2812 strip
- **Power**: Soft-off support with GPIO wakeup

### Pointer Processing Pipeline

```
PMW3610 sensors → zip_2s_mixer (combines/twist) → input processors → HID report
```

The mixer handles:
- Sensor fusion from two trackball sensors
- Twist gesture detection
- Sensitivity/scroll mode switching per layer

Layers modify pointer behavior via `trackball` node in keymap (see `scroll` and `snipe` child nodes).

## Keymap Conventions

- `DECLARE_ENCODERS` macro defines sensor bindings for each layer
- `ltm` / `ltmkp` - hold-tap behaviors for layer+mouse/key
- Macros defined in keymap for RGB toggle (`rgb_tog`, `rgb_off`)

## Shell Interface

When connected via USB, access shell at `endgame$ ` prompt:
- `board version` - firmware version
- `board output [usb|ble]` - get/set output transport
- `board reboot` - reboot device
- `board erase` - factory reset (clears BLE pairings + settings)

## Scripts

### `scripts/gen-keymap-ascii.sh`

Generates ASCII keymap visualization from the `.keymap` file. Updates both:
- `config/efogtech_trackball_0.keymap` - C block comment at end of file
- `README.md` - markdown code block

Run manually: `./scripts/gen-keymap-ascii.sh`

### Neovim Auto-update

Add to your Neovim config for auto-regeneration on save:

```lua
vim.api.nvim_create_autocmd("BufWritePost", {
  pattern = "*efogtech_trackball_0.keymap",
  callback = function()
    local dir = vim.fn.fnamemodify(vim.fn.expand("%:p"), ":h:h")
    vim.fn.jobstart(dir .. "/scripts/gen-keymap-ascii.sh", {
      on_exit = function() vim.cmd("edit") end
    })
  end
})
```

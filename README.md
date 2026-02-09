# Endgame Trackball

ZMK firmware configuration for the Endgame Trackball - a dual-sensor trackball
with 8 buttons, 2 rotary encoders, and RGB underglow.

## Features

- **Dual PMW3610 sensors** - Combined for smooth tracking and twist detection
- **6 layers** - Default, Extras (clipboard), Device (BLE/RGB), Scroll, Snipe, User
- **ZMK Studio** - Real-time keymap editing via USB/BLE
- **Soft-off** - Deep sleep with GPIO wakeup

## Hardware

- MCU: Nordic nRF52833 (BLE 5.0 + USB HID)
- Sensors: 2x PMW3610 optical sensors
- Buttons: 8 programmable buttons
- Encoders: 2 rotary encoders
- RGB: WS2812 underglow

## Keymap

<!-- KEYMAP_ASCII_START -->
```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                        ENDGAME TRACKBALL KEYMAP                               ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║  PHYSICAL LAYOUT:              LEGEND:                                        ║
║           ┌───────┐ ┌───────┐  · = transparent (falls through)                ║
║           │  [0]  │ │  [1]  │  LAYER/key = hold for layer, tap for key        ║
║           └───────┘ └───────┘  ENC = rotary encoder (CW/CCW)                  ║
║   ┌───┐                 ┌───┐                                                 ║
║   │[2]│    ╭───────╮    │[3]│                                                 ║
║   │   │    │   ⬤   │    │   │                                                 ║
║   │[4]│    ╰───────╯    │[5]│                                                 ║
║   └───┘                 └───┘                                                 ║
║           ┌───────┐ ┌───────┐                                                 ║
║           │  [6]  │ │  [7]  │                                                 ║
║           └───────┘ └───────┘                                                 ║
║   ◎E1                    ◎E2                                                  ║
╚═══════════════════════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════════════════════
LAYER 0: Default                                       Pointer: Normal + Accel
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ SNIPE/Enter    │ │ EXTRAS/Esc     │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │MCLK    │                               │Hyper+S  │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │LCLK    │                               │RCLK    │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ SCROLL/MB4     │ │ DEVICE/MB5     │
              └─────────────────┘ └─────────────────┘

    ENC1: Vol+/Vol-                          ENC2: ⌘Tab/⌘⇧Tab

═══════════════════════════════════════════════════════════════════════════════
LAYER 1: Extras                                        Pointer: Normal
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ ⌘C           │ │ ⌘V           │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │⌘X    │                               │⌘Z    │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │⌘Z    │                               │⌘C    │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ ⌘V           │
              └─────────────────┘ └─────────────────┘

    ENC1: Vol+/Vol-                          ENC2: ⌘Tab/⌘⇧Tab

═══════════════════════════════════════════════════════════════════════════════
LAYER 2: Device                                        Pointer: Normal
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ RGB Off        │ │ RGB Eff        │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │BT Clear  │                               │BT Next  │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │RGB Tog  │                               │BT Prev  │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ Studio         │ │ ·             │
              └─────────────────┘ └─────────────────┘

    ENC1: Vol+/Vol-                          ENC2: ⌘Tab/⌘⇧Tab

═══════════════════════════════════════════════════════════════════════════════
LAYER 3: Scroll                                        Pointer: Scroll (1:3)
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ ·             │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │·      │                               │ScrlSens+  │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │RptRate  │                               │ScrlSens-  │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ Sens-          │ │ Sens+          │
              └─────────────────┘ └─────────────────┘

    ENC1: Sens-/Sens+                          ENC2: ScrlSens+/ScrlSens-

═══════════════════════════════════════════════════════════════════════════════
LAYER 4: Snipe                                         Pointer: Snipe (1:4)
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ Power Off      │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │·      │                               │·      │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │·      │                               │·      │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ ·             │
              └─────────────────┘ └─────────────────┘

    ENC1: ←/→                          ENC2: →/←

═══════════════════════════════════════════════════════════════════════════════
LAYER 5: User                                          Pointer: Normal
═══════════════════════════════════════════════════════════════════════════════
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ ·             │
              └─────────────────┘ └─────────────────┘
    ┌────────┐                               ┌────────┐
    │·      │                               │·      │
    └────────┘                               └────────┘
    ┌────────┐                               ┌────────┐
    │·      │                               │·      │
    └────────┘                               └────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ ·             │ │ ·             │
              └─────────────────┘ └─────────────────┘

    ENC1: Vol+/Vol-                          ENC2: ⌘Tab/⌘⇧Tab
```
<!-- KEYMAP_ASCII_END -->

## Building

Firmware is built automatically via GitHub Actions.

1. Push to `main` branch
2. Wait for workflow to complete
3. Download `.uf2` from [Releases](../../releases)

## Flashing

1. Put device in bootloader mode (double-tap reset or hold boot button)
2. Copy `.uf2` file to the mounted drive
3. Device reboots automatically

## Shell Commands

Connect via USB and use serial terminal:

| Command | Description |
|---------|-------------|
| `board version` | Show firmware version |
| `board output [usb\|ble]` | Get/set output transport |
| `board reboot` | Reboot device |
| `board erase` | Factory reset |

## Customization

Edit `config/efogtech_trackball_0.keymap` to customize bindings.

## License

MIT

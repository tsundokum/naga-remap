# naga-remap

Lightweight C daemon that remaps the 12 side buttons on the Razer Naga V2 HyperSpeed mouse to key combos or shell commands on Linux. Zero runtime dependencies, single static binary. Runs as a system service (root) so your user account never needs broad input device access.

## How it works

The Razer Naga V2 HyperSpeed side buttons emit numpad keycodes (`KP1`-`KP9`, `KP0`, `KPMINUS`, `KPPLUS`). This daemon:

1. Finds the mouse's side-button input device via evdev
2. Grabs it exclusively (original numpad presses won't leak through)
3. Reads button events and remaps them to configured key combos or shell commands via uinput
4. Auto-reconnects when the wireless mouse disconnects/reconnects

## Quick start

```bash
# Build
make

# Detect your device (needs root for /dev/input access)
sudo ./naga-remap --detect

# Test with debug output
sudo ./naga-remap -d -c config.def.json

# Install (builds, installs binary + config + systemd service)
./install.sh
```

## Configuration

Edit `/etc/naga-remap/config.json`:

```json
{
  "mappings": [
    {"button": "KEY_KP1", "description": "Copy",     "keys": ["KEY_LEFTCTRL", "KEY_C"]},
    {"button": "KEY_KP2", "description": "Paste",    "keys": ["KEY_LEFTCTRL", "KEY_V"]},
    {"button": "KEY_KP7", "description": "Terminal", "command": "gnome-terminal"}
  ]
}
```

Each mapping has:
- **button**: The numpad keycode the side button emits
- **description**: Human-readable label
- **keys**: Array of keycodes to emit as a combo (modifiers first, target last)
- **command**: Shell command to run (alternative to keys)

### Side button layout (numpad grid)

```
 ┌─────┬─────┬─────┐
 │ KP7 │ KP8 │ KP9 │  (top row)
 ├─────┼─────┼─────┤
 │ KP4 │ KP5 │ KP6 │
 ├─────┼─────┼─────┤
 │ KP1 │ KP2 │ KP3 │
 ├─────┼─────┼─────┤
 │ KP0 │KP-  │ KP+ │  (bottom row)
 └─────┴─────┴─────┘
```

### Available key names

Modifiers: `KEY_LEFTCTRL`, `KEY_RIGHTCTRL`, `KEY_LEFTSHIFT`, `KEY_RIGHTSHIFT`, `KEY_LEFTALT`, `KEY_RIGHTALT`, `KEY_LEFTMETA`, `KEY_RIGHTMETA`

Letters: `KEY_A` through `KEY_Z`

Numbers: `KEY_0` through `KEY_9`

Function keys: `KEY_F1` through `KEY_F12`

Navigation: `KEY_ESC`, `KEY_TAB`, `KEY_ENTER`, `KEY_SPACE`, `KEY_BACKSPACE`, `KEY_DELETE`, `KEY_HOME`, `KEY_END`, `KEY_PAGEUP`, `KEY_PAGEDOWN`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`

Media: `KEY_MUTE`, `KEY_VOLUMEDOWN`, `KEY_VOLUMEUP`, `KEY_PLAYPAUSE`, `KEY_NEXTSONG`, `KEY_PREVIOUSSONG`

## CLI options

```
-c <path>   Config file (default: /etc/naga-remap/config.json)
-d          Debug mode (log all events to stderr)
--detect    Print matching devices and exit
-h          Show help
```

## Manual install

```bash
make
sudo make install
sudo cp config.def.json /etc/naga-remap/config.json
sudo cp naga-remap.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now naga-remap
```

## Service management

```bash
sudo systemctl status naga-remap
sudo systemctl restart naga-remap
sudo journalctl -u naga-remap -f
```

## Requirements

- Linux with evdev/uinput support
- gcc

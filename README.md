# naga-remap

Lightweight Linux daemon that remaps the 12 side buttons on the Razer Naga V2 HyperSpeed to key combos or shell commands.

## How it works

- Reads raw button events from the mouse's side-button input device via the Linux evdev interface
- Grabs the device exclusively so original keycodes don't leak through
- Emits remapped key combos via Linux uinput (virtual keyboard device)
- Works on X11, and should work on Wayland since it operates at the kernel input level
- Runs as a root system service — no GUI tools needed, no OpenRazer dependency
- Single C binary, zero runtime dependencies, ~530 lines of code
- Auto-reconnects when the wireless mouse disconnects/reconnects

## Install

```bash
make && ./install.sh
```

The install script builds the binary, copies it to `/usr/local/bin/`, installs the default config to `/etc/naga-remap/config.json`, sets up a systemd service, and starts it.

## Configuration

Edit `/etc/naga-remap/config.json`:

```json
{
  "mappings": [
    {"button": "KEY_1", "description": "Copy",        "keys": ["KEY_LEFTCTRL", "KEY_LEFTSHIFT", "KEY_C"]},
    {"button": "KEY_2", "description": "Paste",       "keys": ["KEY_LEFTCTRL", "KEY_LEFTSHIFT", "KEY_V"]},
    {"button": "KEY_3", "description": "Cut",          "keys": ["KEY_LEFTCTRL", "KEY_X"]},
    {"button": "KEY_0", "description": "Close window", "keys": ["KEY_LEFTALT", "KEY_F4"]},
    {"button": "KEY_MINUS", "description": "Vol-",     "keys": ["KEY_VOLUMEDOWN"]},
    {"button": "KEY_7", "description": "Terminal",     "command": "gnome-terminal"}
  ]
}
```

Each mapping has:
- **button** — the keycode the side button emits (see grid below)
- **description** — human-readable label (optional, for your reference)
- **keys** — array of keycodes to emit as a combo (modifiers first, target last)
- **command** — shell command to run instead of a key combo

Restart the service after editing: `sudo systemctl restart naga-remap`

### Side button layout

The 12 side buttons emit these keycodes:

```
 ┌───────┬───────┬───────┐
 │ KEY_7 │ KEY_8 │ KEY_9 │  top row
 ├───────┼───────┼───────┤
 │ KEY_4 │ KEY_5 │ KEY_6 │
 ├───────┼───────┼───────┤
 │ KEY_1 │ KEY_2 │ KEY_3 │
 ├───────┼───────┼───────┤
 │ KEY_0 │ KEY_- │ KEY_= │  bottom row
 └───────┴───────┴───────┘
```

Use `sudo ./naga-remap --detect` then `sudo ./naga-remap -d -c config.def.json` to verify the codes on your device.

### Available key names

**Modifiers:** `KEY_LEFTCTRL`, `KEY_RIGHTCTRL`, `KEY_LEFTSHIFT`, `KEY_RIGHTSHIFT`, `KEY_LEFTALT`, `KEY_RIGHTALT`, `KEY_LEFTMETA`, `KEY_RIGHTMETA`

**Letters:** `KEY_A` through `KEY_Z`

**Numbers:** `KEY_0` through `KEY_9`

**Function keys:** `KEY_F1` through `KEY_F12`

**Navigation:** `KEY_ESC`, `KEY_TAB`, `KEY_ENTER`, `KEY_SPACE`, `KEY_BACKSPACE`, `KEY_DELETE`, `KEY_INSERT`, `KEY_HOME`, `KEY_END`, `KEY_PAGEUP`, `KEY_PAGEDOWN`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`

**Media:** `KEY_MUTE`, `KEY_VOLUMEDOWN`, `KEY_VOLUMEUP`, `KEY_PLAYPAUSE`, `KEY_NEXTSONG`, `KEY_PREVIOUSSONG`, `KEY_STOPCD`

**Browser:** `KEY_BACK`, `KEY_FORWARD`

**Numpad:** `KEY_KP0`–`KEY_KP9`, `KEY_KPMINUS`, `KEY_KPPLUS`, `KEY_KPDOT`, `KEY_KPENTER`, `KEY_KPSLASH`, `KEY_KPASTERISK`, `KEY_NUMLOCK`

**Punctuation:** `KEY_MINUS`, `KEY_EQUAL`, `KEY_LEFTBRACE`, `KEY_RIGHTBRACE`, `KEY_BACKSLASH`, `KEY_SEMICOLON`, `KEY_APOSTROPHE`, `KEY_GRAVE`, `KEY_COMMA`, `KEY_DOT`, `KEY_SLASH`, `KEY_CAPSLOCK`

**Misc:** `KEY_PRINT`, `KEY_SCROLLLOCK`, `KEY_PAUSE`, `KEY_COMPOSE`

## Service management

```bash
sudo systemctl status naga-remap     # check status
sudo systemctl restart naga-remap    # restart after config change
sudo systemctl stop naga-remap       # stop
sudo systemctl enable naga-remap     # enable on boot
sudo journalctl -u naga-remap -f     # follow logs
```

## CLI options

```
-c <path>   Config file (default: /etc/naga-remap/config.json)
-d          Debug mode (log all events to stderr)
--detect    Print matching devices and exit
-h          Show help
```

## Requirements

- Linux with evdev/uinput support
- gcc (build only)

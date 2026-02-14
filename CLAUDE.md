# naga-remap — Developer Reference

C daemon that remaps Razer Naga V2 HyperSpeed side button events via evdev→uinput. Runs as a root system service.

## Architecture

### naga-remap.c (~530 lines)

Main daemon. Key functions:

- `find_device()` — scans `/dev/input/event*` for vendor 0x1532 / product 0x00B4 with phys suffix `/input2`
- `detect_devices()` — prints all matching event nodes (used by `--detect`)
- `setup_uinput()` — creates virtual keyboard via `/dev/uinput`, registers all keys from `key_table[]`
- `parse_config()` — reads JSON config, resolves key names to codes via `key_name_to_code()`
- `run_loop()` — grabs evdev device, reads events, dispatches to key combo emission or `exec_command()`
- `exec_command()` — fork/exec with setsid, fire-and-forget (SA_NOCLDWAIT reaps children)
- `setup_signals()` — SIGINT/SIGTERM handler sets `g_running=0`; SIGCHLD with SA_NOCLDWAIT
- `main()` — parses CLI args, loads config, creates uinput device, reconnection loop calling `find_device()` → `run_loop()`

### config.h

- `key_entry_t` lookup table: 113 key name→code mappings
- `config_t` / `key_mapping_t` structs (max 24 mappings, max 8 keys per combo, max 512-char command)
- `key_name_to_code()` / `key_code_to_name()` — linear scan of `key_table[]`

### cJSON.c / cJSON.h

Vendored MIT-licensed JSON parser from [github.com/DaveGamble/cJSON](https://github.com/DaveGamble/cJSON). Not modified.

## Device detection

The Razer Naga V2 HyperSpeed exposes 5 event nodes under `/dev/input/`. The side-button interface is identified by:

- Vendor: `0x1532` (Razer)
- Product: `0x00B4` (Naga V2 HyperSpeed)
- Phys path ending in `/input2`

The `/input2` suffix isolates the side-button interface from the other 4 nodes (main mouse, keyboard shortcuts, etc.).

## Key discovery

The side buttons emit regular number-row keycodes, **not numpad codes**:

```
KEY_7  KEY_8  KEY_9
KEY_4  KEY_5  KEY_6
KEY_1  KEY_2  KEY_3
KEY_0  KEY_MINUS  KEY_EQUAL
```

Discovered via `sudo ./naga-remap --detect` (find the device) then `sudo ./naga-remap -d` (debug mode shows raw event codes). The config.h comment "Razer Naga side buttons emit these" next to the numpad section is stale — the numpad codes are kept in the lookup table as valid mapping targets, not as source buttons.

## uinput setup

`setup_uinput()` creates a virtual keyboard device:

- Registers `EV_KEY`, `EV_SYN`, `EV_REP` event types
- Registers every key in `key_table[]` via `UI_SET_KEYBIT` — this is required for X11/libinput to recognize the virtual device as a proper keyboard
- Each key press/release gets its own `SYN_REPORT`, mimicking real keyboard behavior
- Key combos: press events in order (modifiers first), release in reverse order
- Repeat events (value=2) only repeat the last key (the non-modifier)

## Security model

- Runs as root via a system systemd service (`/etc/systemd/system/naga-remap.service`)
- Root is required for `/dev/input/*` and `/dev/uinput` access
- User accounts never need the `input` group — this avoids giving users broad access to all input devices (keylogger exposure)

## Build & deploy

```bash
make                # build binary
make deploy         # stop service → copy binary → start service
make install        # install binary + default config to prefix
make clean          # remove binary
```

`make deploy` is the fast iteration cycle: stops the service, copies the new binary to `/usr/local/bin/`, restarts. `./install.sh` is the full first-time setup (build + install + config + systemd).

## Config format

JSON file at `/etc/naga-remap/config.json`:

```json
{
  "mappings": [
    {"button": "KEY_1", "description": "Copy", "keys": ["KEY_LEFTCTRL", "KEY_LEFTSHIFT", "KEY_C"]},
    {"button": "KEY_7", "description": "Terminal", "command": "gnome-terminal"}
  ]
}
```

Each entry in `mappings`:
- `button` — source keycode name (must exist in `key_table[]`)
- `description` — optional human label
- `keys` — array of keycode names to emit as a combo (mutually exclusive with `command`)
- `command` — shell command to fork/exec on key-down (mutually exclusive with `keys`)

## Adding new key names

Add an entry to the `key_table[]` array in `config.h`:

```c
{"KEY_NEWNAME", KEY_NEWNAME},
```

The constant (`KEY_NEWNAME`) comes from `<linux/input-event-codes.h>`. After adding, rebuild with `make`.

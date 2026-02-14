#!/bin/bash
set -euo pipefail

CONFIG_DIR="/etc/naga-remap"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Razer Naga V2 Remapper Install ==="
echo
echo "This installs a system-level service running as root."
echo "Your user does NOT need to be in the 'input' group."
echo

# 1. Build
echo "[1/5] Building..."
make -C "$SCRIPT_DIR" clean
make -C "$SCRIPT_DIR"
echo

# 2. Install binary
echo "[2/5] Installing binary to /usr/local/bin/ (requires sudo)..."
sudo install -Dm755 "$SCRIPT_DIR/naga-remap" /usr/local/bin/naga-remap
sudo install -Dm644 "$SCRIPT_DIR/config.def.json" /usr/local/share/naga-remap/config.def.json
echo

# 3. Default config
echo "[3/5] Setting up config in $CONFIG_DIR..."
sudo mkdir -p "$CONFIG_DIR"
if [ -f "$CONFIG_DIR/config.json" ]; then
    echo "  Config already exists at $CONFIG_DIR/config.json (not overwriting)"
else
    sudo cp "$SCRIPT_DIR/config.def.json" "$CONFIG_DIR/config.json"
    sudo chmod 600 "$CONFIG_DIR/config.json"
    echo "  Default config installed to $CONFIG_DIR/config.json"
fi
echo

# 4. Systemd system service
echo "[4/5] Installing systemd service..."
sudo cp "$SCRIPT_DIR/naga-remap.service" /etc/systemd/system/naga-remap.service
sudo systemctl daemon-reload
sudo systemctl enable naga-remap.service
sudo systemctl restart naga-remap.service
echo

# 5. Verify
echo "[5/5] Verifying..."
sleep 1
sudo systemctl status naga-remap.service --no-pager || true
echo

echo "=== Done! ==="
echo
echo "Check status:  sudo systemctl status naga-remap"
echo "View logs:     sudo journalctl -u naga-remap -f"
echo "Edit config:   sudo nano $CONFIG_DIR/config.json"
echo "  (then: sudo systemctl restart naga-remap)"

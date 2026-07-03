# v8 WiFi Management

Full guide for configuring, adding, and removing WiFi networks on the TTGO T-Call v8 device without reflashing firmware.

---

## How It Works

The device maintains a **dynamic list** of saved WiFi networks in LittleFS (`/wifi_nets.json`). On every boot (or reconnect attempt) it tries each network in this order:

1. **Dynamic list** — networks added via dashboard, SMS, or serial (tried in insertion order).
2. **Legacy fixed slots** — `userWifiSsid1/2` from the old dashboard (backward-compat only).
3. **Compile-time secrets** — `wifiSsid/wifiPass` and `wifiSsidBackup/wifiPassBackup` from `secrets.h`.
4. **AP fallback** — starts a hotspot so you can reach the dashboard and add credentials.

If all STA attempts fail at boot, the device enters AP mode and retries every **5 minutes** automatically — no manual reboot needed once the router is reachable.

---

## Method 1 — Web Dashboard (Recommended)

Open `http://<device-ip>:<port>/wifi.html` in a browser while connected to the device (either via STA or by joining the device's AP hotspot).

**Page sections:**
- **Connection** — shows the currently connected SSID or "AP mode" chip.
- **Saved Networks** — lists all stored SSIDs with a **Delete** button per entry.
- **Add Network** — form to enter a new SSID + password; saved immediately to LittleFS.

**Typical flow for moving to a new location:**

1. Join the device's AP hotspot (`TTGO-AP` or the configured AP SSID).
2. Open `http://192.168.4.1:<port>/wifi.html`.
3. Enter the new network's SSID and password → **Add & Save**.
4. Reboot the device (via `/api/reboot` or power-cycle).
5. Device connects to the new network within 15 seconds.

---

## Method 2 — SMS Command

Send an SMS from a number listed in `adminNumbers` or `authenticNumbers`:

```
[newwifi: 'SSID', password: 'pass']
```

The device will:
1. Validate the sender is admin or authentic.
2. Save the network to LittleFS.
3. Confirm via an **ntfy push notification** (free, over data) — it does **NOT** reply by SMS, so no SIM package/balance is needed and no SMS is wasted.
4. **NOT** push the message to Firestore (passwords are never stored in the cloud).
5. Delete the command SMS from SIM memory.

To save **and immediately reboot** in one step:

```
[newwifi: 'SSID', password: 'pass', reboot: true]
```

The device saves the network, replies, then restarts itself.

**Notes:**
- The `password:` key is required even for open networks — use an empty string: `password: ''`.
- Maximum 8 networks in the dynamic list; if the list is full the device replies with an error.
- Existing network with the same SSID is **updated** (password replaced).

---

## Method 3 — Serial Commands

Connect via serial monitor at 115200 baud:

| Command | Effect |
|---------|--------|
| `wifi list` | List all saved dynamic networks (SSIDs only, no passwords) |
| `wifi add <ssid> <pass>` | Add or update a network |
| `wifi delete <ssid>` | Remove a network (case-insensitive match) |
| `wifi clear` | Remove all saved dynamic networks |

**Note:** SSIDs containing spaces cannot be added via serial — use the dashboard or SMS for those.

---

## RTDB Reporting

When the runtime-settings sync runs (on startup, every 10 min, and on manual sync), the device writes the currently connected SSID to:

```
/ttgo_tcall/settings/runtime/connectedSsid
```

This field is absent when the device is in AP or OFFLINE mode.

---

## Boot Retry Behaviour

| Scenario | Behaviour |
|---|---|
| All saved networks fail at boot | AP mode started, retries all networks every 5 min |
| Network added via dashboard while in AP mode | Applied on next reboot or 5-min retry |
| Router goes away during runtime | `WiFi.status()` drops; 5-min retry reconnects automatically |
| `[newwifi: ..., reboot: true]` SMS | Saved then device restarts → connects on boot |

---

## Storage Details

Networks are stored in `/wifi_nets.json` on LittleFS as a JSON array:

```json
[
  {"ssid": "HomeRouter",   "pass": "secretpass"},
  {"ssid": "OfficeWifi",   "pass": "officepass"}
]
```

- Maximum **8** entries.
- If the same SSID is added twice, the password is updated (no duplicate entries).
- Deleting all entries removes the file entirely.
- The file survives firmware updates as long as LittleFS is not reformatted.

---

## Legacy WiFi Slots

The old two-slot system (`userWifiSsid1`, `userWifiSsid2`) still works as a fallback — entries in `/v8_config.json` are tried after the dynamic list. No migration is needed; new networks added via the dynamic list automatically take priority.

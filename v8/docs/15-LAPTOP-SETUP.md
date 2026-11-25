# v8 Laptop Setup and Device Flash

This file records the setup that was verified on this Linux laptop on 2026-11-25.
Use it before changing firmware or reflashing the TTGO T-Call. Only `v8/` is active.
Real credentials belong only in the gitignored `v8/sms_calls/secrets.h`.

---

## Verified environment

- Working directory: `/home/megatron/projects/TTGO_TCall`
- `arduino-cli` 1.5.1 at `/home/megatron/.local/bin/arduino-cli`
- ESP32 platform 3.3.8
- Arduino IDE 2.3.11:
  - Application: `/home/megatron/.local/share/arduino-ide_2.3.11_Linux_64bit/arduino-ide`
  - Launcher: `/home/megatron/.local/bin/arduino-ide`
  - Desktop entry: `/home/megatron/.local/share/applications/arduino-ide.desktop`
- Installed libraries:
  - ArduinoJson 7.4.3
  - Firebase Arduino Client Library for ESP8266 and ESP32 4.4.17
  - DHT sensor library 1.4.7
  - Adafruit SSD1306 2.5.17
  - Adafruit GFX Library 2.0.0
  - Adafruit BusIO 1.17.4
  - Adafruit Unified Sensor 1.1.15

The board appears as `/dev/ttyACM0`. The stable device path is:

```text
/dev/serial/by-id/usb-1a86_USB_Single_Serial_54F4012031-if00
```

Use the stable path in scripts. The serial id is `54F4012031`. The user is in the
`dialout` group, so no `sudo` is needed for the port.

---

## Board options

Use these options for the production board:

```text
JTAGAdapter=default
PSRAM=disabled
PartitionScheme=huge_app
CPUFreq=240
FlashMode=dio
FlashFreq=80
FlashSize=4M
UploadSpeed=921600
LoopCore=1
EventsCore=1
DebugLevel=none
EraseFlash=none
```

`FlashMode=dio` is required. Forcing quad input and output makes this board's
bootloader read corrupt segment addresses and reset through the watchdog. Keep
Huge APP, 4 MB flash, 80 MHz flash frequency, and DIO flash mode.

The Windows `COM13` and `uploaded_code` sketch in `.vscode/arduino.json` are from
another machine. Do not use them on this laptop.

---

## One-time toolchain setup

```sh
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.8
arduino-cli lib install ArduinoJson
arduino-cli lib install "Firebase Arduino Client Library for ESP8266 and ESP32"
arduino-cli lib install "DHT sensor library"
arduino-cli lib install "Adafruit SSD1306"
```

The Firebase library name above contains spaces. The older underscore-only name
in earlier notes is not accepted by the current library index.

---

## Compile and upload

Run from the repository root:

```sh
REPO=/home/megatron/projects/TTGO_TCall
PORT=/dev/serial/by-id/usb-1a86_USB_Single_Serial_54F4012031-if00
OPTIONS="JTAGAdapter=default,PSRAM=disabled,PartitionScheme=huge_app,CPUFreq=240,FlashMode=dio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,LoopCore=1,EventsCore=1,DebugLevel=none,EraseFlash=none"

cd "$REPO"
arduino-cli compile --fqbn esp32:esp32:esp32 --board-options "$OPTIONS" v8/sms_calls
arduino-cli compile --fqbn esp32:esp32:esp32 --board-options "$OPTIONS" --upload --port "$PORT" v8/sms_calls
```

On 2026-11-25 the sketch compiled to 1315519 bytes, which is 41 percent of the
3145728 byte application partition.

---

## LittleFS dashboard upload

Older notes call this "SPIFFS upload". The active board uses LittleFS. The web
files live in `v8/sms_calls/data` and are flashed separately from the sketch so
`/dashboard.html`, `/dashboard.css`, `/dashboard.js`, `/wifi.html` and
`/version.txt` are served by the device.

Any change under `v8/sms_calls/data` requires a new version and date in
`v8/sms_calls/data/version.txt`. The dashboard shows that file in its header and
About tab. The current value is `v8.6.2`, dated 2026-11-25.

The board was measured on 2026-11-25 as:

```text
app0     app  ota_0   0x10000   3M
spiffs   data spiffs  0x310000  896K
coredump data         0x3f0000  64K
```

Do not hardcode an offset from older documentation. Read the partition table
from the board every time:

```sh
ESPTOOL=/home/megatron/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
MKLITTLEFS=/home/megatron/.arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs
CORE=/home/megatron/.arduino15/packages/esp32/hardware/esp32/3.3.8
PORT=/dev/serial/by-id/usb-1a86_USB_Single_Serial_54F4012031-if00

$ESPTOOL --port "$PORT" --baud 921600 read-flash 0x8000 0xc00 /tmp/parttable.bin
python3 "$CORE/tools/gen_esp32part.py" /tmp/parttable.bin
```

Back up the existing file system before writing anything. The backup also holds
`/v8_config.json` and `/wifi_nets.json`, and writing the image erases them:

```sh
$ESPTOOL --port "$PORT" --baud 921600 read-flash 0x310000 0xE0000 /tmp/fs-backup.bin
$MKLITTLEFS -l /tmp/fs-backup.bin
$MKLITTLEFS -u /tmp/old-fs /tmp/fs-backup.bin
```

Build a staging copy from `v8/sms_calls/data`, then copy `v8_config.json` and
`wifi_nets.json` from the unpacked backup into it. If the saved wireless
networks must change, update `secrets.h` first and update those two files from
it. Do not write passwords into a document or into any tracked file.

```sh
$MKLITTLEFS -c /tmp/littlefs-stage -b 4096 -p 256 -s 917504 /tmp/littlefs.bin
$ESPTOOL --port "$PORT" --baud 921600 write-flash 0x310000 /tmp/littlefs.bin
$ESPTOOL --port "$PORT" --baud 921600 read-flash 0x310000 0xE0000 /tmp/littlefs-readback.bin
$MKLITTLEFS -l /tmp/littlefs-readback.bin
```

Close the serial monitor before running `esptool`. If it reports
`Wrong boot mode detected (0x13)`, hold `BOOT` while tapping `EN/RESET`, then
retry.

---

## Wireless network state

The compile-time network defaults live only in `v8/sms_calls/secrets.h`. The
board also keeps a saved dynamic list in `/wifi_nets.json` and legacy fields in
`/v8_config.json`. Those saved values take priority over the compile-time
defaults, so editing `secrets.h` alone is not enough on an already configured
board.

Current device state:

- First saved network: the requested primary network from `secrets.h`.
- Second saved network: the former primary network, now the backup.
- Boot verification: `[WIFI] connected via saved[0]`.
- Cloud verification: `[FIREBASE] Firebase manager ready`.

See `v8/docs/13-WIFI-MANAGEMENT.md` for dashboard, short message service, and
serial ways to change networks later.

---

## Verification checklist

This project has no automated test suite. Verification means compiling,
flashing, reading the board back, and watching the serial output.

- Sketch compile and upload both exit with code 0.
- LittleFS readback lists `version.txt`, `v8_config.json` and `wifi_nets.json`.
- The readback `version.txt` contains `v8.6.2` and `2026-11-25`.
- Serial boot shows `connected via saved[0]` for the primary network.
- Serial boot shows the Firebase manager is ready.
- The dashboard opens at `http://<device-ip>:<webServerPort>/dashboard.html`
  and shows LittleFS `v8.6.2`.

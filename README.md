# ZX Spectrum for the Cheap Yellow Display

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](./LICENSE)

Firmware that turns a [Cheap Yellow Display (CYD)](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) ESP32-2432S028 into a 48K ZX Spectrum with an on-screen touch keyboard.

This project is a CYD-focused port of [atomic14's ESP32 ZX Spectrum emulator](https://github.com/atomic14/esp32-zxspectrum). The upstream tree also contains desktop and web builds; **this repository is maintained for the CYD board**.

## Features

- Boots straight into the **48K ZX Spectrum** (no model picker)
- **Resistive touch keyboard** with left- or right-handed layout
- Spectrum display with borders above the keyboard row
- **In-emulator menu** (Menu key or touch **Menu**): recalibrate, volume, snapshots, load games, poke, exit
- **SD card** support for `.z80`, `.sna`, `.tap`, and `.tzx` files
- Settings and touch calibration stored in LittleFS (`settings.json`), preserved across firmware uploads

## The Innovation

The **touch-screen keyboard** stays permanently available under the Spectrum display, with **R1–R4** buttons that switch between rows of the original Spectrum keyboard. It’s surprisingly usable, about as good as the original rubber keyboard, and it has the same spirit to it.

Left-handed and right-handed layouts:

![Touch keyboard (left-handed)](assets/spec-001.jpg)
![Touch keyboard (right-handed)](assets/spec-002.jpg)

Loading a game from tape (`.tap` / `.tzx`) includes a progress bar. It runs at about **3× real speed** which is nostalgic, but not *that* nostalgic.

![The Lords of Midnight loading](assets/spec-003.jpg)

Once loaded, you can navigate through the game as easily as with a real Spectrum:

![The Lords of Midnight gameplay 1](assets/spec-004.jpg)
![The Lords of Midnight gameplay 2](assets/spec-005.jpg)
![The Lords of Midnight gameplay 3](assets/spec-006.jpg)

Screenshots demonstrate the emulator using the best game ever written for the ZX Spectrum, namely *The Lords of Midnight* © 1984 Beyond Software / Mike Singleton.

**Demo: Sinclair BASIC programming**

[![▶ Click here to watch on YouTube](https://img.youtube.com/vi/5wpk6Go0m5Q/hqdefault.jpg)](https://youtu.be/5wpk6Go0m5Q)

[▶ Watch on YouTube](https://youtu.be/5wpk6Go0m5Q).

I also plan a feature inspired by the old cardboard keyboard overlays: if there’s a matching <file>.KEY alongside a tape image (for example game.tzx + game.key), the on‑screen keyboard will temporarily relabel the keys with game-specific actions to act as a live overlay/cheat sheet.

## Hardware

Target board: **ESP32-2432S028** (320×240 ILI9341 TFT, XPT2046 touch, microSD on SPI).

| Function | GPIO |
|----------|------|
| TFT SPI | 14 SCLK, 12 MISO, 13 MOSI, 15 CS, 2 DC, 21 BL |
| Touch (bit-bang) | 33 CS, 36 IRQ, 25 CLK, 39 MISO, 32 MOSI |
| SD card | VSPI: CS 5, CLK 18, MISO 19, MOSI 23 (separate from TFT HSPI) |
| Buzzer (PWM) | 26 |

Other ESP32 boards from the upstream project may still build from `firmware/platformio.ini`, but **only `cheap-yellow-display` is the focus here**.

## Install firmware in your browser

**[Install CYD ZX Spectrum firmware](https://kf106.github.io/cyd-zxspectrum/)**

Use **Chrome** or **Edge** on a computer. Plug the CYD in with USB, open the link, click **Install firmware**, pick the serial port, and wait. On Ubuntu the port is usually **`/dev/ttyUSB0`**.

On first boot: touch calibration, then left- or right-handed keyboard.

## Building and flashing (Ubuntu)

### Prerequisites

Install build tools and USB serial access:

```sh
sudo apt update
sudo apt install git python3 python3-venv python3-pip
```

Add your user to the `dialout` group so PlatformIO can access the CYD over USB (log out and back in, or reboot, after this):

```sh
sudo usermod -aG dialout $USER
```

### One-time setup

Clone this repository and create a local PlatformIO virtual environment in the repo root:

```sh
cd cyd-zxspectrum
python3 -m venv .pio-venv
.pio-venv/bin/pip install -U pip platformio
```

The first build downloads the ESP32 toolchain and libraries; it can take several minutes.

### Build, flash, and monitor

From the repository root:

```sh
# Build
.pio-venv/bin/pio run -e cheap-yellow-display --project-dir firmware

# Flash (CYD connected by USB; on Ubuntu usually /dev/ttyUSB0, sometimes /dev/ttyACM0)
.pio-venv/bin/pio run -e cheap-yellow-display --project-dir firmware -t upload

# Serial monitor (Ctrl+C to exit)
.pio-venv/bin/pio device monitor --project-dir firmware -b 115200
```

If upload fails to find the port, list devices and pass the port explicitly:

```sh
.pio-venv/bin/pio device list
.pio-venv/bin/pio run -e cheap-yellow-display --project-dir firmware -t upload --upload-port /dev/ttyUSB0
```

To erase flash and settings (forces touch calibration on next boot):

```sh
.pio-venv/bin/pio run -e cheap-yellow-display --project-dir firmware -t erase
.pio-venv/bin/pio run -e cheap-yellow-display --project-dir firmware -t upload
```

### Optional: VS Code

According to Atomic14 you can also open the `firmware` folder in [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension and use the **cheap-yellow-display** environment from the PlatformIO sidebar, although I haven't tested this.

## First boot

On first boot the firmware:

1. Runs **four-corner touch calibration**
2. Asks for **left- or right-handed** keyboard layout

Calibration and settings are written to LittleFS and **are not erased** when you upload new firmware.

To calibrate again:

- Use **Erase Flash** in PlatformIO (`pio run -t erase`, then upload), or
- Open the in-emulator **Menu → Recalibrate**, or
- Set `"cydSetupComplete": false` in `settings.json` on the device

## Custom keyboard images (optional)

Built-in key art is compiled into the firmware from PNGs in [`assets/keyboard/`](assets/keyboard/).

### Per-game keyboard (`.tap` / `.tzx` + matching `.kbd`)

When you load a tape from the **SD card**, the firmware looks for a keyboard pack with the **same basename**:

| Tape on SD | Keyboard pack (same folder) |
|------------|-----------------------------|
| `/sdcard/lom.tzx` | `/sdcard/lom.kbd` |
| `/sdcard/games/foo.tap` | `/sdcard/games/foo.kbd` |

Build the pack from a complete keyboard folder (**47** required PNGs: `0.png`–`9.png`, `a.png`–`z.png`, `enter.png`, `del.png`, `caps.png`, `sym.png`, `menu.png`, `r1.png`–`r4.png`, `space-left.png`, `space-right.png`; `blank.png` is optional). The converter exits with an error listing any missing required files.

```sh
python3 -m pip install pillow   # once
# Game pack only — does not change built-in firmware key art:
python3 utils/png_converter.py utils/lom-keyboard --batch \
  --pack-only --pack /path/to/sdcard/LOM.kbd
```

Do **not** run `utils/lom-keyboard --batch` without `--pack-only` unless you intend to replace the default keys compiled into the firmware (that writes into `firmware/src/Screens/images/keyboard/`).

Copy `lom.kbd` next to `lom.tzx` on the card. After the tape finishes loading, the pack is read into the same **96 KiB workspace** used for tape loading (no extra heap for the keyboard). Serial log: `Keyboard pack loaded into workspace`.

**Tap the Spectrum screen** to switch between game art and built-in keys — only when a pack was loaded for the current game. Loading another tape resets to built-in first, then loads the new game’s pack if present. Pack size must fit in 96 KiB (~47 keys at 32×26, plus optional `blank`).

## Games and storage

**SD card (recommended):** format as **FAT32** (not exFAT; 64 GB cards work if formatted FAT32), copy `.z80`, `.sna`, `.tap`, or `.tzx` files to the card root (or use **Menu → Load game / tape** to browse).

**Without SD:** put files in `firmware/data/` and upload the filesystem (`pio run -t uploadfs`).

Snapshots are saved under `/snapshots` on the SD card when present.

## Optional: USB serial keyboard

The `keyboard-server/` directory has Python tooling to send keystrokes over USB serial (useful on a desk with the CYD connected). Atomic14 has tested it on macOS. I haven't tried it yet.

```sh
cd keyboard-server
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python serial_keyboard.py
```

## Repository layout

| Path | Purpose |
|------|---------|
| `firmware/` | ESP32 firmware (PlatformIO) |
| `docs/flasher/` | Browser installer page (published to GitHub Pages on release) |
| `keyboard-server/` | Host-side serial keyboard helper |
| `desktop/` | Desktop builds (upstream) |
| `docs/` | Screenshots and web flasher sources |
| `assets/` | Images for the keys and this page |
| `utils/` | PNG → firmware C++ and SD `.kbd` converter |


## License

GPL v3 — see [LICENSE](./LICENSE). Emulator core and much of the firmware derive from atomic14's [esp32-zxspectrum](https://github.com/atomic14/esp32-zxspectrum).

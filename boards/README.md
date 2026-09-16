```
.
├── platformio.ini
├── boards
    ├── _jsonfiles
    │   ├── [mcu].json          (esp32, esp32s3, esp32c3, esp32c5, esp32c6, esp32p4, esp32p4_es, esp32s2)
    │   ├── [mcu].h             (generic pin defaults for that chip family)
    │   ├── pins_arduino.h      (dispatcher: includes the right [mcu].h for the chip being built)
    │   └── variant.cpp
    ├── [board]
    │   ├── interface.cpp
    │   ├── [board].ini
    │   └── connections.md      (optional)

...
```

# Adding a new board

Porting a new device boils down to 3 files under `boards/<board>/`:

- `[board].ini`
- `interface.cpp`
- `connections.md` (optional)

There is no per-board JSON or `pins_arduino.h` anymore. `boards/_jsonfiles/` holds one generic
PlatformIO board JSON and one generic `pins_arduino.h` per MCU family (`esp32`, `esp32s3`, `esp32c5`,
etc.) — every board just points at the JSON for its chip and overrides only what's different for that
specific device.

# Files
(Replace \[board] with the board name)

## boards/\[board]/\[board].ini
This is the PlatformIO config for the device. Look at other boards for what's needed. In particular:

- `board = <mcu>` — the chip family, matching one of the JSON files in `boards/_jsonfiles/` (e.g.
  `board = esp32s3`), **not** a per-board name.
- `board_build.*` / `board_upload.*` — set these directly in the `.ini` for anything that differs from
  the generic JSON for that MCU: flash size, PSRAM memory type, partitions, upload speed, etc. For
  example:
  ```ini
  board_build.arduino.memory_type = qio_qspi ; qio_opi when PSRAM is octal
  board_build.flash_mode = qio
  board_build.f_flash = 80000000L
  board_upload.flash_size = 8MB
  board_upload.maximum_size = 8388608
  board_upload.speed = 921600
  ```
- `build_flags` — every pin and feature define for the board goes here as `-D`, grouped with short `;`
  comments (see `boards/ESP-General/ESP-General.ini` for a fully worked example: TFT pins, SD card pins,
  CC1101/NRF24/W5500 pins, I2C pins, etc. are all plain `-D` entries, not a header file).

**Important**: avoid overriding the plain Arduino pin names that come from `boards/_jsonfiles/[mcu].h`
(`SDA`, `SCL`, `SS`, `TX`, `RX`, `MOSI`, `MISO`, `SCK`, `A0`-`A19`, `LED_BUILTIN`, etc.) via `-D`. Those
stay as ordinary `static const uint8_t` variables in the generic header on purpose — turning them into
compiler macros makes the substitution global and can silently break unrelated third-party library code
that happens to use the same short names as local variables (this actually broke a build during the
2026 boards/ refactor). If your board's real
wiring for one of those pins differs from the MCU's generic default, give it its own Bruce-specific name
instead (`SYS_I2C_SDA`, `SPI_SS_PIN`, etc. — these are all safe as `-D` since they're specific enough not
to collide with anything) and use that name explicitly wherever the board needs the real pin (typically in
`interface.cpp`'s `_setup_gpio()`, e.g. `Wire.begin(SYS_I2C_SDA, SYS_I2C_SCL)`).

## boards/\[board]/interface.cpp
This is where you do the board-specific setup code (`_setup_gpio()`, etc.).

## boards/\[board]/connections.md (optional)
Wiring notes / pinout documentation for the board.

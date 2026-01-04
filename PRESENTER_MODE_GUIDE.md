# Bruce Firmware - Presenter Mode Addition Guide

This document summarizes the changes made to add a **Presenter Mode** to the Bruce firmware for the LilyGo T-Embed ESP32-S3, and how to build and flash the firmware.

---

## Overview

**Presenter Mode** is a Bluetooth HID feature that turns the T-Embed into a wireless presentation remote. It connects to a computer via Bluetooth and sends keyboard commands to control presentations.

### Controls

| T-Embed Input             | Keyboard Output | Use Case                |
| ------------------------- | --------------- | ----------------------- |
| **Middle Button (Press)** | `Enter`         | Advance slide / Confirm |
| **Rotary Wheel Right**    | `Right Arrow →` | Next slide              |
| **Rotary Wheel Left**     | `Left Arrow ←`  | Previous slide          |
| **Escape Button**         | Exit            | Exit Presenter Mode     |

### Display

Shows "PRESENTER" in large centered text while active.

---

## Code Changes Made

### 1. Header File: `src/modules/badusb_ble/ducky_typer.h`

Added function declaration for PresenterMode:

```cpp
// Presenter mode - press button to advance slides
void PresenterMode(HIDInterface *&hid, bool ble = true);
```

**Location:** Added after the `MediaCommands` declaration, before `#endif`

### 2. Source File: `src/modules/badusb_ble/ducky_typer.cpp`

Added the `PresenterMode` function implementation at the end of the file (after `MediaCommands` function):

```cpp
// Presenter mode - simple button press to advance slides
void PresenterMode(HIDInterface *&hid, bool ble) {
    if (_Ask_for_restart == 2) {
        displayError("Restart your Device");
        delay(1000);
        return;
    }
    _Ask_for_restart = 1; // arm the flag

    ducky_startKb(hid, ble);

    displayTextLine("Pairing...");

    while (!hid->isConnected() && !check(EscPress));

    if (!hid->isConnected()) {
        displayWarning("Canceled", true);
        returnToMenu = true;
        return;
    }

    BLEConnected = true;

    // Draw simple presenter screen
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextSize(3);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawCentreString("PRESENTER", tftWidth / 2, tftHeight / 2 - 15, 1);

    while (1) {
        // Middle button = Enter
        if (check(SelPress)) {
            hid->press(KEY_RETURN);
            hid->releaseAll();
            delay(150); // debounce
        }

        // Wheel right = Right arrow (next slide)
        if (check(NextPress)) {
            hid->press(KEY_RIGHT_ARROW);
            hid->releaseAll();
            delay(150); // debounce
        }

        // Wheel left = Left arrow (previous slide)
        if (check(PrevPress)) {
            hid->press(KEY_LEFT_ARROW);
            hid->releaseAll();
            delay(150); // debounce
        }

        // Escape to exit
        if (check(EscPress)) break;

        delay(10);
    }

    hid->releaseAll();
    returnToMenu = true;
}
```

### 3. Menu File: `src/core/menu_items/BleMenu.cpp`

Added Presenter option to the BLE menu:

```cpp
options.push_back({"Media Cmds", [=]() { MediaCommands(hid_ble, true); }});
options.push_back({"Presenter", [=]() { PresenterMode(hid_ble, true); }});  // <-- Added this line
```

**Location:** In `BleMenu::optionsMenu()` function, right after the "Media Cmds" option.

---

## Build & Flash Instructions

### Prerequisites

1. **Python 3.x** installed
2. **PlatformIO** installed via pip:
   ```powershell
   pip install platformio
   ```
3. **T-Embed connected** via USB
4. **Close any serial monitors** or programs using the COM port

### Build Commands (Windows PowerShell)

```powershell
# Navigate to firmware directory
cd D:\Programing\Projects\Bruce_firmware\firmware

# Add PlatformIO to PATH and build + upload
$env:Path += ";$env:USERPROFILE\.platformio\penv\Scripts"
python -m platformio run -e lilygo-t-embed -t upload
```

### Available Build Environments

For different T-Embed variants, use the appropriate environment:

- `lilygo-t-embed` - Standard T-Embed
- `lilygo-t-embed-cc1101` - T-Embed with CC1101 RF module

### Build Only (without upload)

```powershell
python -m platformio run -e lilygo-t-embed
```

Output binary will be at: `Bruce-lilygo-t-embed.bin`

### Manual Flash

If auto-upload fails, you can manually flash using:

1. **Web Flasher:** Use the Bruce web flasher at https://bruce.computer
2. **esptool:**
   ```powershell
   esptool.py --chip esp32s3 --port COM3 write_flash 0x0 Bruce-lilygo-t-embed.bin
   ```

---

## Troubleshooting

### COM Port Busy Error

```
Could not open COM3, the port is busy or doesn't exist.
```

**Solution:**

- Close Arduino IDE, serial monitors, or any program using the port
- Try a different USB cable
- Put device in bootloader mode (hold BOOT while connecting USB)

### GitHub 503 Errors During Build

```
PackageException: Got the unrecognized status code '503'
```

**Solution:** Simply retry the build command - GitHub servers may be temporarily overloaded.

### Missing Package Manifest Error

```
MissingPackageManifestError: Could not find one of 'package.json'
```

**Solution:** Clean corrupted packages:

```powershell
Remove-Item -Recurse -Force "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32*"
python -m platformio run -e lilygo-t-embed -t upload
```

### Windows Long Path Warning

Enable long paths in Windows:

```powershell
# Run as Administrator
New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name 'LongPathsEnabled' -Value 1 -PropertyType DWORD -Force
```

---

## Key Files Reference

| File                                     | Purpose                                      |
| ---------------------------------------- | -------------------------------------------- |
| `platformio.ini`                         | Build configuration, environment definitions |
| `src/modules/badusb_ble/ducky_typer.cpp` | BLE keyboard/BadUSB implementation           |
| `src/modules/badusb_ble/ducky_typer.h`   | Header with function declarations            |
| `src/core/menu_items/BleMenu.cpp`        | Bluetooth menu structure                     |
| `lib/Bad_Usb_Lib/BleKeyboard.h`          | BLE HID keyboard library                     |

---

## Input Detection Reference

The T-Embed uses these input checks (defined in `core/mykeyboard.h`):

- `check(SelPress)` - Middle/Select button pressed
- `check(NextPress)` - Rotary wheel turned right
- `check(PrevPress)` - Rotary wheel turned left
- `check(EscPress)` - Escape/back button pressed

---

## HID Key Codes Reference

Common key codes used (from `USBHIDKeyboard.h`):

- `KEY_RETURN` - Enter key
- `KEY_RIGHT_ARROW` - Right arrow
- `KEY_LEFT_ARROW` - Left arrow
- `KEY_UP_ARROW` - Up arrow
- `KEY_DOWN_ARROW` - Down arrow
- `KEY_PAGE_DOWN` - Page Down
- `KEY_PAGE_UP` - Page Up
- `KEY_SPACE` - Space bar

---

## Summary

To add similar BLE HID features to Bruce firmware:

1. **Add function declaration** in `ducky_typer.h`
2. **Implement function** in `ducky_typer.cpp` using:
   - `ducky_startKb(hid, ble)` to initialize BLE keyboard
   - `hid->press(KEY_CODE)` to send key press
   - `hid->releaseAll()` to release keys
   - `check(InputType)` to detect button presses
3. **Add menu entry** in appropriate menu file (e.g., `BleMenu.cpp`)
4. **Build and flash** using PlatformIO

---

_Document created: December 11, 2025_
_Firmware: Bruce for LilyGo T-Embed ESP32-S3_

# M5Stack CoreInk - Bruce Firmware

This directory contains the board-specific configuration and drivers for the M5Stack CoreInk e-ink device.

## Device Overview

The M5Stack CoreInk is an ESP32-based development kit featuring:
- **Display**: 200x200 pixel e-ink display (1.54" diagonal)
- **CPU**: ESP32-PICO-D4 (dual-core, 240MHz)
- **Memory**: 4MB Flash
- **Battery**: Built-in 390mAh rechargeable lithium battery with power management
- **Input**: 3-button rocker switch
- **Connectivity**: Grove I2C port (Pin 21 SDA, Pin 22 SCL)
- **LED**: Power status LED with programmable control
- **Size**: Compact 56mm × 40mm × 16mm form factor

## Bruce Firmware Adaptations

### E-Ink Display Optimizations

The e-ink display requires special handling compared to traditional LCD/OLED displays:

1. **Refresh Rate Management**
   - Configurable auto-refresh intervals: Manual, 15s, 30s, 60s, or 5 minutes
   - Intelligent flush system prevents excessive refreshes
   - Menu updates use 300ms minimum interval for responsiveness

2. **Visual Rendering**
   - Forced black & white color scheme (no grayscale)
   - Disabled text scrolling animations
   - Simplified clock display (HH:MM only)
   - No backlight control (not applicable to e-ink)

3. **Power Management**
   - Screen timeout features disabled
   - Display sleep mode bypassed
   - LED feedback for button presses (immediate user feedback)
   - Optimized power consumption for extended battery life

### Input System

The rocker switch provides navigation:
- **Left/Right**: Previous/Next menu item (configurable inversion)
- **Center (short press)**: Select/Confirm
- **Center (long press, 800ms+)**: Back/Escape
- **LED Pulse**: 35ms flash on any button press for tactile feedback

### Pin Configuration

| Function | Pin | Notes |
|----------|-----|-------|
| Grove SDA | 21 | I2C Data (external modules) |
| Grove SCL | 22 | I2C Clock (external modules) |
| Rocker Left | 39 | Navigation input |
| Rocker Center | 38 | Select/Back input |
| Rocker Right | 37 | Navigation input |
| BadUSB TX | 21 | Via Grove SDA |
| BadUSB RX | 22 | Via Grove SCL |

### External Module Support

Via the Grove I2C connector, the CoreInk supports:
- **CC1101**: Sub-GHz RF transceiver
- **NRF24**: 2.4GHz wireless module
- **PN532**: RFID/NFC reader
- **FM Radio**: RDA5807M or similar I2C modules
- **SD Card**: Via I2C or external SPI adapter

## Files in This Directory

- **`m5stack-coreink.ini`**: PlatformIO build configuration with compiler flags and pin definitions
- **`interface.cpp`**: Hardware abstraction layer for buttons, battery, LED, and power management
- **`pins_arduino.h`**: Arduino pin definitions for the CoreInk board
- **`readme.md`**: This file

## Building for CoreInk

1. Edit `platformio.ini` in the project root
2. Set `default_envs = m5stack-coreink`
3. Build: `pio run -e m5stack-coreink`
4. Flash: `pio run -e m5stack-coreink -t upload`

Or use the web flasher at https://bruce.computer/flasher

## Configuration Notes

- No onboard SD card slot (use external adapter)
- Default rotation: 3 (270 degrees)
- 4MB partition scheme with full LittleFS support
- USB CDC for serial communication

## Known Limitations

- No built-in microphone
- No RGB LED strip support
- No onboard speaker (tone generation not available)
- E-ink ghosting may occur with frequent refreshes (mitigated by configurable intervals)

## Additional Resources

- [M5Stack CoreInk Product Page](https://shop.m5stack.com/products/m5stack-esp32-core-ink-development-kit1-54-elnk-display)
- [Bruce Wiki](https://github.com/pr3y/Bruce/wiki)
- [Bruce Discord](https://discord.gg/WJ9XF9czVT)

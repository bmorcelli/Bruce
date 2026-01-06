# Bruce Flasher Guide

This guide explains how to use the **Bruce Flasher** tool, a simple way to build and flash Bruce Firmware to your ESP32 device.

## Prerequisites

1.  **Python 3.7+**: Ensure Python is installed and added to your system PATH.
2.  **USB Drivers**: Make sure you have the correct USB drivers for your device (CH34x, CP210x, etc.).
3.  **Dependencies**: Install the required Python packages:
    ```bash
    pip install -r requirements.txt
    ```

## How to Use

1.  **Open a Terminal**: Navigate to the `firmware` folder where the script is located.

    ```bash
    cd firmware
    ```

2.  **Run the Flasher**:

    ```bash
    python bruce_flasher.py
    ```

3.  **Select Your Device**:

    - You will see a list of supported devices (M5Stack, Lilygo, CYD, etc.).
    - Type the **number** corresponding to your device and press `Enter`.

4.  **Select Action**:

    - Choose **1. Build & Flash** to compile the code and upload it to your device.
    - _Note: The first run may take a few minutes as it downloads necessary frameworks._

5.  **Select Port** (if prompted):
    - Select your device's COM port from the list, or choose **0** for auto-detection.

## Troubleshooting

- **"Device not in bootloader mode"**:
  - Hold the **BOOT** button (sometimes labeled as '0') on your device.
  - Press the **RESET** button once.
  - Release the **BOOT** button.
  - Try flashing again.
- **"MissingPackageManifestError"**:
  - Run the tool again and select option **4. Clean / Fix** (if available) or simply retry "Build & Flash", as the tool now includes auto-cleanup steps.
- **Black Screen after flashing**:
  - Press the **RESET** button on your device.

---

_Happy Hacking!_

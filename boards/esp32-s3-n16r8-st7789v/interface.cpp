#include "CYD28_TouchscreenR.h"
#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "core/utils.h"
#include <Arduino.h>
#include <interface.h>

CYD28_TouchR touch(320, 240);

void _setup_gpio() {
    bruceConfigPins.irTx = -1;
    bruceConfigPins.irRx = -1;
    bruceConfigPins.rotation = 1;
    bruceConfigPins.badusb_bus = {(gpio_num_t)-1, (gpio_num_t)-1}; // rx, tx (none available)
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)21, (gpio_num_t)38, (gpio_num_t)18, (gpio_num_t)-1
    }; // sck,miso,mosi,cs
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI)
    bruceConfigPins.outer_bus = {(gpio_num_t)21, (gpio_num_t)38, (gpio_num_t)18, (gpio_num_t)15};
    bruceConfigPins.PN532_bus = {(gpio_num_t)21, (gpio_num_t)38, (gpio_num_t)18, (gpio_num_t)15};

    pinMode(XPT2046_SPI_CONFIG_CS_GPIO_NUM, OUTPUT);
    digitalWrite(XPT2046_SPI_CONFIG_CS_GPIO_NUM, HIGH);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
}

void _post_setup_gpio() {
    // Use software SPI (GPIO bit-banging) for touch to avoid conflicts with AUX_SPI
    // CYD28_TouchR::begin() with no arguments uses software SPI mode on the defined GPIO pins
    if (!touch.begin()) { Serial.println("Touchscreen initialization failed!"); }
}

int getBattery() { return 0; }

bool isCharging() { return false; }

void _setBrightness(uint8_t brightval) { analogWrite(TFT_BL, (brightval * 255) / 100); }

void InputHandler(void) {
    static unsigned long lastTouch = 0;
    if (millis() - lastTouch < 200 && !LongPress) return;
    if (touch.touched()) {
        auto point = touch.getPointScaled();
        lastTouch = millis();
        if (bruceConfigPins.rotation == 1) point.y = (tftHeight + 20) - point.y;
        if (bruceConfigPins.rotation == 3) point.x = tftWidth - point.x;
        if (bruceConfigPins.rotation == 0) {
            int temporary = point.x;
            point.x = point.y;
            point.y = temporary;
        }
        if (bruceConfigPins.rotation == 2) {
            int temporary = point.x;
            point.x = tftWidth - point.y;
            point.y = (tftHeight + 20) - temporary;
        }
        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;
        touchPoint.x = point.x;
        touchPoint.y = point.y;
        touchPoint.pressed = true;
        touchHeatMap(touchPoint);
        return;
    }
    checkPowerSaveTime();
    PrevPress = false;
    NextPress = false;
    SelPress = false;
    AnyKeyPress = false;
    EscPress = false;
}

void powerOff() {}

void checkReboot() {}

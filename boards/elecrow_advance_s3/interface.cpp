#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include "core/utils.h"
#include "hal/device.h"
#include "hal/inputs/touch.h"
#include <Arduino.h>
#include <Wire.h>
#include <interface.h>

#define BOARD_TOUCH_INT 47
#define GT911_SLAVE_ADDRESS_L 0x5D
#define TFT_BRIGHT_Bits 8
#define TFT_BRIGHT_FREQ 5000

// =============================================================================
//  CrowPanel Advance 3.5" (ESP32-S3) interface
//  - Display: ILI9488 over SPI (handled by TFT_eSPI)
//  - Touch:   GT911 capacitive, directly on I2C (SDA=15, SCL=16, INT=47),
//             no IO expander, no dedicated RST line.
//  - Backlight: direct PWM on GPIO38.
// =============================================================================

static DeviceTouch touchCfg() {
    DeviceTouch cfg;
    cfg.pin_sda = 15;
    cfg.pin_scl = 16;
    cfg.pin_irq = BOARD_TOUCH_INT;
    cfg.i2c_bus = &Wire1;
    // No RST line on this board (pin_rst stays -1)
    // rotation:        0      1      2      3
    const bool swapXY[4] = {false, true, false, true};
    const bool mirrorX[4] = {false, false, true, true};
    const bool mirrorY[4] = {false, true, true, false};
    for (int i = 0; i < 4; i++) {
        cfg.SwapXY[i] = swapXY[i];
        cfg.MirrorX[i] = mirrorX[i];
        cfg.MirrorY[i] = mirrorY[i];
    }
    return cfg;
}

/***************************************************************************************
** Function name: _setup_gpio()
***************************************************************************************/
void _setup_gpio() {
    bruceConfigPins.sys_i2c = {(gpio_num_t)15, (gpio_num_t)16}; // sda, scl
    bruceConfigPins.rotation = 1;
    bruceConfigPins.uart_bus = {(gpio_num_t)44, (gpio_num_t)43};   // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)44, (gpio_num_t)43};    // rx, tx
    bruceConfigPins.badusb_bus = {(gpio_num_t)18, (gpio_num_t)17}; // rx, tx
    bruceConfigPins.SDCARD_bus = {
        (gpio_num_t)5, (gpio_num_t)4, (gpio_num_t)6, (gpio_num_t)7
    }; // sck,miso,mosi,cs
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI)
    bruceConfigPins.outer_bus = {(gpio_num_t)5, (gpio_num_t)4, (gpio_num_t)6, (gpio_num_t)7};
    bruceConfigPins.PN532_bus = {(gpio_num_t)5, (gpio_num_t)4, (gpio_num_t)6, (gpio_num_t)7};

    bruceConfig.colorInverted = 0;

    // Bring up the I2C bus the GT911 lives on.
    setSysI2CBus(&Wire1);
    Wire1.begin(bruceConfigPins.sys_i2c.sda, bruceConfigPins.sys_i2c.scl);

    // GT911 power-on reset sequence. No RST line on this board, so hold INT low
    // briefly to keep address 0x5D, then release it as an input. This board-specific dance
    // (address-select via INT alone, no RST) isn't the generic hal_touch_init int-sync path
    // (that one needs a real RST line), so it stays here before hal_touch_init() takes over.
    pinMode(BOARD_TOUCH_INT, OUTPUT);
    digitalWrite(BOARD_TOUCH_INT, LOW);
    delay(10);
    pinMode(BOARD_TOUCH_INT, INPUT);
    delay(50);

    if (!hal_touch_init(touchCfg(), GT911_SLAVE_ADDRESS_L)) {
        Serial.println("Failed to find GT911 touch - check wiring!");
    } else {
        Serial.println("GT911 touch started");
    }
}

/***************************************************************************************
** Function name: _post_setup_gpio()
***************************************************************************************/
void _post_setup_gpio() {
    pinMode(TFT_BL, OUTPUT);
    ledcAttach(TFT_BL, TFT_BRIGHT_FREQ, TFT_BRIGHT_Bits);
    ledcWrite(TFT_BL, 255);
}

/***************************************************************************************
** Function name: getBattery()
***************************************************************************************/
int getBattery() { return 100; }

/*********************************************************************
** Function: _setBrightness
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    int dutyCycle;
    if (brightval == 100) dutyCycle = 255;
    else if (brightval == 75) dutyCycle = 130;
    else if (brightval == 50) dutyCycle = 70;
    else if (brightval == 25) dutyCycle = 20;
    else if (brightval == 0) dutyCycle = 0;
    else dutyCycle = ((brightval * 255) / 100);
    ledcWrite(TFT_BL, dutyCycle);
}

/*********************************************************************
** Function: InputHandler (GT911 capacitive)
**********************************************************************/
void InputHandler(void) {
    static long d_tmp = 0;
    if (millis() - d_tmp > 200 || LongPress) {
        BruceTouchPoint t;
        if (hal_touch_read(touchCfg(), t)) {
            d_tmp = millis();
            hal_touch_apply(t);
        }
    }
}

/*********************************************************************
** Function: powerOff
**********************************************************************/
void powerOff() {}

/*********************************************************************
** Function: checkReboot
**********************************************************************/
void checkReboot() {}

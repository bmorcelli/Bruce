#include "core/powerSave.h"
#include "core/utils.h"
#include <CYD28_TouchscreenR.h>
#include <interface.h>

CYD28_TouchR touch(320, 240);

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    bruceConfigPins.i2c_bus = {(gpio_num_t)22, (gpio_num_t)21}; // sda, scl (Grove)
    bruceConfigPins.rfTx = 22;
    bruceConfigPins.rfRx = 21;
    bruceConfigPins.irTx = -1;
    bruceConfigPins.irRx = 21;
    bruceConfigPins.rotation = 1;
    bruceConfigPins.uart_bus = {(gpio_num_t)3, (gpio_num_t)1}; // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)3, (gpio_num_t)1};  // rx, tx
    bruceConfigPins.badusb_bus = {(gpio_num_t)21, (gpio_num_t)22}; // rx, tx (Grove)
    bruceConfigPins.SDCARD_bus = {
        (gpio_num_t)-1, (gpio_num_t)-1, (gpio_num_t)-1, (gpio_num_t)-1
    }; // sck,miso,mosi,cs (no SD card slot)
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI)
    bruceConfigPins.outer_bus = {(gpio_num_t)25, (gpio_num_t)39, (gpio_num_t)32, (gpio_num_t)33};
    bruceConfigPins.PN532_bus = {(gpio_num_t)25, (gpio_num_t)39, (gpio_num_t)32, (gpio_num_t)33};

    bruceConfig.colorInverted = 0;
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
}

/***************************************************************************************
** Function name: _post_setup_gpio()
** Description:   second stage gpio setup to make touch work
***************************************************************************************/
void _post_setup_gpio() {
    if (!touch.begin(&tft.getSPIinstance())) {
        Serial.println("Touch IC not Started");
        log_i("Touch IC not Started");
    } else Serial.println("Touch IC Started");
}

/***************************************************************************************
** Function name: getBattery()
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
int getBattery() { return 0; }

/*********************************************************************
** Function: setBrightness
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    if (brightval > 5) digitalWrite(TFT_BL, HIGH);
    else digitalWrite(TFT_BL, LOW);
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = millis();
    if (millis() - tm > 300 || LongPress) {
        if (touch.touched()) {
            auto t = touch.getPointScaled();
            t = touch.getPointScaled();
            tm = millis();
            if (bruceConfigPins.rotation == 3) {
                t.y = (tftHeight + 20) - t.y;
                t.x = tftWidth - t.x;
            }
            if (bruceConfigPins.rotation == 0) {
                int tmp = t.x;
                t.x = tftWidth - t.y;
                t.y = tmp;
            }
            if (bruceConfigPins.rotation == 2) {
                int tmp = t.x;
                t.x = t.y;
                t.y = (tftHeight + 20) - tmp;
            }

            if (!wakeUpScreen()) AnyKeyPress = true;
            else return;

            touchPoint.x = t.x;
            touchPoint.y = t.y;
            touchPoint.pressed = true;
            touchHeatMap(touchPoint);
        } else touchPoint.pressed = false;
    }
}

/*********************************************************************
** Function: powerOff
** Turns off the device (or try to)
**********************************************************************/
void powerOff() {}

/*********************************************************************
** Function: checkReboot
** Btn logic to turn off the device
**********************************************************************/
void checkReboot() {}

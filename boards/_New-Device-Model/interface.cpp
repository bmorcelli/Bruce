#include "core/powerSave.h"
#include <interface.h>

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    // -- Audio --
    // Defaults only: brucePins.conf overrides them on the next boot, and the user can re-map them
    // from Config > Set Device pins without rebuilding.
    //
    // I2S speaker, needs -DHAS_SPEAKER=1 in the .ini. {bclk, ws(LRCLK), dout, mclk}; leave mclk at
    // GPIO_NUM_NC when the codec derives MCLK from BCLK.
    // bruceConfigPins.speaker_bus = {(gpio_num_t)41, (gpio_num_t)43, (gpio_num_t)42, GPIO_NUM_NC};
    //
    // Microphone, needs -DHAS_MICROPHONE=1 in the .ini. {clk, data, ws, type}:
    //   MIC_TYPE_PDM         -- PDM mic (e.g. an SPM1423 on its default wiring): clk + data, no ws
    //   MIC_TYPE_I2S_MSB     -- MSB/left-justified I2S (e.g. an SPM1423 behind an ES8311)
    //   MIC_TYPE_I2S_PHILIPS -- standard (Philips) I2S, e.g. an INMP441
    // Both I2S types take clk as BCLK.
    // bruceConfigPins.mic_bus = {(gpio_num_t)43, (gpio_num_t)46, GPIO_NUM_NC, MIC_TYPE_PDM};
    //
    // Piezo buzzer. Always compiled in, no feature gate -- it just stays silent while the pin is
    // -1. A board with HAS_SPEAKER uses the I2S speaker instead and ignores this.
    // bruceConfigPins.buzzer = 11;
}

/***************************************************************************************
** Function name: _post_setup_gpio()
** Location: main.cpp
** Description:   second stage gpio setup to make a few functions work
***************************************************************************************/
void _post_setup_gpio() {}

/***************************************************************************************
** Function name: getBattery()
** location: display.cpp
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
int getBattery() { return 0; }

/*********************************************************************
** Function: setBrightness
** location: settings.cpp
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    checkPowerSaveTime();
    PrevPress = false;
    NextPress = false;
    SelPress = false;
    AnyKeyPress = false;
    EscPress = false;

    if (false /*Conditions fot all inputs*/) {
        if (!wakeUpScreen()) AnyKeyPress = true;
        else goto END;
    }
    if (false /*Conditions for previous btn*/) { PrevPress = true; }
    if (false /*Conditions for Next btn*/) { NextPress = true; }
    if (false /*Conditions for Esc btn*/) { EscPress = true; }
    if (false /*Conditions for Select btn*/) { SelPress = true; }
END:
    if (AnyKeyPress) {
        long tmp = millis();
        while ((millis() - tmp) < 200 && false /*Conditions fot all inputs*/);
    }
}

/*********************************************************************
** Function: keyboard
** location: mykeyboard.cpp
** Starts keyboard to type data
**********************************************************************/
String keyboard(String mytext, int maxSize, String msg) {}

/*********************************************************************
** Function: powerOff
** location: mykeyboard.cpp
** Turns off the device (or try to)
**********************************************************************/
void powerOff() {}

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to turn off the device (name is odd btw)
**********************************************************************/
void checkReboot() {}

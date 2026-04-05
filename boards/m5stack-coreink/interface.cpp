#include "core/powerSave.h"
#include <M5Unified.h>
#include <interface.h>

namespace {
constexpr uint8_t ROCKER_LEFT_PIN = 39;
constexpr uint8_t ROCKER_CENTER_PIN = 38;
constexpr uint8_t ROCKER_RIGHT_PIN = 37;
constexpr bool ROCKER_ACTIVE_LOW = true;

bool readRockerPin(uint8_t pin) {
    int level = digitalRead(pin);
    return ROCKER_ACTIVE_LOW ? (level == LOW) : (level == HIGH);
}
} // namespace

/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    M5.begin();
    pinMode(ROCKER_LEFT_PIN, INPUT);
    pinMode(ROCKER_CENTER_PIN, INPUT);
    pinMode(ROCKER_RIGHT_PIN, INPUT);
}

/***************************************************************************************
** Function name: getBattery()
** location: display.cpp
** Description:   Delivers the battery value from 1-100
***************************************************************************************/
int getBattery() {
    int percent = M5.Power.getBatteryLevel();
    return (percent < 0) ? 1 : (percent >= 100) ? 100 : percent;
}

/*********************************************************************
** Function: setBrightness
** location: settings.cpp
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) { (void)brightval; }

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;

    M5.update();

    // Rocker button: left/right = next/prev (inverted for intuitive nav), center = select/back
    static bool lastLeft = false;
    static bool lastRight = false;
    static bool lastCenter = false;
    static unsigned long centerPressTime = 0;

    bool leftNow = readRockerPin(ROCKER_LEFT_PIN);
    bool rightNow = readRockerPin(ROCKER_RIGHT_PIN);
    bool centerNow = readRockerPin(ROCKER_CENTER_PIN);

    bool leftPressed = leftNow && !lastLeft;
    bool rightPressed = rightNow && !lastRight;
    bool centerPressed = centerNow && !lastCenter;

    // Track center button hold time for long press detection
    if (centerNow && !lastCenter) { centerPressTime = millis(); }
    bool centerLongPress = centerNow && (millis() - centerPressTime > 800);

    lastLeft = leftNow;
    lastRight = rightNow;
    lastCenter = centerNow;

    if (centerPressed) {
        leftPressed = false;
        rightPressed = false;
    }

    bool any = leftPressed || rightPressed || centerPressed;
    if (any) tm = millis();
    if (any && wakeUpScreen()) return;

    AnyKeyPress = any;
    UpPress = false;
    DownPress = false;
    SelPress = centerPressed && !centerLongPress; // Select on short press
    NextPress = leftPressed;                      // Inverted: left = next (right direction)
    PrevPress = rightPressed;                     // Inverted: right = prev (left direction)
    EscPress = centerLongPress;                   // Back on long press (800ms+)
    LongPress = false;
}

/*********************************************************************
** Function: powerOff
** location: mykeyboard.cpp
** Turns off the device (or try to)
**********************************************************************/
void powerOff() { M5.Power.powerOff(); }
void goToDeepSleep() { M5.Power.deepSleep(); }

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to turn off the device (name is odd btw)
**********************************************************************/
void checkReboot() {}

/***************************************************************************************
** Function name: isCharging()
** Description:   Determines if the device is charging
***************************************************************************************/
bool isCharging() {
    if (M5.Power.getBatteryCurrent() > 0 || M5.Power.getBatteryCurrent()) return true;
    else return false;
}
